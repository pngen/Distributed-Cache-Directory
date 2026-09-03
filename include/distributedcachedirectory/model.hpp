#pragma once
#include "authority.hpp"
#include "digest.hpp"
#include "enums.hpp"
#include "generation.hpp"
#include "strong_type.hpp"
#include "time.hpp"
#include <cstdint>
#include <string>

namespace distributedcachedirectory {

using ContentDigest = digest::Sha256;

// ---- Access economics (the directory never becomes a transfer fabric) ----

struct AccessEstimate {
  double estimated_latency_ns{0.0};
  double estimated_bandwidth_bytes_per_s{0.0};
  std::uint64_t transfer_bytes{0};
  std::uint32_t staging_steps{0};
  bool restore_required{false};
  std::uint32_t remote_hops{0};
  LocalityClass locality{LocalityClass::UNKNOWN};
  CostClass cost_class{CostClass::UNKNOWN};
  DataProvenance provenance{DataProvenance::UNKNOWN};

  bool operator==(const AccessEstimate& o) const {
    return estimated_latency_ns == o.estimated_latency_ns &&
           estimated_bandwidth_bytes_per_s == o.estimated_bandwidth_bytes_per_s &&
           transfer_bytes == o.transfer_bytes && staging_steps == o.staging_steps &&
           restore_required == o.restore_required && remote_hops == o.remote_hops &&
           locality == o.locality && cost_class == o.cost_class && provenance == o.provenance;
  }
};

// ---- Location / locator ----

// A locator is an opaque, non-pointer key describing how to reach a location.
// It is a string because raw pointers (host or CUDA device pointers) are
// process-scoped and must never be persisted or reused as cross-process
// authority after a process restart.
struct Locator {
  std::string key;

  bool operator==(const Locator& o) const { return key == o.key; }
  bool operator!=(const Locator& o) const { return key != o.key; }
  bool operator<(const Locator& o) const { return key < o.key; }
};

struct LocationDescriptor {
  LocationId location_id;
  NodeId node;
  WorkerId worker;
  WorkerBootId worker_boot;
  DeviceId device;                       // null if not device-scoped
  MemoryDomain domain{MemoryDomain::UNKNOWN};
  StorageBackendId backend;
  StorageTierId tier;
  std::string process_tag;               // informational process scope
  Locator locator;
  std::uint64_t logical_bytes{0};
  std::uint64_t physical_bytes{0};
  bool physical_bytes_known{false};
  ContentDigest content_digest{};
  bool content_digest_known{false};
  LocalityClass locality{LocalityClass::UNKNOWN};
  Reachability reachability{Reachability::UNKNOWN};
  Health health{Health::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  Integrity integrity{Integrity::UNKNOWN};
  AccessEstimate estimate;
  LocationGeneration generation;
  ProvenanceId provenance;
  AuthorityEnvelope authority;
  TimestampNs created_ns{0};
  TimestampNs observed_ns{0};

  bool operator==(const LocationDescriptor& o) const {
    return location_id == o.location_id && node == o.node && worker == o.worker &&
           worker_boot == o.worker_boot && device == o.device && domain == o.domain &&
           backend == o.backend && tier == o.tier && process_tag == o.process_tag &&
           locator == o.locator && logical_bytes == o.logical_bytes &&
           physical_bytes == o.physical_bytes && physical_bytes_known == o.physical_bytes_known &&
           content_digest_known == o.content_digest_known &&
           digest::digest_equal(content_digest, o.content_digest) &&
           locality == o.locality && reachability == o.reachability && health == o.health &&
           freshness == o.freshness && integrity == o.integrity && estimate == o.estimate &&
           generation == o.generation && provenance == o.provenance &&
           authority == o.authority && created_ns == o.created_ns && observed_ns == o.observed_ns;
  }
};

// ---- Lease ----

struct LeaseDescriptor {
  LeaseId lease_id;
  LeaseGeneration generation;
  WorkerBootId holder;
  TimestampNs start_ns{0};
  TimestampNs expiry_ns{0};
  bool expires{false};
  LeaseState state{LeaseState::UNKNOWN};
  ProvenanceId provenance;
  AuthorityEnvelope authority;

