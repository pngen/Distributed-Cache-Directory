#include "distributedcachedirectory/server.hpp"
#include <filesystem>
#include <thread>
#include <utility>

namespace distributedcachedirectory {
namespace server {

CoordinatorServer::CoordinatorServer() : dir_(std::make_shared<Directory>()) {}
CoordinatorServer::~CoordinatorServer() { stop(); }

bool CoordinatorServer::start(unsigned short port, const std::string& persist_path, std::string& err) {
  persist_path_ = persist_path;
  if (!persist_path_.empty() && std::filesystem::exists(persist_path_)) {
    auto r = dir_->recover(persist_path_);
    if (!r) { err = r.error_text; return false; }
  }
  if (!listener_.bind("0.0.0.0", port, err)) return false;
  port_ = listener_.port();
  running_ = true;
  return true;
}

unsigned short CoordinatorServer::port() const { return port_; }

void CoordinatorServer::run() {
  while (running_) {
    std::string err;
    auto s = listener_.accept(err);
    if (!s) { if (!running_) break; continue; }
    std::thread([this, stream = std::move(*s)]() mutable { handle_connection(std::move(stream)); }).detach();
  }
}

void CoordinatorServer::stop() {
  running_ = false;
  listener_.close();
}

void CoordinatorServer::respond_ack(transport::FrameChannel& ch, const AckResult& a) {
  std::string err; ch.send(static_cast<std::uint8_t>(protocol::MessageKind::ACK), protocol::serialize_ack(a), err);
}
void CoordinatorServer::respond_error(transport::FrameChannel& ch, DirectoryError e, const std::string& m) {
  std::string err; ch.send(static_cast<std::uint8_t>(protocol::MessageKind::ERROR), protocol::serialize_error(e, m), err);
}

void CoordinatorServer::handle_connection(transport::TcpStream&& stream) {
  transport::FrameChannel ch(std::move(stream));
  WorkerBootId boot; bool have_boot = false;
  while (true) {
    std::uint8_t kind = 0; std::vector<std::uint8_t> payload; std::string err;
    if (!ch.receive(kind, payload, err)) {
      if (have_boot) { std::lock_guard<std::mutex> lock(dir_mutex_); dir_->on_worker_loss(boot, "connection closed"); }
      break;
    }
    auto mk = static_cast<protocol::MessageKind>(kind);
    std::lock_guard<std::mutex> lock(dir_mutex_);
    switch (mk) {
      case protocol::MessageKind::HELLO: {
        WorkerSession s;
        if (!protocol::deserialize_hello(payload, s)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed hello"); break; }
        auto r = dir_->register_worker_session(s);
        if (!r) { respond_error(ch, r.error, r.error_text); break; }
        boot = s.boot; have_boot = true;
        std::string e2; ch.send(static_cast<std::uint8_t>(protocol::MessageKind::ACK), protocol::serialize_epoch_reply(dir_->epoch()), e2);
        break;
      }
      case protocol::MessageKind::REGISTER_CACHE: {
        CacheDescriptor c;
        if (!protocol::deserialize_cache(payload, c)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed cache"); break; }
        auto r = dir_->register_cache(c); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "cache registered"}); break;
      }
      case protocol::MessageKind::REGISTER_LOCATION: {
        LocationDescriptor l;
        if (!protocol::deserialize_location(payload, l)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed location"); break; }
        auto r = dir_->register_location(l); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "location registered"}); break;
      }
      case protocol::MessageKind::REGISTER_REPLICA: {
        ReplicaDescriptor rep;
        if (!protocol::deserialize_replica(payload, rep)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed replica"); break; }
        auto r = dir_->register_replica(rep); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "replica registered"}); break;
      }
      case protocol::MessageKind::REGISTER_ENTRY: {
        DirectoryRecord rec;
        if (!protocol::deserialize_record(payload, rec)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed record"); break; }
        auto r = dir_->register_entry(rec); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "entry registered"}); break;
      }
      case protocol::MessageKind::UPDATE_HEALTH: {
        protocol::UpdateHealthMsg m;
        if (!protocol::deserialize_update_health(payload, m)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed update"); break; }
        auto r = dir_->update_health(m.id, m.health, m.env); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "health updated"}); break;
      }
      case protocol::MessageKind::UPDATE_FRESHNESS: {
        protocol::UpdateRefreshMsg m;
        if (!protocol::deserialize_update_freshness(payload, m)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed update"); break; }
        auto r = dir_->update_freshness(m.id, m.freshness, m.env); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "freshness updated"}); break;
      }
      case protocol::MessageKind::UPDATE_REACHABILITY: {
        protocol::UpdateReachMsg m;
        if (!protocol::deserialize_update_reachability(payload, m)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed update"); break; }
        auto r = dir_->update_reachability(m.id, m.reachability, m.env); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "reachability updated"}); break;
      }
      case protocol::MessageKind::UPDATE_INTEGRITY: {
        protocol::UpdateIntegrityMsg m;
        if (!protocol::deserialize_update_integrity(payload, m)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed update"); break; }
        auto r = dir_->update_integrity(m.id, m.integrity, m.env); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "integrity updated"}); break;
      }
      case protocol::MessageKind::RENEW_LEASE: {
        protocol::RenewLeaseMsg m;
        if (!protocol::deserialize_renew_lease(payload, m)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed renew"); break; }
        auto r = dir_->renew_lease(m.lease_id, m.env); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "lease renewed"}); break;
      }
      case protocol::MessageKind::INVALIDATE: {
        protocol::InvalidateMsg m;
        if (!protocol::deserialize_invalidate(payload, m)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed invalidate"); break; }
        Result<InvalidateResult> r;
        switch (m.kind) {
          case 0: r = dir_->invalidate_state(StateId(m.target), StateGeneration(m.generation), m.env, m.reason); break;
          case 1: r = dir_->invalidate_cache(CacheId(m.target), m.env, m.reason); break;
          case 2: r = dir_->invalidate_replica(ReplicaId(m.target), m.env, m.reason); break;
          case 3: r = dir_->invalidate_location(LocationId(m.target), m.env, m.reason); break;
          case 4: r = dir_->invalidate_worker_boot(WorkerBootId(m.target), m.env, m.reason); break;
          default: respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "bad invalidation kind"); break;
        }
        if (!r.ok) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, r.value.message}); break;
      }
      case protocol::MessageKind::TOMBSTONE: {
        TombstoneRecord t;
        if (!protocol::deserialize_tombstone(payload, t)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed tombstone"); break; }
        auto r = dir_->tombstone(t); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "tombstone recorded"}); break;
      }
      case protocol::MessageKind::QUERY: {
        DirectoryQuery q;
        if (!protocol::deserialize_query(payload, q)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed query"); break; }
        auto r = dir_->query(q);
        std::string e2; ch.send(static_cast<std::uint8_t>(protocol::MessageKind::QUERY_RESULT), protocol::serialize_query_result(r.value), e2);
        break;
      }
      case protocol::MessageKind::SAVE: {
        std::string path; if (!protocol::deserialize_path(payload, path)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed path"); break; }
        auto r = dir_->save(path); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "saved"}); break;
      }
      case protocol::MessageKind::RECOVER: {
        std::string path; if (!protocol::deserialize_path(payload, path)) { respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "malformed path"); break; }
        auto r = dir_->recover(path); if (!r) { respond_error(ch, r.error, r.error_text); break; }
        respond_ack(ch, AckResult{true, "recovered"}); break;
      }
      default: respond_error(ch, DirectoryError::PROTOCOL_VIOLATION, "unknown message kind");
    }
  }
}

}  // namespace server
}  // namespace distributedcachedirectory
