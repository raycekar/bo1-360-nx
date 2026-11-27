// Script text patches. BO1 keeps its GSC/CSC scripts as plain text and compiles them at load:
// Scr_AddSourceBuffer (TU11 sub_824267F0: r3 script instance, r4 name, r5 file name, r7 archive flag) returns a
// heap copy of the source (from the zone's rawfile, or the saved archive), and the compiler reads it after this
// call. The hook calls the original, then edits that buffer in place. Patches keep the length the same (padded
// with spaces), so line numbers and offsets do not move.
//
// Patches (each behind a setting):
//   bo1zm_splitscreen_fog = false: maps/_utility::set_splitscreen_fog() returns at once, so split screen keeps the
//   map's own fog and draw distance (the 360 pulled the fog in to 350..2986 units and culled at 10000 to save GPU).

#include <cstring>
#include <string_view>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(bo1zm_splitscreen_fog, true, "BO1",
                    "Zombies split screen: keep the Xbox 360's closer fog and cull distance. false = full view distance.");

namespace {

constexpr size_t kMaxScript = 8u << 20;

std::string_view GuestString(uint8_t* base, uint32_t ptr) {
  if (!ptr) return {};
  const char* s = reinterpret_cast<const char*>(base + ptr);
  return std::string_view(s, strnlen(s, kMaxScript));
}

// Replace `from` with `to` (same length or shorter, padded with spaces) at the first match at or after `start`,
// within `window` bytes of it. Returns true when patched.
bool PatchAfter(char* text, size_t len, size_t start, size_t window, std::string_view from, std::string_view to) {
  if (to.size() > from.size() || start >= len) return false;
  std::string_view hay(text + start, std::min(window, len - start));
  const size_t at = hay.find(from);
  if (at == std::string_view::npos) return false;
  char* p = text + start + at;
  std::memcpy(p, to.data(), to.size());
  std::memset(p + to.size(), ' ', from.size() - to.size());
  return true;
}

void PatchSplitscreenFog(char* text, size_t len) {
  std::string_view all(text, len);
  const size_t def = all.find("\nset_splitscreen_fog(");
  if (def == std::string_view::npos) return;
  if (PatchAfter(text, len, def, 400, "!IsSplitScreen()", "1"))
    REXLOG_INFO("bo1zm: split-screen fog off (maps/_utility set_splitscreen_fog returns at once)");
  else
    REXLOG_WARN("bo1zm: set_splitscreen_fog found but its guard was not; split-screen fog stays on");
}

}  // namespace

REX_EXTERN(__imp__sub_824267F0);
REX_HOOK_RAW(sub_824267F0) {  // Scr_AddSourceBuffer
  const std::string name(GuestString(base, ctx.r4.u32));
  __imp__sub_824267F0(ctx, base);
  const uint32_t buf = ctx.r3.u32;
  if (!buf) return;
  char* text = reinterpret_cast<char*>(base + buf);
  const size_t len = strnlen(text, kMaxScript);

  if (!REXCVAR_GET(bo1zm_splitscreen_fog) && (name == "maps/_utility" || name == "maps\\_utility"))
    PatchSplitscreenFog(text, len);
}
