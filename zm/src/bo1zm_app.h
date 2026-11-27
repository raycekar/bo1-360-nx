// bo1zm - Call of Duty: Black Ops (Xbox 360, TU11 default.xex), Zombies.
//
// Portable layout: everything sits next to bo1zm.exe unless bo1zm.toml says otherwise
// (game_data_root / update_data_root / user_data_root / cache_root):
//   game\    the disc's files, with TU11's default.xexp beside default.xex
//   update\  the rest of Title Update 11 (patch fastfiles)
//   user\    saves, cache and DLC (user\0000000000000000\41560855\...)

#pragma once

#include <filesystem>

#include <rex/filesystem.h>
#include <rex/rex_app.h>

class Bo1zmApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Bo1zmApp>(new Bo1zmApp(ctx, "bo1zm", PPCImageConfig));
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    namespace fs = std::filesystem;
    const fs::path exe = rex::filesystem::GetExecutableFolder();
    if (paths.game_data_root.empty()) paths.game_data_root = exe / "game";
    if (paths.update_data_root.empty() && fs::is_directory(exe / "update"))
      paths.update_data_root = exe / "update";
    // The SDK's default user folder is <home>/bo1zm; keep it portable instead.
    const fs::path default_user = rex::filesystem::GetUserFolder() / "bo1zm";
    if (paths.user_data_root.empty() || paths.user_data_root == default_user) {
      paths.user_data_root = exe / "user";
      if (paths.cache_root == default_user / "cache") paths.cache_root = exe / "user" / "cache";
    }
  }
};