  bool operator==(const LeaseDescriptor& o) const {
    return lease_id == o.lease_id && generation == o.generation && holder == o.holder &&
           start_ns == o.start_ns && expiry_ns == o.expiry_ns && expires == o.expires &&
           state == o.state && provenance == o.provenance && authority == o.authority;
  }
};

// ---- Replica ----

struct ReplicaDescriptor {
  ReplicaId replica_id;
  LocationId location;
  ReplicaGeneration generation;
  CacheId cache;
  CacheEntryId entry;
  Reachability reachability{Reachability::UNKNOWN};
  Health health{Health::UNKNOWN};
  Integrity integrity{Integrity::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  TimestampNs observed_ns{0};
  AuthorityEnvelope authority;

  bool operator==(const ReplicaDescriptor& o) const {
    return replica_id == o.replica_id && location == o.location && generation == o.generation &&
           cache == o.cache && entry == o.entry && reachability == o.reachability &&
           health == o.health && integrity == o.integrity && freshness == o.freshness &&
           observed_ns == o.observed_ns && authority == o.authority;
  }
};

// ---- Cache ----

struct CacheDescriptor {
  CacheId cache_id;
  CacheKind kind{CacheKind::UNKNOWN};
  NodeId node;
  WorkerId worker;
  WorkerBootId worker_boot;
  MemoryDomain domain{MemoryDomain::UNKNOWN};
  std::uint64_t capacity{0};
  bool capacity_known{false};
  std::uint64_t free_capacity{0};
  bool free_capacity_known{false};
  Reachability reachability{Reachability::UNKNOWN};
  Health health{Health::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  ProvenanceId provenance;
  CacheGeneration generation;
  AuthorityEnvelope authority;
  TimestampNs observed_ns{0};

  bool operator==(const CacheDescriptor& o) const {
    return cache_id == o.cache_id && kind == o.kind && node == o.node && worker == o.worker &&
           worker_boot == o.worker_boot && domain == o.domain && capacity == o.capacity &&
           capacity_known == o.capacity_known && free_capacity == o.free_capacity &&
           free_capacity_known == o.free_capacity_known && reachability == o.reachability &&
           health == o.health && freshness == o.freshness && provenance == o.provenance &&
           generation == o.generation && authority == o.authority && observed_ns == o.observed_ns;
  }
};

// ---- Directory record ----

struct DirectoryRecord {
  DirectoryRecordId record_id;
  StateId state_id;
  StateGeneration state_generation;
  StateKind kind{StateKind::GENERIC_STATE};
  std::string name_space;                         // namespace if represented
  CacheId cache;
  CacheEntryId entry;
  ReplicaId replica;
  LocationId location;
  NodeId node;
  WorkerId worker;
  WorkerBootId worker_boot;
  DeviceId device;                                // null if not device-scoped
  MemoryDomain domain{MemoryDomain::UNKNOWN};
  std::uint64_t logical_bytes{0};
  std::uint64_t physical_bytes{0};
  bool physical_bytes_known{false};
  ContentDigest content_digest{};
  bool content_digest_known{false};
  CompatibilityId compatibility;
  ProvenanceId provenance;
  LocationGeneration location_generation;
  ReplicaGeneration replica_generation;
  EntryGeneration entry_generation;
  Health health{Health::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  Integrity integrity{Integrity::UNKNOWN};
  Reachability reachability{Reachability::UNKNOWN};
  LeaseDescriptor lease;
  bool has_lease{false};
  AccessEstimate estimate;
  PolicyGeneration policy_generation;
  AuthorityEnvelope authority;
  RecordGeneration record_generation;
  LifecycleState lifecycle{LifecycleState::REGISTERED};
  bool current{false};
  TimestampNs created_ns{0};
  TimestampNs updated_ns{0};
  TimestampNs observed_ns{0};
  ContentDigest semantic_digest{};

  bool operator==(const DirectoryRecord& o) const {
    return record_id == o.record_id && state_id == o.state_id &&
           state_generation == o.state_generation && kind == o.kind &&
           name_space == o.name_space && cache == o.cache && entry == o.entry &&
           replica == o.replica && location == o.location && node == o.node &&
           worker == o.worker && worker_boot == o.worker_boot && device == o.device &&
           domain == o.domain && logical_bytes == o.logical_bytes &&
           physical_bytes == o.physical_bytes && physical_bytes_known == o.physical_bytes_known &&
           content_digest_known == o.content_digest_known &&
           digest::digest_equal(content_digest, o.content_digest) &&
           compatibility == o.compatibility && provenance == o.provenance &&
           location_generation == o.location_generation &&
           replica_generation == o.replica_generation &&
           entry_generation == o.entry_generation && health == o.health &&
           freshness == o.freshness && integrity == o.integrity &&
           reachability == o.reachability && has_lease == o.has_lease &&
           (!has_lease || lease == o.lease) && estimate == o.estimate &&
           policy_generation == o.policy_generation && authority == o.authority &&
           record_generation == o.record_generation && lifecycle == o.lifecycle &&
           current == o.current && created_ns == o.created_ns &&
           updated_ns == o.updated_ns && observed_ns == o.observed_ns &&
           digest::digest_equal(semantic_digest, o.semantic_digest);
  }
};

// ---- Tombstone ----

enum class TombstoneKind : std::uint8_t {
  STATE = 0,
  CACHE = 1,
  ENTRY = 2,
  REPLICA = 3,
  LOCATION = 4,
  WORKER_BOOT = 5,
  NODE = 6,
  DEVICE = 7,
  BACKEND = 8,
  COMPATIBILITY = 9,
  POLICY = 10,
  CONTENT = 11,
  GENERIC = 12,
};

inline const char* to_string(TombstoneKind k) {
  switch (k) {
    case TombstoneKind::STATE: return "STATE";
    case TombstoneKind::CACHE: return "CACHE";
    case TombstoneKind::ENTRY: return "ENTRY";
    case TombstoneKind::REPLICA: return "REPLICA";
    case TombstoneKind::LOCATION: return "LOCATION";
    case TombstoneKind::WORKER_BOOT: return "WORKER_BOOT";
    case TombstoneKind::NODE: return "NODE";
    case TombstoneKind::DEVICE: return "DEVICE";
    case TombstoneKind::BACKEND: return "BACKEND";
    case TombstoneKind::COMPATIBILITY: return "COMPATIBILITY";
    case TombstoneKind::POLICY: return "POLICY";
    case TombstoneKind::CONTENT: return "CONTENT";
    case TombstoneKind::GENERIC: return "GENERIC";
  }
  return "GENERIC";
}

struct TombstoneTarget {
  TombstoneKind kind{TombstoneKind::GENERIC};
  StateId state;
  CacheId cache;
  CacheEntryId entry;
  ReplicaId replica;
  LocationId location;
  WorkerBootId worker_boot;
  NodeId node;
  DeviceId device;
  StorageBackendId backend;
  CompatibilityId compatibility;
  PolicyGeneration policy_generation;
  ContentDigest content{};
  std::uint64_t generation_floor{0};  // target generation floor

