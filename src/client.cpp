#include "distributedcachedirectory/client.hpp"
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#undef ERROR
#endif

namespace distributedcachedirectory {
namespace client {

DcdClient::DcdClient(transport::TcpStream&& stream) : ch_(std::move(stream)) {}

DcdClient DcdClient::connect(const std::string& host, unsigned short port, std::string& err) {
  if (!transport::winsock_init()) { err = "winsock init failed"; return DcdClient(); }
#ifdef _WIN32
  sockaddr_in hint{}; hint.sin_family = AF_INET; hint.sin_port = htons(port);
  if (host == "localhost") hint.sin_addr.s_addr = htonl(INADDR_LOOPBACK); else ::inet_pton(AF_INET, host.c_str(), &hint.sin_addr);
  SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) { err = "socket failed"; return DcdClient(); }
  if (::connect(s, reinterpret_cast<sockaddr*>(&hint), sizeof(sockaddr_in)) == SOCKET_ERROR) { ::closesocket(s); err = "connect failed"; return DcdClient(); }
  return DcdClient(transport::TcpStream(static_cast<unsigned long long>(s)));
#else
  err = "not supported on this platform"; return DcdClient();
#endif
}

bool DcdClient::connected() const { return ch_.valid(); }
void DcdClient::close() { ch_.close(); }
bool DcdClient::send_recv(std::uint8_t kind, const std::vector<std::uint8_t>& payload, protocol::Frame& resp, std::string& err) {
  if (!ch_.send(kind, payload, err)) return false;
  std::uint8_t k = 0; std::vector<std::uint8_t> p;
  if (!ch_.receive(k, p, err)) return false;
  resp.kind = static_cast<protocol::MessageKind>(k); resp.payload = std::move(p);
  return true;
}
bool DcdClient::send_wait_ack(std::uint8_t kind, const std::vector<std::uint8_t>& payload, std::string& err) {
  protocol::Frame resp;
  if (!send_recv(kind, payload, resp, err)) return false;
  if (resp.kind == protocol::MessageKind::ACK) { AckResult a; if (!protocol::deserialize_ack(resp.payload, a)) { err = "malformed ack"; return false; } if (!a.ok) { err = a.message; last_error_ = DirectoryError::INTERNAL; last_error_text_ = a.message; return false; } return true; }
  if (resp.kind == protocol::MessageKind::ERROR) { DirectoryError e; std::string m; protocol::deserialize_error(resp.payload, e, m); last_error_ = e; last_error_text_ = m; err = m; return false; }
  err = "unexpected response"; return false;
}
bool DcdClient::hello(const WorkerSession& session, CoordinatorEpoch& epoch, std::string& err) {
  protocol::Frame resp;
  if (!send_recv(static_cast<std::uint8_t>(protocol::MessageKind::HELLO), protocol::serialize_hello(session), resp, err)) return false;
  if (resp.kind != protocol::MessageKind::ACK) { if (resp.kind == protocol::MessageKind::ERROR) { DirectoryError e; std::string m; protocol::deserialize_error(resp.payload, e, m); last_error_=e; last_error_text_=m; err=m; } else err="hello rejected"; return false; }
  if (!protocol::deserialize_epoch_reply(resp.payload, epoch)) { err = "malformed hello reply"; return false; }
  return true;
}
bool DcdClient::register_cache(const CacheDescriptor& cache, std::string& err) { return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::REGISTER_CACHE), protocol::serialize_cache(cache), err); }
bool DcdClient::register_location(const LocationDescriptor& loc, std::string& err) { return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::REGISTER_LOCATION), protocol::serialize_location(loc), err); }
bool DcdClient::register_replica(const ReplicaDescriptor& rep, std::string& err) { return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::REGISTER_REPLICA), protocol::serialize_replica(rep), err); }
bool DcdClient::register_entry(const DirectoryRecord& record, std::string& err) { return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::REGISTER_ENTRY), protocol::serialize_record(record), err); }
bool DcdClient::query(const DirectoryQuery& q, QueryResult& result, std::string& err) {
  protocol::Frame resp;
  if (!send_recv(static_cast<std::uint8_t>(protocol::MessageKind::QUERY), protocol::serialize_query(q), resp, err)) return false;
  if (resp.kind != protocol::MessageKind::QUERY_RESULT) { if (resp.kind == protocol::MessageKind::ERROR) { DirectoryError e; std::string m; protocol::deserialize_error(resp.payload, e, m); last_error_=e; last_error_text_=m; err=m; } else err="query rejected"; return false; }
  if (!protocol::deserialize_query_result(resp.payload, result)) { err = "malformed query result"; return false; }
  return true;
}
bool DcdClient::update_health(DirectoryRecordId id, Health h, const AuthorityEnvelope& env, std::string& err) { protocol::UpdateHealthMsg m{id, h, env}; return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::UPDATE_HEALTH), protocol::serialize_update_health(m), err); }
bool DcdClient::update_freshness(DirectoryRecordId id, Freshness f, const AuthorityEnvelope& env, std::string& err) { protocol::UpdateRefreshMsg m{id, f, env}; return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::UPDATE_FRESHNESS), protocol::serialize_update_freshness(m), err); }
bool DcdClient::update_reachability(DirectoryRecordId id, Reachability r, const AuthorityEnvelope& env, std::string& err) { protocol::UpdateReachMsg m{id, r, env}; return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::UPDATE_REACHABILITY), protocol::serialize_update_reachability(m), err); }
bool DcdClient::update_integrity(DirectoryRecordId id, Integrity i, const AuthorityEnvelope& env, std::string& err) { protocol::UpdateIntegrityMsg m{id, i, env}; return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::UPDATE_INTEGRITY), protocol::serialize_update_integrity(m), err); }
bool DcdClient::renew_lease(LeaseId id, const AuthorityEnvelope& env, std::string& err) { protocol::RenewLeaseMsg m{id, env}; return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::RENEW_LEASE), protocol::serialize_renew_lease(m), err); }
bool DcdClient::invalidate(const protocol::InvalidateMsg& msg, std::string& err) { return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::INVALIDATE), protocol::serialize_invalidate(msg), err); }
bool DcdClient::tombstone(const TombstoneRecord& t, std::string& err) { return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::TOMBSTONE), protocol::serialize_tombstone(t), err); }
bool DcdClient::save(const std::string& path, std::string& err) { return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::SAVE), protocol::serialize_path(path), err); }
bool DcdClient::recover(const std::string& path, std::string& err) { return send_wait_ack(static_cast<std::uint8_t>(protocol::MessageKind::RECOVER), protocol::serialize_path(path), err); }

}  // namespace client
}  // namespace distributedcachedirectory
