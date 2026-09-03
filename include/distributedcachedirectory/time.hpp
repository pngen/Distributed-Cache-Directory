#pragma once
#include <chrono>
#include <cstdint>

namespace distributedcachedirectory {

// Wall-clock nanoseconds since the Unix epoch. Used only for human-readable
// timestamps and lease bookkeeping; all authority and generation decisions use
// counting semantics, never wall-clock TTL as the sole liveness model.
using TimestampNs = std::uint64_t;

inline TimestampNs wall_clock_ns() {
  return static_cast<TimestampNs>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

inline TimestampNs steady_ns() {
  return static_cast<TimestampNs>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

// A monotonic "ticks" counter for deterministic orderings in tests.
class SteadyCounter {
 public:
  TimestampNs next() { return ++counter_; }
  TimestampNs value() const { return counter_; }
 private:
  TimestampNs counter_{0};
};

}  // namespace distributedcachedirectory
