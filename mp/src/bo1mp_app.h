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
};
