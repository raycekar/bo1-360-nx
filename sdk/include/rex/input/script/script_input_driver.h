#pragma once
/**
 * @file        rex/input/script/script_input_driver.h
 * @brief       Virtual controllers driven over a named pipe, for unattended tests.
 *
 * With input_script_pads = N (1-4) the driver shows N gamepads that count as real pads (they take
 * players 1..N). A test tool connects to \\.\pipe\<input_script_pipe> and sends text lines:
 *
 *   pad <i> <buttons> <lt> <rt> <lx> <ly> <rx> <ry>   set pad i (0-3); buttons as hex XINPUT bits
 *   ping                                              answers "pong"
 *   <name> <args...>                                  a command registered with RegisterCommand
 *
 * Every line gets one answer line ("ok", or the command's own answer, or "error ...").
 */

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <rex/input/input_driver.h>

namespace rex::input::script {

/// A pipe command other subsystems add, for example "shot" (save the next frame) in the GPU code.
/// The handler runs on the pipe thread; its return value is the answer line.
using CommandHandler = std::function<std::string(const std::string& args)>;
void RegisterCommand(const std::string& name, CommandHandler handler);

class ScriptInputDriver final : public InputDriver {
 public:
  ScriptInputDriver(rex::ui::Window* window, size_t window_z_order, uint32_t pad_count,
                    std::string pipe_name);
  ~ScriptInputDriver() override;

  X_STATUS Setup() override;

  void EnumerateDevices(std::vector<DeviceInfo>& out) override;
  X_RESULT GetDeviceState(DeviceId id, X_INPUT_STATE* out_state) override;
  X_RESULT GetDeviceCapabilities(DeviceId id, uint32_t flags,
                                 X_INPUT_CAPABILITIES* out_caps) override;
  X_RESULT SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION* vibration) override;
  X_RESULT GetDeviceKeystroke(DeviceId id, uint32_t flags,
                              X_INPUT_KEYSTROKE* out_keystroke) override;

 private:
  struct Pad {
    uint16_t buttons = 0;
    uint8_t lt = 0, rt = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
    uint32_t packet = 0;
    uint16_t keystroke_buttons = 0;  // buttons already reported through GetDeviceKeystroke
  };

  int PadIndex(DeviceId id) const;
  void PipeThread();
  std::string HandleLine(const std::string& line);

  uint32_t pad_count_;
  std::string pipe_name_;
  std::mutex mutex_;
  std::array<Pad, 4> pads_{};
  std::atomic<bool> stop_{false};
  std::thread thread_;
  void* pipe_ = nullptr;
};

}  // namespace rex::input::script
