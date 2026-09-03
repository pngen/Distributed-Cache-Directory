#pragma once
#include "enums.hpp"
#include "model.hpp"
#include "strong_type.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace distributedcachedirectory {

// ---- Rejection reasons for hard-filter elimination ----

enum class RejectionReason : std::uint8_t {
  WRONG_STATE,
  WRONG_STATE_GENERATION,
  WRONG_CONTENT_DIGEST,
  STALE_RECORD,
  TOMBSTONED,
  INVALIDATED,
  CORRUPT,
  MISSING_BYTES,
  UNREACHABLE,
  INCOMPATIBLE,
  DOMAIN_UNAVAILABLE,
  HEALTH_BELOW_REQUIREMENT,
  FRESHNESS_BELOW_REQUIREMENT,
  REACHABILITY_BELOW_REQUIREMENT,
  INTEGRITY_BELOW_REQUIREMENT,
  STALE_AUTHORITY,
  EXPIRED_LEASE,
  MISSING_REQUIRED_EVIDENCE,
  NOT_CURRENT,
  NO_REQUIRED_REPLICA_COUNT,
};

inline const char* to_string(RejectionReason r) {
  switch (r) {
    case RejectionReason::WRONG_STATE: return "WRONG_STATE";
    case RejectionReason::WRONG_STATE_GENERATION: return "WRONG_STATE_GENERATION";
    case RejectionReason::WRONG_CONTENT_DIGEST: return "WRONG_CONTENT_DIGEST";
    case RejectionReason::STALE_RECORD: return "STALE_RECORD";
    case RejectionReason::TOMBSTONED: return "TOMBSTONED";
    case RejectionReason::INVALIDATED: return "INVALIDATED";
    case RejectionReason::CORRUPT: return "CORRUPT";
    case RejectionReason::MISSING_BYTES: return "MISSING_BYTES";
    case RejectionReason::UNREACHABLE: return "UNREACHABLE";
    case RejectionReason::INCOMPATIBLE: return "INCOMPATIBLE";
    case RejectionReason::DOMAIN_UNAVAILABLE: return "DOMAIN_UNAVAILABLE";
    case RejectionReason::HEALTH_BELOW_REQUIREMENT: return "HEALTH_BELOW_REQUIREMENT";
    case RejectionReason::FRESHNESS_BELOW_REQUIREMENT: return "FRESHNESS_BELOW_REQUIREMENT";
    case RejectionReason::REACHABILITY_BELOW_REQUIREMENT: return "REACHABILITY_BELOW_REQUIREMENT";
    case RejectionReason::INTEGRITY_BELOW_REQUIREMENT: return "INTEGRITY_BELOW_REQUIREMENT";
    case RejectionReason::STALE_AUTHORITY: return "STALE_AUTHORITY";
    case RejectionReason::EXPIRED_LEASE: return "EXPIRED_LEASE";
    case RejectionReason::MISSING_REQUIRED_EVIDENCE: return "MISSING_REQUIRED_EVIDENCE";
    case RejectionReason::NOT_CURRENT: return "NOT_CURRENT";
    case RejectionReason::NO_REQUIRED_REPLICA_COUNT: return "NO_REQUIRED_REPLICA_COUNT";
  }
  return "MISSING_REQUIRED_EVIDENCE";
}

// ---- Named ranking factors ----

enum class RankingFactorKind : std::uint8_t {
  EXACT_GENERATION,
  SAME_PROCESS,
  SAME_DEVICE,
  SAME_NODE,
  SAME_NUMA,
  DOMAIN_PREFERENCE,
  REACHABILITY,
  HEALTH,
  FRESHNESS,
  INTEGRITY,
  LATENCY,
  BANDWIDTH,
  TRANSFER_BYTES,
  STAGING,
  RESTORE,
  REPLICA_DIVERSITY,
  POLICY_PREFERENCE,
};

