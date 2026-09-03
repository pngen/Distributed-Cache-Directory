#pragma once
#include <cstdint>
#include <string>

namespace distributedcachedirectory {

// Reusable state kinds represented by the directory. These label *what kind of
// reusable machine-produced state* a directory record points at. The directory
// never implements runtime- or state-specific semantics for any of these; it
// only tracks location, generation, integrity, freshness, health, reachability,
// and access economics for the copy that carries the kind.
enum class StateKind : std::uint8_t {
  KV_STATE = 0,
  PREFIX_STATE = 1,
  TENSOR_STATE = 2,
  MODEL_ARTIFACT = 3,
  MODEL_SHARD = 4,
  ADAPTER = 5,
  COMPILED_KERNEL = 6,
  EXECUTION_GRAPH = 7,
  EXECUTION_PLAN = 8,
  CHECKPOINT = 9,
  CHECKPOINT_CHUNK = 10,
  CHECKPOINT_MANIFEST = 11,
  BUFFER = 12,
  STORAGE_OBJECT = 13,
  GENERIC_STATE = 14,
};

inline const char* to_string(StateKind k) {
  switch (k) {
    case StateKind::KV_STATE: return "KV_STATE";
    case StateKind::PREFIX_STATE: return "PREFIX_STATE";
    case StateKind::TENSOR_STATE: return "TENSOR_STATE";
    case StateKind::MODEL_ARTIFACT: return "MODEL_ARTIFACT";
    case StateKind::MODEL_SHARD: return "MODEL_SHARD";
    case StateKind::ADAPTER: return "ADAPTER";
    case StateKind::COMPILED_KERNEL: return "COMPILED_KERNEL";
    case StateKind::EXECUTION_GRAPH: return "EXECUTION_GRAPH";
    case StateKind::EXECUTION_PLAN: return "EXECUTION_PLAN";
    case StateKind::CHECKPOINT: return "CHECKPOINT";
    case StateKind::CHECKPOINT_CHUNK: return "CHECKPOINT_CHUNK";
    case StateKind::CHECKPOINT_MANIFEST: return "CHECKPOINT_MANIFEST";
    case StateKind::BUFFER: return "BUFFER";
    case StateKind::STORAGE_OBJECT: return "STORAGE_OBJECT";
    case StateKind::GENERIC_STATE: return "GENERIC_STATE";
  }
  return "GENERIC_STATE";
}

// Cache classes. The directory never claims a physical cache class it cannot
// prove; a caller that cannot establish a physical class uses UNKNOWN.
enum class CacheKind : std::uint8_t {
  PROCESS_LOCAL = 0,
  DEVICE_LOCAL = 1,
  HOST_LOCAL = 2,
  NODE_LOCAL = 3,
  LOCAL_FILESYSTEM_CACHE = 4,
  REMOTE_NODE_CACHE = 5,
  SHARED_CACHE_CLASS = 6,
  OBJECT_CACHE_CLASS = 7,
  SYNTHETIC_REMOTE = 8,
  UNKNOWN = 9,
};

inline const char* to_string(CacheKind k) {
  switch (k) {
    case CacheKind::PROCESS_LOCAL: return "PROCESS_LOCAL";
    case CacheKind::DEVICE_LOCAL: return "DEVICE_LOCAL";
    case CacheKind::HOST_LOCAL: return "HOST_LOCAL";
    case CacheKind::NODE_LOCAL: return "NODE_LOCAL";
    case CacheKind::LOCAL_FILESYSTEM_CACHE: return "LOCAL_FILESYSTEM_CACHE";
    case CacheKind::REMOTE_NODE_CACHE: return "REMOTE_NODE_CACHE";
    case CacheKind::SHARED_CACHE_CLASS: return "SHARED_CACHE_CLASS";
    case CacheKind::OBJECT_CACHE_CLASS: return "OBJECT_CACHE_CLASS";
    case CacheKind::SYNTHETIC_REMOTE: return "SYNTHETIC_REMOTE";
    case CacheKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

// Freshness is modeled independently per subject (directory record, location,
// cache, health, reachability, observation). It is never collapsed to a single
// boolean, and a recovered process-local/CUDA location must not silently become
// CURRENT.
enum class Freshness : std::uint8_t {
  CURRENT = 0,
  STALE = 1,
  REVALIDATION_REQUIRED = 2,
  UNKNOWN = 3,
};

inline const char* to_string(Freshness f) {
  switch (f) {
    case Freshness::CURRENT: return "CURRENT";
    case Freshness::STALE: return "STALE";
    case Freshness::REVALIDATION_REQUIRED: return "REVALIDATION_REQUIRED";
    case Freshness::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

enum class Health : std::uint8_t {
  HEALTHY = 0,
  DEGRADED = 1,
  UNHEALTHY = 2,
  UNAVAILABLE = 3,
  UNKNOWN = 4,
};

inline const char* to_string(Health h) {
  switch (h) {
    case Health::HEALTHY: return "HEALTHY";
    case Health::DEGRADED: return "DEGRADED";
    case Health::UNHEALTHY: return "UNHEALTHY";
    case Health::UNAVAILABLE: return "UNAVAILABLE";
    case Health::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

enum class Integrity : std::uint8_t {
  UNKNOWN = 0,
  UNVERIFIED = 1,
  VERIFIED = 2,
  CORRUPT = 3,
  MISSING = 4,
};

inline const char* to_string(Integrity i) {
  switch (i) {
    case Integrity::UNKNOWN: return "UNKNOWN";
    case Integrity::UNVERIFIED: return "UNVERIFIED";
    case Integrity::VERIFIED: return "VERIFIED";
    case Integrity::CORRUPT: return "CORRUPT";
    case Integrity::MISSING: return "MISSING";
  }
  return "UNKNOWN";
}

enum class Reachability : std::uint8_t {
  REACHABLE = 0,
  DEGRADED = 1,
  UNREACHABLE = 2,
  REVALIDATION_REQUIRED = 3,
  UNKNOWN = 4,
};

inline const char* to_string(Reachability r) {
  switch (r) {
    case Reachability::REACHABLE: return "REACHABLE";
    case Reachability::DEGRADED: return "DEGRADED";
    case Reachability::UNREACHABLE: return "UNREACHABLE";
    case Reachability::REVALIDATION_REQUIRED: return "REVALIDATION_REQUIRED";
    case Reachability::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

// Directory record lifecycle. A record is never AVAILABLE merely because the
// metadata exists.
enum class LifecycleState : std::uint8_t {
  REGISTERED = 0,
  AVAILABLE = 1,
  DEGRADED = 2,
  STALE = 3,
  UNREACHABLE = 4,
  INVALIDATED = 5,
  SUPERSEDED = 6,
  TOMBSTONED = 7,
  EXPIRED = 8,
  MISSING = 9,
  FAILED = 10,
};

inline const char* to_string(LifecycleState s) {
  switch (s) {
    case LifecycleState::REGISTERED: return "REGISTERED";
    case LifecycleState::AVAILABLE: return "AVAILABLE";
    case LifecycleState::DEGRADED: return "DEGRADED";
    case LifecycleState::STALE: return "STALE";
    case LifecycleState::UNREACHABLE: return "UNREACHABLE";
    case LifecycleState::INVALIDATED: return "INVALIDATED";
    case LifecycleState::SUPERSEDED: return "SUPERSEDED";
    case LifecycleState::TOMBSTONED: return "TOMBSTONED";
    case LifecycleState::EXPIRED: return "EXPIRED";
    case LifecycleState::MISSING: return "MISSING";
    case LifecycleState::FAILED: return "FAILED";
  }
  return "FAILED";
}

// Provenance of an observation or cost/estimate.
enum class DataProvenance : std::uint8_t {
  MEASURED = 0,
  REPORTED = 1,
  DERIVED = 2,
  SYNTHETIC = 3,
  UNKNOWN = 4,
};

inline const char* to_string(DataProvenance p) {
  switch (p) {
    case DataProvenance::MEASURED: return "MEASURED";
    case DataProvenance::REPORTED: return "REPORTED";
    case DataProvenance::DERIVED: return "DERIVED";
    case DataProvenance::SYNTHETIC: return "SYNTHETIC";
    case DataProvenance::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

// Memory / storage domain. Only actual hardware is classified; an unknown fact
// stays UNKNOWN.
enum class MemoryDomain : std::uint8_t {
  CUDA_DEVICE = 0,
  HOST_PINNED = 1,
  HOST_MEMORY = 2,
  LOCAL_FILESYSTEM = 3,
  LOCAL_NVME_CLASS = 4,
  SHARED_FILESYSTEM_CLASS = 5,
  OBJECT_STORAGE_CLASS = 6,
  REMOTE_CACHE_CLASS = 7,
  SYNTHETIC_REMOTE = 8,
  UNKNOWN = 9,
};

inline const char* to_string(MemoryDomain d) {
  switch (d) {
    case MemoryDomain::CUDA_DEVICE: return "CUDA_DEVICE";
    case MemoryDomain::HOST_PINNED: return "HOST_PINNED";
    case MemoryDomain::HOST_MEMORY: return "HOST_MEMORY";
    case MemoryDomain::LOCAL_FILESYSTEM: return "LOCAL_FILESYSTEM";
    case MemoryDomain::LOCAL_NVME_CLASS: return "LOCAL_NVME_CLASS";
    case MemoryDomain::SHARED_FILESYSTEM_CLASS: return "SHARED_FILESYSTEM_CLASS";
    case MemoryDomain::OBJECT_STORAGE_CLASS: return "OBJECT_STORAGE_CLASS";
    case MemoryDomain::REMOTE_CACHE_CLASS: return "REMOTE_CACHE_CLASS";
    case MemoryDomain::SYNTHETIC_REMOTE: return "SYNTHETIC_REMOTE";
    case MemoryDomain::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

// Locality class for a candidate/location.
enum class LocalityClass : std::uint8_t {
  SAME_PROCESS = 0,
  SAME_DEVICE = 1,
  SAME_NODE = 2,
  SAME_NUMA = 3,
  NEAR = 4,
  REMOTE = 5,
  UNKNOWN = 6,
};

inline const char* to_string(LocalityClass l) {
  switch (l) {
    case LocalityClass::SAME_PROCESS: return "SAME_PROCESS";
    case LocalityClass::SAME_DEVICE: return "SAME_DEVICE";
    case LocalityClass::SAME_NODE: return "SAME_NODE";
    case LocalityClass::SAME_NUMA: return "SAME_NUMA";
    case LocalityClass::NEAR: return "NEAR";
    case LocalityClass::REMOTE: return "REMOTE";
    case LocalityClass::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

enum class CostClass : std::uint8_t {
  LOCAL = 0,
  NEAR = 1,
  REMOTE = 2,
  UNKNOWN = 3,
};

inline const char* to_string(CostClass c) {
  switch (c) {
    case CostClass::LOCAL: return "LOCAL";
    case CostClass::NEAR: return "NEAR";
    case CostClass::REMOTE: return "REMOTE";
    case CostClass::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

enum class LeaseState : std::uint8_t {
  ACTIVE = 0,
  EXPIRED = 1,
  REVOKED = 2,
  REVALIDATION_REQUIRED = 3,
  UNKNOWN = 4,
};

inline const char* to_string(LeaseState s) {
  switch (s) {
    case LeaseState::ACTIVE: return "ACTIVE";
    case LeaseState::EXPIRED: return "EXPIRED";
    case LeaseState::REVOKED: return "REVOKED";
    case LeaseState::REVALIDATION_REQUIRED: return "REVALIDATION_REQUIRED";
    case LeaseState::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

// Query outcome classification.
enum class QueryOutcome : std::uint8_t {
  FOUND_LOCAL = 0,
  FOUND_REMOTE = 1,
  FOUND_MULTIPLE = 2,
  NOT_FOUND = 3,
  STALE_ONLY = 4,
  UNREACHABLE_ONLY = 5,
  CORRUPT_ONLY = 6,
  INCOMPATIBLE_ONLY = 7,
  INSUFFICIENT_EVIDENCE = 8,
  UNKNOWN = 9,
};

inline const char* to_string(QueryOutcome o) {
  switch (o) {
    case QueryOutcome::FOUND_LOCAL: return "FOUND_LOCAL";
    case QueryOutcome::FOUND_REMOTE: return "FOUND_REMOTE";
    case QueryOutcome::FOUND_MULTIPLE: return "FOUND_MULTIPLE";
    case QueryOutcome::NOT_FOUND: return "NOT_FOUND";
    case QueryOutcome::STALE_ONLY: return "STALE_ONLY";
    case QueryOutcome::UNREACHABLE_ONLY: return "UNREACHABLE_ONLY";
    case QueryOutcome::CORRUPT_ONLY: return "CORRUPT_ONLY";
    case QueryOutcome::INCOMPATIBLE_ONLY: return "INCOMPATIBLE_ONLY";
    case QueryOutcome::INSUFFICIENT_EVIDENCE: return "INSUFFICIENT_EVIDENCE";
    case QueryOutcome::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

}  // namespace distributedcachedirectory
