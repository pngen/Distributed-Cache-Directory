#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace distributedcachedirectory {

// Structured directory error classification. Used both for reporting to callers
// and for the multiprocess stale-mutation proof (which classifies every rejected
// assertion).
enum class DirectoryError : std::uint8_t {
  NONE = 0,
  STALE_EPOCH = 1,
  STALE_WORKER_BOOT = 2,
  STALE_AUTHORITY = 3,
  STALE_GENERATION = 4,
  RECORD_GENERATION_REGRESSION = 5,
  DUPLICATE_IDENTITY = 6,
  CONFLICTING_DUPLICATE = 7,
  TOMBSTONED = 8,
  INVALID_IDENTITY = 9,
  MISSING_DEPENDENCY = 10,
  DIMENSION_MISMATCH = 11,
  NO_AUTHORITY = 12,
  FAILED_LEASE_VALIDATION = 13,
  INVALID_STATE_TRANSITION = 14,
  CORRUPT_DIGEST = 15,
  INDEX_INCONSISTENT = 16,
  ACCOUNTING_NEGATIVE = 17,
  PERSISTENCE_CORRUPT = 18,
  PROTOCOL_VIOLATION = 19,
  RESOURCE_EXHAUSTED = 20,
  INTERNAL = 21,
};

inline const char* to_string(DirectoryError e) {
  switch (e) {
    case DirectoryError::NONE: return "NONE";
    case DirectoryError::STALE_EPOCH: return "STALE_EPOCH";
    case DirectoryError::STALE_WORKER_BOOT: return "STALE_WORKER_BOOT";
    case DirectoryError::STALE_AUTHORITY: return "STALE_AUTHORITY";
    case DirectoryError::STALE_GENERATION: return "STALE_GENERATION";
    case DirectoryError::RECORD_GENERATION_REGRESSION: return "RECORD_GENERATION_REGRESSION";
    case DirectoryError::DUPLICATE_IDENTITY: return "DUPLICATE_IDENTITY";
    case DirectoryError::CONFLICTING_DUPLICATE: return "CONFLICTING_DUPLICATE";
    case DirectoryError::TOMBSTONED: return "TOMBSTONED";
    case DirectoryError::INVALID_IDENTITY: return "INVALID_IDENTITY";
    case DirectoryError::MISSING_DEPENDENCY: return "MISSING_DEPENDENCY";
    case DirectoryError::DIMENSION_MISMATCH: return "DIMENSION_MISMATCH";
    case DirectoryError::NO_AUTHORITY: return "NO_AUTHORITY";
    case DirectoryError::FAILED_LEASE_VALIDATION: return "FAILED_LEASE_VALIDATION";
    case DirectoryError::INVALID_STATE_TRANSITION: return "INVALID_STATE_TRANSITION";
    case DirectoryError::CORRUPT_DIGEST: return "CORRUPT_DIGEST";
    case DirectoryError::INDEX_INCONSISTENT: return "INDEX_INCONSISTENT";
    case DirectoryError::ACCOUNTING_NEGATIVE: return "ACCOUNTING_NEGATIVE";
    case DirectoryError::PERSISTENCE_CORRUPT: return "PERSISTENCE_CORRUPT";
    case DirectoryError::PROTOCOL_VIOLATION: return "PROTOCOL_VIOLATION";
    case DirectoryError::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
    case DirectoryError::INTERNAL: return "INTERNAL";
  }
  return "INTERNAL";
}

template <typename T>
struct Result {
  bool ok{false};
  DirectoryError error{DirectoryError::NONE};
  std::string error_text;
  T value{};

  static Result success(T v) { Result r; r.ok = true; r.value = std::move(v); return r; }
  static Result failure(DirectoryError e, std::string msg = {}) {
    Result r; r.ok = false; r.error = e; r.error_text = std::move(msg); return r;
  }
  explicit operator bool() const { return ok; }
  const T& operator*() const { return value; }
  T& operator*() { return value; }
};

struct AckResult {
  bool ok{true};
  std::string message;
};

struct InvalidateResult {
  bool ok{false};
  std::uint32_t affected_records{0};
  std::string message;
};

}  // namespace distributedcachedirectory
