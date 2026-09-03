#pragma once
#include "directory.hpp"
#include "transport.hpp"
#include "protocol.hpp"
#include <string>
#include <vector>

namespace distributedcachedirectory {
namespace client {

// A worker-side client to a coordinator that owns a Directory over framed TCP.
class DcdClient {
 public:
  DcdClient() = default;
  explicit DcdClient(transport::TcpStream&& stream);

  static DcdClient connect(const std::string& host, unsigned short port, std::string& err);

  bool connected() const;
  void close();

  // HELLO. Returns the coordinator's current epoch.
  bool hello(const WorkerSession& session, CoordinatorEpoch& epoch, std::string& err);
  bool register_cache(const CacheDescriptor& cache, std::string& err);
  bool register_location(const LocationDescriptor& loc, std::string& err);
  bool register_replica(const ReplicaDescriptor& rep, std::string& err);
  bool register_entry(const DirectoryRecord& record, std::string& err);
  bool query(const DirectoryQuery& q, QueryResult& result, std::string& err);
  bool update_health(DirectoryRecordId id, Health h, const AuthorityEnvelope& env, std::string& err);
  bool update_freshness(DirectoryRecordId id, Freshness f, const AuthorityEnvelope& env, std::string& err);
  bool update_reachability(DirectoryRecordId id, Reachability r, const AuthorityEnvelope& env, std::string& err);
  bool update_integrity(DirectoryRecordId id, Integrity i, const AuthorityEnvelope& env, std::string& err);
  bool renew_lease(LeaseId id, const AuthorityEnvelope& env, std::string& err);
  bool invalidate(const protocol::InvalidateMsg& msg, std::string& err);
  bool tombstone(const TombstoneRecord& t, std::string& err);
  bool save(const std::string& path, std::string& err);
  bool recover(const std::string& path, std::string& err);

  const DirectoryError last_error() const { return last_error_; }
  const std::string& last_error_text() const { return last_error_text_; }

 private:
  bool send_recv(std::uint8_t kind, const std::vector<std::uint8_t>& payload, protocol::Frame& resp, std::string& err);
  bool send_wait_ack(std::uint8_t kind, const std::vector<std::uint8_t>& payload, std::string& err);

  transport::FrameChannel ch_;
  DirectoryError last_error_{DirectoryError::NONE};
  std::string last_error_text_;
};

}  // namespace client
}  // namespace distributedcachedirectory
