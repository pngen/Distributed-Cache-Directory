#pragma once
#include "errors.hpp"
#include <cstdint>
#include <string>

namespace distributedcachedirectory {

// Exact accounting of directory activity. Every counter is a signed value so
// that an accounting error (a decrement below zero, a double-account, or a
// mismatched pair) is detectable; accounting_validate() reports the first
// negative or inconsistent counter.
#define DCD_ACCT_FIELDS(X)              \
  X(caches)                             \
  X(current_records)                    \
  X(historical_records)                 \
  X(logical_states)                     \
  X(replicas)                           \
  X(locations)                          \
  X(cuda_locations)                     \
  X(host_locations)                     \
  X(storage_locations)                  \
  X(reachable_records)                  \
  X(stale_records)                      \
  X(degraded_records)                   \
  X(corrupt_records)                    \
  X(active_leases)                      \
  X(expired_leases)                     \
  X(invalidations)                      \
  X(tombstones)                         \
  X(registrations)                      \
  X(updates)                            \
  X(queries)                            \
  X(local_hits)                         \
  X(remote_hits)                        \
  X(misses)                             \
  X(stale_only_outcomes)                \
  X(unreachable_only_outcomes)          \
  X(integrity_failures)                 \
  X(stale_mutation_rejections)          \
  X(duplicate_conflict_rejections)      \
  X(worker_restarts)                    \

struct Accounting {
#define DCD_FIELD(name) std::int64_t name{0};
  DCD_ACCT_FIELDS(DCD_FIELD)
#undef DCD_FIELD

  bool validate(std::string& problem) const {
    bool ok = true;
#define DCD_CHK(name) if (name < 0) { problem = std::string(#name) + " is negative"; ok = false; }
    DCD_ACCT_FIELDS(DCD_CHK)
#undef DCD_CHK
    return ok;
  }

  bool validate() const {
    std::string p;
    return validate(p);
  }

  Accounting operator+(const Accounting& o) const {
    Accounting r;
#define DCD_ADD(name) r.name = name + o.name;
    DCD_ACCT_FIELDS(DCD_ADD)
#undef DCD_ADD
    return r;
  }
};

}  // namespace distributedcachedirectory
