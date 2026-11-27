/**
 * Virtual controllers driven over a named pipe (see script_input_driver.h).
 */

#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>

#include <rex/input/flags.h>
#include <rex/input/script/script_input_driver.h>
#include <rex/logging.h>
#include <rex/ui/virtual_key.h>

#if REX_PLATFORM_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rex::input::script {

namespace {

constexpr uint64_t kPadIdBase = 0x53435200;  // "SCR"

std::mutex& CommandMutex() {
  static std::mutex m;
  return m;
}
std::map<std::string, CommandHandler>& Commands() {
  static std::map<std::string, CommandHandler> c;
  return c;
}

// XINPUT button bit -> virtual key, in bit order (as the SDL driver reports them).
constexpr rex::ui::VirtualKey kVk[16] = {
    rex::ui::VirtualKey::kXInputPadDpadUp,      rex::ui::VirtualKey::kXInputPadDpadDown,
    rex::ui::VirtualKey::kXInputPadDpadLeft,    rex::ui::VirtualKey::kXInputPadDpadRight,
    rex::ui::VirtualKey::kXInputPadStart,       rex::ui::VirtualKey::kXInputPadBack,
    rex::ui::VirtualKey::kXInputPadLThumbPress, rex::ui::VirtualKey::kXInputPadRThumbPress,
    rex::ui::VirtualKey::kXInputPadLShoulder,   rex::ui::VirtualKey::kXInputPadRShoulder,
    rex::ui::VirtualKey::kNone,                 rex::ui::VirtualKey::kNone,
    rex::ui::VirtualKey::kXInputPadA,           rex::ui::VirtualKey::kXInputPadB,
    rex::ui::VirtualKey::kXInputPadX,           rex::ui::VirtualKey::kXInputPadY,
};

}  // namespace

void RegisterCommand(const std::string& name, CommandHandler handler) {
  std::lock_guard lock(CommandMutex());
  Commands()[name] = std::move(handler);
}

ScriptInputDriver::ScriptInputDriver(rex::ui::Window* window, size_t window_z_order,
                                     uint32_t pad_count, std::string pipe_name)
    : InputDriver(window, window_z_order),
      pad_count_(pad_count > 4 ? 4 : pad_count),
      pipe_name_(std::move(pipe_name)) {}

ScriptInputDriver::~ScriptInputDriver() {
  stop_ = true;
#if REX_PLATFORM_WIN32
  if (pipe_) {
    CancelIoEx(static_cast<HANDLE>(pipe_), nullptr);
  }
  // Wake a ConnectNamedPipe that is still waiting for a client.
  std::string path = "\\\\.\\pipe\\" + pipe_name_;
  HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                         nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    CloseHandle(h);
  }
#endif
  if (thread_.joinable()) {
    thread_.join();
  }
}

X_STATUS ScriptInputDriver::Setup() {
  REXLOG_INFO("Script input: {} virtual pad(s), control pipe \\\\.\\pipe\\{}", pad_count_,
              pipe_name_);
  thread_ = std::thread([this] { PipeThread(); });
  return X_STATUS_SUCCESS;
}

int ScriptInputDriver::PadIndex(DeviceId id) const {
  const uint64_t v = static_cast<uint64_t>(id);
  if (v < kPadIdBase || v >= kPadIdBase + pad_count_) {
    return -1;
  }
  return int(v - kPadIdBase);
}

void ScriptInputDriver::EnumerateDevices(std::vector<DeviceInfo>& out) {
  for (uint32_t i = 0; i < pad_count_; i++) {
    DeviceInfo info;
    info.id = static_cast<DeviceId>(kPadIdBase + i);
    info.name = "Script pad " + std::to_string(i + 1);
    info.guid = "script-" + std::to_string(i);
    info.synthetic = false;  // a real player, so it takes player slots 1..N
    out.push_back(info);
  }
}

