#pragma once
#include "generation.hpp"
#include "strong_type.hpp"
#include <cstdint>

namespace distributedcachedirectory {

// The authority envelope carried by every directory assertion (registration,
// update, invalidate, tombstone, lease renewal).
//
// Authority is incarnation-scoped: a mutation is only admissible under a
// *current* authority. The current authority is characterized by three things:
//   - the coordinator epoch it was issued under,
//   - the WorkerBootId of the incarnation that asserts it, and
//   - a per-(record, boot) fence counter that is monotonic *within one boot*.
//
// A numerically larger fence from an old WorkerBootId can never fence a fresh
// process incarnation, because authority is keyed by WorkerBootId first. A stale
// replay fails the epoch check, the boot check, or the tombstone/generation
// check well before any fence comparison.
struct AuthorityEnvelope {
  CoordinatorEpoch epoch;
  WorkerBootId boot;
  DirectoryGeneration directory_generation;
  std::uint64_t fence{0};

  bool operator==(const AuthorityEnvelope& o) const {
    return epoch == o.epoch && boot == o.boot &&
           directory_generation == o.directory_generation && fence == o.fence;
  }
  bool operator!=(const AuthorityEnvelope& o) const { return !(*this == o); }
};

}  // namespace distributedcachedirectory
