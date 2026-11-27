#pragma once
// Counts D3D12 resource/heap creations per call site and logs them every 10 s ([gpu-alloc]), to find per-frame
// allocation churn (a create + destroy every frame costs the GPU thread kernel calls: CreateAllocation,
// FreeGpuVirtualAddress, DestroyAllocation2). Cheap: one atomic add per creation.

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include <rex/logging.h>

namespace rex::graphics::d3d12 {

inline void NoteResourceCreate(const char* site, uint64_t bytes) {
  struct Entry {
    const char* site;
    std::atomic<uint32_t> count;
    std::atomic<uint64_t> bytes;
  };
  static std::array<Entry, 32> entries{};
  static std::mutex mutex;
  static auto window = std::chrono::steady_clock::now();
  for (auto& e : entries) {
    if (!e.site) e.site = site;  // racy registration is fine for a diagnostic
    if (e.site == site) {
      e.count.fetch_add(1, std::memory_order_relaxed);
      e.bytes.fetch_add(bytes, std::memory_order_relaxed);
      break;
    }
  }
  std::lock_guard lock(mutex);
  const auto now = std::chrono::steady_clock::now();
  if (now - window < std::chrono::seconds(10)) return;
  window = now;
  for (auto& e : entries) {
    if (!e.site) break;
    const uint32_t n = e.count.exchange(0);
    const uint64_t b = e.bytes.exchange(0);
    if (n) REXGPU_INFO("[gpu-alloc] 10 s: {} creations, {:.1f} MB at {}", n, b / 1048576.0, e.site);
  }
}

}  // namespace rex::graphics::d3d12