X_RESULT ScriptInputDriver::GetDeviceState(DeviceId id, X_INPUT_STATE* out_state) {
  int i = PadIndex(id);
  if (i < 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  if (out_state) {
    std::lock_guard lock(mutex_);
    const Pad& p = pads_[i];
    std::memset(out_state, 0, sizeof(*out_state));
    out_state->packet_number = p.packet;
    out_state->gamepad.buttons = p.buttons;
    out_state->gamepad.left_trigger = p.lt;
    out_state->gamepad.right_trigger = p.rt;
    out_state->gamepad.thumb_lx = p.lx;
    out_state->gamepad.thumb_ly = p.ly;
    out_state->gamepad.thumb_rx = p.rx;
    out_state->gamepad.thumb_ry = p.ry;
  }
  return X_ERROR_SUCCESS;
}

X_RESULT ScriptInputDriver::GetDeviceCapabilities(DeviceId id, uint32_t flags,
                                                  X_INPUT_CAPABILITIES* out_caps) {
  if (PadIndex(id) < 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  if (out_caps) {
    std::memset(out_caps, 0, sizeof(*out_caps));
    out_caps->type = 0x01;
    out_caps->sub_type = 0x01;
    out_caps->gamepad.buttons = 0xFFFF;
    out_caps->gamepad.left_trigger = 0xFF;
    out_caps->gamepad.right_trigger = 0xFF;
    out_caps->gamepad.thumb_lx = static_cast<int16_t>(0x7FFF);
    out_caps->gamepad.thumb_ly = static_cast<int16_t>(0x7FFF);
    out_caps->gamepad.thumb_rx = static_cast<int16_t>(0x7FFF);
    out_caps->gamepad.thumb_ry = static_cast<int16_t>(0x7FFF);
    out_caps->vibration.left_motor_speed = 0xFFFF;
    out_caps->vibration.right_motor_speed = 0xFFFF;
  }
  return X_ERROR_SUCCESS;
}

X_RESULT ScriptInputDriver::SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION*) {
  return PadIndex(id) < 0 ? X_ERROR_DEVICE_NOT_CONNECTED : X_ERROR_SUCCESS;
}

X_RESULT ScriptInputDriver::GetDeviceKeystroke(DeviceId id, uint32_t,
                                               X_INPUT_KEYSTROKE* out_keystroke) {
  int i = PadIndex(id);
  if (i < 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  if (!out_keystroke) {
    return X_ERROR_BAD_ARGUMENTS;
  }
  std::lock_guard lock(mutex_);
  Pad& p = pads_[i];
  const uint16_t changed = p.buttons ^ p.keystroke_buttons;
  // Releases first, then presses (as XInput does).
  for (int pass = 0; pass < 2; pass++) {
    for (int b = 0; b < 16; b++) {
      const uint16_t bit = uint16_t(1u << b);
      if (!(changed & bit) || kVk[b] == rex::ui::VirtualKey::kNone) {
        continue;
      }
      const bool down = (p.buttons & bit) != 0;
      if ((pass == 0) == down) {
        continue;
      }
      std::memset(out_keystroke, 0, sizeof(*out_keystroke));
      out_keystroke->virtual_key = uint16_t(kVk[b]);
      out_keystroke->flags = down ? X_INPUT_KEYSTROKE_KEYDOWN : X_INPUT_KEYSTROKE_KEYUP;
      p.keystroke_buttons ^= bit;
      return X_ERROR_SUCCESS;
    }
  }
  return X_ERROR_EMPTY;
}

std::string ScriptInputDriver::HandleLine(const std::string& line) {
  std::istringstream in(line);
  std::string cmd;
  in >> cmd;
  if (cmd.empty()) {
    return "error empty";
  }
  if (cmd == "ping") {
    return "pong";
  }
  if (cmd == "pad") {
    int i = -1;
    std::string buttons;
    int lt = 0, rt = 0, lx = 0, ly = 0, rx = 0, ry = 0;
    in >> i >> buttons >> lt >> rt >> lx >> ly >> rx >> ry;
    if (i < 0 || i >= int(pad_count_) || buttons.empty()) {
      return "error usage: pad <i> <buttons hex> <lt> <rt> <lx> <ly> <rx> <ry>";
    }
    auto clamp16 = [](int v) { return int16_t(v < -32768 ? -32768 : v > 32767 ? 32767 : v); };
    auto clamp8 = [](int v) { return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v); };
    std::lock_guard lock(mutex_);
    Pad& p = pads_[i];
    p.buttons = uint16_t(std::strtoul(buttons.c_str(), nullptr, 16));
    p.lt = clamp8(lt);
    p.rt = clamp8(rt);
    p.lx = clamp16(lx);
    p.ly = clamp16(ly);
    p.rx = clamp16(rx);
    p.ry = clamp16(ry);
    p.packet++;
    return "ok";
  }
  CommandHandler handler;
  {
    std::lock_guard lock(CommandMutex());
    auto it = Commands().find(cmd);
    if (it != Commands().end()) {
      handler = it->second;
    }
  }
  if (!handler) {
    return "error unknown command " + cmd;
  }
  std::string rest;
  std::getline(in, rest);
  if (!rest.empty() && rest[0] == ' ') {
    rest.erase(0, 1);
  }
  return handler(rest);
}

void ScriptInputDriver::PipeThread() {
#if REX_PLATFORM_WIN32
  const std::string path = "\\\\.\\pipe\\" + pipe_name_;
  while (!stop_) {
    HANDLE pipe = CreateNamedPipeA(path.c_str(), PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096,
                                   0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      REXLOG_ERROR("Script input: cannot create {} (error {})", path, GetLastError());
      return;
    }
    pipe_ = pipe;
    BOOL connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
    if (connected && !stop_) {
      std::string buffer;
      char chunk[1024];
      DWORD got = 0;
      while (!stop_ && ReadFile(pipe, chunk, sizeof(chunk), &got, nullptr) && got) {
        buffer.append(chunk, got);
        size_t nl;
        while ((nl = buffer.find('\n')) != std::string::npos) {
          std::string line = buffer.substr(0, nl);
          buffer.erase(0, nl + 1);
          if (!line.empty() && line.back() == '\r') {
            line.pop_back();
          }
          std::string answer = HandleLine(line) + "\n";
          DWORD written = 0;
          WriteFile(pipe, answer.data(), DWORD(answer.size()), &written, nullptr);
        }
      }
    }
    pipe_ = nullptr;
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
  }
#endif
}

}  // namespace rex::input::script