inline const char* to_string(RankingFactorKind k) {
  switch (k) {
    case RankingFactorKind::EXACT_GENERATION: return "EXACT_GENERATION";
    case RankingFactorKind::SAME_PROCESS: return "SAME_PROCESS";
    case RankingFactorKind::SAME_DEVICE: return "SAME_DEVICE";
    case RankingFactorKind::SAME_NODE: return "SAME_NODE";
    case RankingFactorKind::SAME_NUMA: return "SAME_NUMA";
    case RankingFactorKind::DOMAIN_PREFERENCE: return "DOMAIN_PREFERENCE";
    case RankingFactorKind::REACHABILITY: return "REACHABILITY";
    case RankingFactorKind::HEALTH: return "HEALTH";
    case RankingFactorKind::FRESHNESS: return "FRESHNESS";
    case RankingFactorKind::INTEGRITY: return "INTEGRITY";
    case RankingFactorKind::LATENCY: return "LATENCY";
    case RankingFactorKind::BANDWIDTH: return "BANDWIDTH";
    case RankingFactorKind::TRANSFER_BYTES: return "TRANSFER_BYTES";
    case RankingFactorKind::STAGING: return "STAGING";
    case RankingFactorKind::RESTORE: return "RESTORE";
    case RankingFactorKind::REPLICA_DIVERSITY: return "REPLICA_DIVERSITY";
    case RankingFactorKind::POLICY_PREFERENCE: return "POLICY_PREFERENCE";
  }
  return "POLICY_PREFERENCE";
}

struct RankingFactor {
  RankingFactorKind kind;
  double weight{0.0};
  double value{0.0};         // normalized 0..1 contribution
  double contribution{0.0};  // weight * value
  std::string note;
};

// ---- Directory query ----

struct DirectoryQuery {
  QueryId query_id;
  bool exact_state{false};
  StateId state;
  bool exact_state_generation{false};
  StateGeneration state_generation;
  bool has_kind{false};
  StateKind kind{StateKind::GENERIC_STATE};
  std::string name_space;
  bool require_content_digest{false};
  ContentDigest content_digest{};
  bool require_compatibility{false};
  CompatibilityId compatibility;
  bool has_integrity{false};
  Integrity integrity{Integrity::UNKNOWN};
  bool has_freshness{false};
  Freshness freshness{Freshness::UNKNOWN};
  bool has_health{false};
  Health health{Health::UNKNOWN};
  bool has_reachability{false};
  Reachability reachability{Reachability::UNKNOWN};
  std::vector<MemoryDomain> allowed_domains;
  std::vector<MemoryDomain> preferred_domains;
  bool has_max_latency{false};
  double max_latency_ns{0.0};
  bool has_preferred_node{false};
  NodeId preferred_node;
  bool has_preferred_device{false};
  DeviceId preferred_device;
  std::string requester_process_tag;   // locality: same-process factor
  NodeId requester_node;                 // locality: same-node factor
  std::uint32_t min_replica_count{0};
  bool current_only{true};
};

// ---- Explanation entries ----

struct ExplanationEntry {
  std::string kind;   // e.g. "query", "candidate", "rejection", "location", ...
  std::string text;
};

struct Candidate {
  DirectoryRecord record;
  std::vector<RankingFactor> factors;
  double score{0.0};
  bool selected{false};

  bool operator==(const Candidate& o) const { return record == o.record; }
};

struct Rejection {
  DirectoryRecordId record_id;
  RejectionReason reason;
  StateId state;
  StateGeneration state_generation;
  ReplicaId replica;
  LocationId location;
};

struct QueryResult {
  QueryId query_id;
  QueryOutcome outcome{QueryOutcome::UNKNOWN};
  std::vector<Candidate> candidates;      // ranked, current-eligible
  std::vector<Rejection> rejections;      // hard-filtered records
  std::uint32_t selected_index{0};
  std::uint32_t matched_scan_count{0};    // canonical scan count (invariant)
  std::vector<ExplanationEntry> explanation;
  std::string summary;
};

}  // namespace distributedcachedirectory
