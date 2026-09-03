#pragma once
#include "authority.hpp"
#include "directory.hpp"
#include "model.hpp"
#include "query.hpp"
#include "serde.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Windows system headers define `ERROR` as a macro; keep the protocol enum
// enumerator `ERROR` usable by undefining it before the enum is declared.
#undef ERROR
#undef NOERROR

namespace distributedcachedirectory {
namespace protocol {

inline constexpr std::uint32_t FRAME_MAGIC = 0x46444331u;
inline constexpr std::uint32_t FRAME_VERSION = 1u;
inline constexpr std::uint32_t MAX_PAYLOAD = 64u * 1024u * 1024u;

enum class MessageKind : std::uint8_t {
  HELLO = 0, REGISTER_CACHE = 1, REGISTER_ENTRY = 2, UPDATE_LOCATION = 3,
  UPDATE_HEALTH = 4, UPDATE_FRESHNESS = 5, UPDATE_REACHABILITY = 6, UPDATE_INTEGRITY = 7,
  RENEW_LEASE = 8, INVALIDATE = 9, TOMBSTONE = 10, QUERY = 11, QUERY_RESULT = 12,
  SAVE = 13, RECOVER = 14, ACK = 15, ERROR = 16,
  REGISTER_LOCATION = 17, REGISTER_REPLICA = 18,
};

inline const char* to_string(MessageKind k) {
  switch (k) {
    case MessageKind::HELLO: return "HELLO";
    case MessageKind::REGISTER_CACHE: return "REGISTER_CACHE";
    case MessageKind::REGISTER_ENTRY: return "REGISTER_ENTRY";
    case MessageKind::UPDATE_LOCATION: return "UPDATE_LOCATION";
    case MessageKind::UPDATE_HEALTH: return "UPDATE_HEALTH";
    case MessageKind::UPDATE_FRESHNESS: return "UPDATE_FRESHNESS";
    case MessageKind::UPDATE_REACHABILITY: return "UPDATE_REACHABILITY";
    case MessageKind::UPDATE_INTEGRITY: return "UPDATE_INTEGRITY";
    case MessageKind::RENEW_LEASE: return "RENEW_LEASE";
    case MessageKind::INVALIDATE: return "INVALIDATE";
    case MessageKind::TOMBSTONE: return "TOMBSTONE";
    case MessageKind::QUERY: return "QUERY";
    case MessageKind::QUERY_RESULT: return "QUERY_RESULT";
    case MessageKind::SAVE: return "SAVE";
    case MessageKind::RECOVER: return "RECOVER";
    case MessageKind::ACK: return "ACK";
    case MessageKind::ERROR: return "ERROR";
    case MessageKind::REGISTER_LOCATION: return "REGISTER_LOCATION";
    case MessageKind::REGISTER_REPLICA: return "REGISTER_REPLICA";
  }
  return "UNKNOWN";
}

struct Frame { MessageKind kind{MessageKind::ERROR}; std::vector<std::uint8_t> payload; };
struct FrameHeader { std::uint32_t magic{0}; std::uint32_t version{0}; MessageKind kind{MessageKind::ERROR}; std::uint32_t length{0}; std::uint32_t crc{0}; };

std::vector<std::uint8_t> encode_frame(MessageKind kind, std::span<const std::uint8_t> payload);
bool decode_frame(std::span<const std::uint8_t> bytes, Frame& out, std::string& err, std::size_t& consumed);

std::vector<std::uint8_t> serialize_ack(const AckResult& a);
bool deserialize_ack(std::span<const std::uint8_t> p, AckResult& a);
std::vector<std::uint8_t> serialize_error(DirectoryError e, const std::string& msg);
bool deserialize_error(std::span<const std::uint8_t> p, DirectoryError& e, std::string& msg);
std::vector<std::uint8_t> serialize_hello(const WorkerSession& s);
bool deserialize_hello(std::span<const std::uint8_t> p, WorkerSession& s);
std::vector<std::uint8_t> serialize_epoch_reply(CoordinatorEpoch epoch);
bool deserialize_epoch_reply(std::span<const std::uint8_t> p, CoordinatorEpoch& epoch);
std::vector<std::uint8_t> serialize_cache(const CacheDescriptor& c);
bool deserialize_cache(std::span<const std::uint8_t> p, CacheDescriptor& c);
std::vector<std::uint8_t> serialize_location(const LocationDescriptor& l);
bool deserialize_location(std::span<const std::uint8_t> p, LocationDescriptor& l);
std::vector<std::uint8_t> serialize_replica(const ReplicaDescriptor& r);
bool deserialize_replica(std::span<const std::uint8_t> p, ReplicaDescriptor& r);
std::vector<std::uint8_t> serialize_record(const DirectoryRecord& r);
bool deserialize_record(std::span<const std::uint8_t> p, DirectoryRecord& r);
std::vector<std::uint8_t> serialize_query(const DirectoryQuery& q);
bool deserialize_query(std::span<const std::uint8_t> p, DirectoryQuery& q);
std::vector<std::uint8_t> serialize_query_result(const QueryResult& qr);
bool deserialize_query_result(std::span<const std::uint8_t> p, QueryResult& qr);

struct UpdateHealthMsg { DirectoryRecordId id; Health health; AuthorityEnvelope env; };
std::vector<std::uint8_t> serialize_update_health(const UpdateHealthMsg& m);
bool deserialize_update_health(std::span<const std::uint8_t> p, UpdateHealthMsg& m);
struct UpdateRefreshMsg { DirectoryRecordId id; Freshness freshness; AuthorityEnvelope env; };
std::vector<std::uint8_t> serialize_update_freshness(const UpdateRefreshMsg& m);
bool deserialize_update_freshness(std::span<const std::uint8_t> p, UpdateRefreshMsg& m);
struct UpdateReachMsg { DirectoryRecordId id; Reachability reachability; AuthorityEnvelope env; };
std::vector<std::uint8_t> serialize_update_reachability(const UpdateReachMsg& m);
bool deserialize_update_reachability(std::span<const std::uint8_t> p, UpdateReachMsg& m);
struct UpdateIntegrityMsg { DirectoryRecordId id; Integrity integrity; AuthorityEnvelope env; };
std::vector<std::uint8_t> serialize_update_integrity(const UpdateIntegrityMsg& m);
bool deserialize_update_integrity(std::span<const std::uint8_t> p, UpdateIntegrityMsg& m);
struct RenewLeaseMsg { LeaseId lease_id; AuthorityEnvelope env; };
std::vector<std::uint8_t> serialize_renew_lease(const RenewLeaseMsg& m);
bool deserialize_renew_lease(std::span<const std::uint8_t> p, RenewLeaseMsg& m);
struct InvalidateMsg { std::uint8_t kind; std::uint64_t target; std::uint64_t generation; AuthorityEnvelope env; std::string reason; };
std::vector<std::uint8_t> serialize_invalidate(const InvalidateMsg& m);
bool deserialize_invalidate(std::span<const std::uint8_t> p, InvalidateMsg& m);
std::vector<std::uint8_t> serialize_tombstone(const TombstoneRecord& t);
bool deserialize_tombstone(std::span<const std::uint8_t> p, TombstoneRecord& t);
std::vector<std::uint8_t> serialize_path(const std::string& path);
bool deserialize_path(std::span<const std::uint8_t> p, std::string& path);

}  // namespace protocol
}  // namespace distributedcachedirectory
