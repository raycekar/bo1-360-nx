#pragma once
// Shader / pipeline building progress, shared between the GPU plugin (which builds them) and the app's on-screen
// "BUILDING SHADERS" text. Lives in the runtime (rexcore) so both DLLs see the same counters.

#include <atomic>
#include <cstdint>

namespace rex::graphics {

struct ShaderProgress {
  std::atomic<uint32_t> queued{0};   // pipelines handed to the creation threads (startup and during play)
  std::atomic<uint32_t> created{0};  // of those, created
  std::atomic<bool> startup{false};  // the stored pipelines are being created before the game goes on
  std::atomic<uint32_t> startup_total{0};
  std::atomic<uint32_t> startup_created{0};

  uint32_t pending() const {
    const uint32_t q = queued.load(std::memory_order_relaxed), c = created.load(std::memory_order_relaxed);
    return q > c ? q - c : 0;
  }
};

ShaderProgress& GetShaderProgress();

}  // namespace rex::graphics
