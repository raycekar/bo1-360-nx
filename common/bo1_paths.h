// Where the shared game data sits. A build at the top of the PC folder has game\, update\ and user\ beside its exe.
// A queued test build sits in builds\<name>\ and uses the same data two levels up, so a new test build adds only
// its own exe and DLLs to a sync (tools/package_win.py --queue).
#pragma once

#include <filesystem>
#include <system_error>

namespace bo1 {

inline std::filesystem::path DataBase(const std::filesystem::path& exe_folder) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (fs::is_directory(exe_folder / "game", ec)) return exe_folder;
  const fs::path shared = exe_folder.parent_path().parent_path();
  if (exe_folder.parent_path().filename() == "builds" && fs::is_directory(shared / "game", ec)) return shared;
  return exe_folder;
}

}  // namespace bo1
