// bo1mp - Call of Duty: Black Ops (Xbox 360, TU11 default_mp.xex), Multiplayer.
//
// Portable layout: everything sits next to bo1mp.exe unless bo1mp.toml says otherwise
// (game_data_root / update_data_root / user_data_root / cache_root):
//   game\    the disc's files, with TU11's default.xexp beside default.xex
//   update\  the rest of Title Update 11 (patch fastfiles)
//   user\    saves, cache and DLC (user\0000000000000000\41560855\...)

#pragma once

#include <filesystem>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/rex_app.h>

#include "bo1_online.h"

class Bo1mpApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Bo1mpApp>(new Bo1mpApp(ctx, "bo1mp", PPCImageConfig));
  }

  // Split screen: one offline profile per controller port (the SDK default is 1, so players 2-4 could not sign
  // in). bo1mp.toml's offline_user_count still wins when it is set there. As in bo2-360-nx.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    (void)config;
    if (rex::cvar::GetFlagSource("offline_user_count") == rex::cvar::Source::kDefault) {
      rex::cvar::SetFlagByName("offline_user_count", "4");
    }
    // Zombies <-> Multiplayer from the game's menus: the 360 relaunches default.xex / default_mp.xex; on PC each is
    // its own exe next to this one (SDK XamLoaderLaunchTitle). bo1mp.toml's launch_title_map still wins.
    if (rex::cvar::GetFlagSource("launch_title_map") == rex::cvar::Source::kDefault) {
      rex::cvar::SetFlagByName("launch_title_map", "default_mp.xex=bo1mp.exe;default.xex=bo1zm.exe");
    }
  }

  // The SDK loads game:\\default.xex (Zombies and the campaign); Multiplayer is default_mp.xex.
  void OnLoadXexImage(std::string& xex_image) override { xex_image = "game:\\default_mp.xex"; }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    namespace fs = std::filesystem;
    const fs::path exe = rex::filesystem::GetExecutableFolder();
    if (paths.game_data_root.empty()) paths.game_data_root = exe / "game";
    if (paths.update_data_root.empty() && fs::is_directory(exe / "update"))
      paths.update_data_root = exe / "update";
    // The SDK's default user folder is <home>/bo1zm; keep it portable instead.
    const fs::path default_user = rex::filesystem::GetUserFolder() / "bo1mp";
    if (paths.user_data_root.empty() || paths.user_data_root == default_user) {
      paths.user_data_root = exe / "user";
      if (paths.cache_root == default_user / "cache") paths.cache_root = exe / "user" / "cache";
    }
  }
  // Online co-op (bo1_online in the toml): start the connection, show the status window for a short time.
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override { imgui_drawer_for_online_ = drawer; }
  void OnPostSetup() override {
    if (bo1::StartOnlineFromSettings()) bo1::ShowOnlineStatus(imgui_drawer_for_online_);
  }
  void OnShutdown() override { bo1::ShutdownOnline(); }

 private:
  rex::ui::ImGuiDrawer* imgui_drawer_for_online_ = nullptr;
};
