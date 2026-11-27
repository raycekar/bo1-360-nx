#pragma once
/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <atomic>
#include <mutex>

#include <rex/system/xobject.h>
#include <rex/system/xtypes.h>
#include <rex/thread.h>

namespace rex::system {

// https://www.nirsoft.net/kernel_struct/vista/KEVENT.html
struct X_KEVENT {
  X_DISPATCH_HEADER header;
};
static_assert_size(X_KEVENT, 0x10);

class XEvent : public XObject {
 public:
  static const XObject::Type kObjectType = XObject::Type::Event;

  explicit XEvent(KernelState* kernel_state);
  ~XEvent() override;

  void Initialize(bool manual_reset, bool initial_state);
  void InitializeNative(void* native_ptr, X_DISPATCH_HEADER* header);

  void Query(uint32_t* out_type, uint32_t* out_state);

  int32_t Set(uint32_t priority_increment, bool wait);
  int32_t Pulse(uint32_t priority_increment, bool wait);
  int32_t Reset();
  void Clear();

  bool Save(stream::ByteStream* stream) override;
  static object_ref<XEvent> Restore(KernelState* kernel_state, stream::ByteStream* stream);

  // Zero-timeout polls without a system call. Games spin on "is it set yet?" (BO1's job system: about 200,000 polls a
  // second on the main thread), which on the 360 is a cheap kernel call but here is a host wait plus a yield. The
  // hint is (sequence << 1) | signaled; every Set/Reset bumps the sequence, so a poller only clears the signaled bit
  // when nothing changed while it looked (a stale clear could otherwise hide a Set from pollers for good).
  uint64_t poll_hint() const { return poll_hint_.load(std::memory_order_acquire); }
  bool manual_reset() const { return manual_reset_; }
  void RepairPollHint() { BumpPollHint(true); }  // a real poll found it signaled behind the hint
  uint32_t CountFastPoll() { return fast_polls_.fetch_add(1, std::memory_order_relaxed); }
  // After a real zero-timeout poll that found the event unsignaled, or consumed an auto-reset signal.
  void ClearPollHint(uint64_t seen) {
    poll_hint_.compare_exchange_strong(seen, seen & ~uint64_t(1), std::memory_order_acq_rel);
  }
 protected:
  rex::thread::WaitHandle* GetWaitHandle() override { return event_.get(); }

 private:
  void BumpPollHint(bool signaled) {
    uint64_t v = poll_hint_.load(std::memory_order_relaxed);
    while (!poll_hint_.compare_exchange_weak(v, (((v >> 1) + 1) << 1) | (signaled ? 1 : 0),
                                             std::memory_order_acq_rel)) {
    }
  }

  bool manual_reset_ = false;
  std::unique_ptr<rex::thread::Event> event_;
  std::mutex state_mutex_;  // Set/Reset: host event and poll hint in one step
  std::atomic<uint32_t> fast_polls_{0};
  std::atomic<uint64_t> poll_hint_{1};  // signaled until known otherwise: the first poll asks the host
};

}  // namespace rex::system
