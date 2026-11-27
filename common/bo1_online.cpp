// Online co-op, first version: a setting, no menu (docs/online-plan.md).
//
//   bo1_online = "host"        this PC hosts; the status window shows the join code to send to friends
//   bo1_online = "XXXX-XXXX"   this PC joins the host of that code
//
// The SDK prepares the session in the background (UPnP, STUN, hole punching; sdk/src/system/online_link.cpp). After
// that, the players use the game's own System Link menu: the host opens a System Link lobby, the joiners search.
// A small status window shows the progress and closes itself 30 s after the session is ready. It is not kept open,
// because any ImGui dialog makes the presenter paint from the UI thread (see rex_app.cpp, LaunchModule).

#include "bo1_online.h"

#include <chrono>
#include <string>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/join_code.h>
#include <rex/system/online_link.h>
#include <rex/ui/imgui_dialog.h>

REXCVAR_DEFINE_STRING(bo1_online, "", "BO1",
                      "Online play over System Link: empty = off, \"host\", or the host's join code XXXX-XXXX");

namespace bo1 {
namespace {

namespace online = rex::system::online;
namespace join_code = rex::system::join_code;

std::string Ip(uint32_t ip) {
  return std::to_string(ip >> 24) + "." + std::to_string((ip >> 16) & 255) + "." + std::to_string((ip >> 8) & 255) +
         "." + std::to_string(ip & 255);
}

const char* StateText(const online::Report& r) {
  switch (r.state) {
    case online::State::kIdle: return "off";
    case online::State::kPreparing: return "preparing";
    case online::State::kReady: return "ready";
    case online::State::kFailed: break;
  }
  switch (r.failure) {
    case online::Failure::kNoNetwork: return "failed: no network";
    case online::Failure::kNoPublicAddress: return "failed: no public address (check the internet connection)";
    case online::Failure::kSameNetwork:
      return "failed: the code is this network's own address (on one LAN, use System Link without a code)";
    default: return "failed";
  }
}

void LogReport(const online::Report& r) {
  REXLOG_INFO("bo1 online: {} ({}), lan {}, public {}, router {}, ports {}/{} from {}, stun {}, double NAT {}, "
              "relay peers {}",
              StateText(r), r.role == online::Role::kHost ? "host" : "join", Ip(r.lan_ip), Ip(r.public_ip),
              r.router_found ? Ip(r.router_ip) : std::string("none"), r.ports_mapped, r.port_count, r.first_port,
              r.stun_ok ? "ok" : "no", r.double_nat ? "yes" : "no", r.relay.peers);
  if (r.state == online::State::kReady && r.role == online::Role::kHost) {
    REXLOG_INFO("bo1 online: JOIN CODE {} (send it to your friends; open a System Link lobby)",
                join_code::Make(r.public_ip));
  }
}

class StatusDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit StatusDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    (void)io;
    const online::Report r = online::Current();
    if (r.version != last_version_) {
      last_version_ = r.version;
      LogReport(r);
    }
    const auto now = std::chrono::steady_clock::now();
    if (r.state == online::State::kReady || r.state == online::State::kFailed) {
      if (done_at_ == std::chrono::steady_clock::time_point{}) done_at_ = now;
      if (now - done_at_ > std::chrono::seconds(30)) {
        Close();
        return;
      }
    }
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("Online", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::Text("Online (%s): %s", r.role == online::Role::kHost ? "host" : "join", StateText(r));
    if (r.state == online::State::kReady) {
      if (r.role == online::Role::kHost) {
        ImGui::Text("Join code: %s", join_code::Make(r.public_ip).c_str());
        ImGui::Text("Open a System Link lobby.");
      } else {
        ImGui::Text("Host %s. Search in System Link.", Ip(r.target_ip).c_str());
      }
    }
    ImGui::Text("Router: %s, ports %d/%d", r.router_found ? "UPnP" : "none", r.ports_mapped, r.port_count);
    if (!r.router_found && r.state == online::State::kReady && r.relay.peers == 0) {
      ImGui::Text("If friends cannot join: forward UDP %u-%u to this PC.", r.first_port,
                  unsigned(r.first_port + r.port_count - 1));
    }
    if (r.relay.active) ImGui::Text("Relay: %d broker(s) up, %d peer(s)", r.relay.brokers_up, r.relay.peers);
    ImGui::End();
  }

 private:
  uint64_t last_version_ = ~0ull;
  std::chrono::steady_clock::time_point done_at_{};
};

bool g_started = false;

}  // namespace

bool StartOnlineFromSettings() {
  const std::string value = REXCVAR_GET(bo1_online);
  if (value.empty()) return false;
  if (value == "host" || value == "HOST") {
    REXLOG_INFO("bo1 online: hosting; preparing the connection");
    online::Start(online::Role::kHost, 0);
  } else {
    uint32_t ip = 0;
    const auto result = join_code::Read(value, &ip);
    if (result != join_code::Result::kOk) {
      REXLOG_ERROR("bo1 online: bo1_online = \"{}\": {}", value, join_code::Describe(result));
      return false;
    }
    REXLOG_INFO("bo1 online: joining the host {}; preparing the connection", Ip(ip));
    online::Start(online::Role::kJoin, ip);
  }
  g_started = true;
  return true;
}

void ShowOnlineStatus(rex::ui::ImGuiDrawer* drawer) {
  if (g_started && drawer) new StatusDialog(drawer);  // deletes itself on Close()
}

void ShutdownOnline() {
  if (g_started) online::Shutdown();  // removes the router's port forwarding
}

}  // namespace bo1