  bool operator==(const TombstoneTarget& o) const {
    return kind == o.kind && state == o.state && cache == o.cache && entry == o.entry &&
           replica == o.replica && location == o.location && worker_boot == o.worker_boot &&
           node == o.node && device == o.device && backend == o.backend &&
           compatibility == o.compatibility && policy_generation == o.policy_generation &&
           digest::digest_equal(content, o.content) && generation_floor == o.generation_floor;
  }
};

struct TombstoneRecord {
  TombstoneId tombstone_id;
  TombstoneTarget target;
  CoordinatorEpoch epoch;
  WorkerBootId worker_boot;
  DirectoryGeneration authority_generation;
  std::string reason;
  TimestampNs timestamp_ns{0};
  ProvenanceId provenance;

  bool operator==(const TombstoneRecord& o) const {
    return tombstone_id == o.tombstone_id && target == o.target && epoch == o.epoch &&
           worker_boot == o.worker_boot && authority_generation == o.authority_generation &&
           reason == o.reason && timestamp_ns == o.timestamp_ns && provenance == o.provenance;
  }
};

// ---- Observation (completed, recorded) ----

struct ObservationRecord {
  ObservationId observation_id;
  ObservationGeneration generation;
  WorkerBootId worker_boot;
  std::string subject;                 // e.g. "location", "cache", "replica", "worker"
  std::string subject_key;             // the identity it observed
  Reachability reachability{Reachability::UNKNOWN};
  Health health{Health::UNKNOWN};
  Integrity integrity{Integrity::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  double measured_latency_ns{0.0};
  DataProvenance provenance{DataProvenance::UNKNOWN};
  AuthorityEnvelope authority;
  TimestampNs observed_ns{0};

  bool operator==(const ObservationRecord& o) const {
    return observation_id == o.observation_id && generation == o.generation &&
           worker_boot == o.worker_boot && subject == o.subject &&
           subject_key == o.subject_key && reachability == o.reachability &&
           health == o.health && integrity == o.integrity && freshness == o.freshness &&
           measured_latency_ns == o.measured_latency_ns && provenance == o.provenance &&
           authority == o.authority && observed_ns == o.observed_ns;
  }
};

}  // namespace distributedcachedirectory
