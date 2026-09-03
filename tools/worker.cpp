#include "distributedcachedirectory/client.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace dcd = distributedcachedirectory;

static unsigned long long u(const std::string& s) { return std::strtoull(s.c_str(), nullptr, 0); }
static int in(const std::string& s) { return std::atoi(s.c_str()); }
static std::vector<std::string> tok(const std::string& line) {
  std::vector<std::string> v; std::istringstream is(line); std::string t;
  while (is >> t) v.push_back(t);
  return v;
}
static void ok(const std::string& m) { std::printf("OK %s\n", m.c_str()); std::fflush(stdout); }
static void er(const std::string& m, dcd::DirectoryError e, const std::string& text) {
  std::printf("ERR %s %s %s\n", m.c_str(), dcd::to_string(e), text.c_str()); std::fflush(stdout);
}

int main(int argc, char** argv) {
  std::string host = "127.0.0.1"; unsigned short port = 4000;
  dcd::WorkerBootId boot; dcd::WorkerId worker; dcd::NodeId node; std::string tag = "worker"; std::string epov;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&]() -> std::string { return (i + 1 < argc) ? std::string(argv[++i]) : std::string(); };
    if (a == "--coordinator") { std::string v = need(); std::size_t c = v.rfind(':'); if (c != std::string::npos) { host = v.substr(0, c); port = static_cast<unsigned short>(u(v.substr(c + 1))); } }
    else if (a == "--boot") boot = dcd::WorkerBootId(u(need()));
    else if (a == "--worker") worker = dcd::WorkerId(u(need()));
    else if (a == "--node") node = dcd::NodeId(u(need()));
    else if (a == "--tag") tag = need();
    else if (a == "--epoch") epov = need();
  }
  std::string err;
  dcd::client::DcdClient cli = dcd::client::DcdClient::connect(host, port, err);
  if (!cli.connected()) { std::printf("ERR CONNECT %s\n", err.c_str()); return 1; }
  dcd::CoordinatorEpoch epoch;
  dcd::WorkerSession sess; sess.boot = boot; sess.worker = worker; sess.node = node; sess.tag = tag;
  if (!cli.hello(sess, epoch, err)) { std::printf("ERR HELLO %s\n", err.c_str()); return 1; }
  if (!epov.empty()) epoch = dcd::CoordinatorEpoch(u(epov));
  std::printf("READY epoch=%llu\n", static_cast<unsigned long long>(epoch.value())); std::fflush(stdout);

  unsigned long long fence = 1;
  auto env = [&](unsigned long long f) { dcd::AuthorityEnvelope e; e.epoch = epoch; e.boot = boot; e.directory_generation = dcd::DirectoryGeneration(0); e.fence = f; return e; };
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty() || line[0] == '#') continue;
    auto t = tok(line);
    if (t.empty()) continue;
    const std::string& cmd = t[0];
    if (cmd == "quit" || cmd == "exit") break;
    if (cmd == "hold") { for (;;) std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
    try {
      if (cmd == "register-cache") {
        dcd::CacheDescriptor c; c.cache_id = dcd::CacheId(u(t.at(1))); c.kind = static_cast<dcd::CacheKind>(in(t.at(2))); c.node = node; c.worker = worker; c.worker_boot = boot;
        c.domain = static_cast<dcd::MemoryDomain>(in(t.at(3))); c.capacity = t.size()>4?u(t.at(4)):0; c.capacity_known = t.size()>4; c.reachability = dcd::Reachability::REACHABLE; c.health = dcd::Health::HEALTHY; c.freshness = dcd::Freshness::CURRENT;
        c.provenance = dcd::ProvenanceId(1); c.generation = dcd::CacheGeneration(fence); c.authority = env(fence);
        if (cli.register_cache(c, err)) ok("cache-registered"); else er("cache", cli.last_error(), cli.last_error_text());
        ++fence;
      } else if (cmd == "register-location") {
        dcd::LocationDescriptor l; l.location_id = dcd::LocationId(u(t.at(1))); l.domain = static_cast<dcd::MemoryDomain>(in(t.at(2))); l.node = node; l.worker = worker; l.worker_boot = boot;
        if (t.size()>3 && t.at(3) != "0") l.device = dcd::DeviceId(u(t.at(3))); l.locator.key = t.size()>4 ? t.at(4) : ("loc:" + t.at(1)); l.logical_bytes = t.size()>5 ? u(t.at(5)) : 1024;
        l.locality = dcd::LocalityClass::SAME_NODE; l.reachability = dcd::Reachability::REACHABLE; l.health = dcd::Health::HEALTHY; l.freshness = dcd::Freshness::CURRENT; l.integrity = dcd::Integrity::VERIFIED;
        l.provenance = dcd::ProvenanceId(1); l.generation = dcd::LocationGeneration(fence); l.authority = env(fence); l.process_tag = tag;
        if (cli.register_location(l, err)) ok("location-registered"); else er("location", cli.last_error(), cli.last_error_text());
        ++fence;
      } else if (cmd == "register-replica") {
        dcd::ReplicaDescriptor r; r.replica_id = dcd::ReplicaId(u(t.at(1))); r.location = dcd::LocationId(u(t.at(2))); r.cache = dcd::CacheId(u(t.at(3))); r.entry = dcd::CacheEntryId(u(t.at(4)));
        r.generation = dcd::ReplicaGeneration(t.size()>5?u(t.at(5)):fence); r.reachability = dcd::Reachability::REACHABLE; r.health = dcd::Health::HEALTHY; r.integrity = dcd::Integrity::VERIFIED; r.freshness = dcd::Freshness::CURRENT; r.authority = env(t.size()>5?u(t.at(5)):fence);
        if (cli.register_replica(r, err)) ok("replica-registered"); else er("replica", cli.last_error(), cli.last_error_text());
        if (t.size()>5) fence = u(t.at(5)); ++fence;
      } else if (cmd == "register-entry") {
        dcd::DirectoryRecord r; r.state_id = dcd::StateId(u(t.at(1))); r.state_generation = dcd::StateGeneration(u(t.at(2))); r.kind = static_cast<dcd::StateKind>(in(t.at(3)));
        r.cache = dcd::CacheId(u(t.at(4))); r.entry = dcd::CacheEntryId(u(t.at(5))); r.replica = dcd::ReplicaId(u(t.at(6))); r.location = dcd::LocationId(u(t.at(7)));
        r.node = node; r.worker = worker; r.worker_boot = boot; if (t.size()>8 && t.at(8) != "0") r.device = dcd::DeviceId(u(t.at(8)));
        r.domain = static_cast<dcd::MemoryDomain>(in(t.at(9))); r.logical_bytes = u(t.at(10));
        if (t.size()>11 && !t.at(11).empty() && t.at(11) != "-") { r.content_digest_known = true; }
        r.location_generation = dcd::LocationGeneration(t.size()>12?u(t.at(12)):1); r.replica_generation = dcd::ReplicaGeneration(t.size()>13?u(t.at(13)):1); r.entry_generation = dcd::EntryGeneration(t.size()>14?u(t.at(14)):1);
        r.health = dcd::Health::HEALTHY; r.freshness = dcd::Freshness::CURRENT; r.integrity = dcd::Integrity::VERIFIED; r.reachability = dcd::Reachability::REACHABLE;
        r.provenance = dcd::ProvenanceId(1); r.authority = env(t.size()>15?u(t.at(15)):fence);
        if (cli.register_entry(r, err)) ok("entry-registered"); else er("entry", cli.last_error(), cli.last_error_text());
        if (t.size()>15) fence = u(t.at(15)); else ++fence;
      } else if (cmd == "query") {
        dcd::DirectoryQuery q; q.query_id = dcd::QueryId(1); q.exact_state = true; q.state = dcd::StateId(u(t.at(1))); q.requester_process_tag = tag;
        if (t.size()>2) q.requester_node = dcd::NodeId(u(t.at(2)));
        dcd::QueryResult res;
        if (!cli.query(q, res, err)) er("query", cli.last_error(), cli.last_error_text());
        else { std::printf("RESULT outcome=%s count=%u\n", dcd::to_string(res.outcome), static_cast<unsigned>(res.candidates.size())); for (auto& c : res.candidates) std::printf("  cand replica=%s score=%f integ=%s fresh=%s reach=%s\n", c.record.replica.str().c_str(), c.score, dcd::to_string(c.record.integrity), dcd::to_string(c.record.freshness), dcd::to_string(c.record.reachability)); std::fflush(stdout); }
      } else if (cmd == "update-health") { unsigned long long f = t.size()>3?u(t.at(3)):fence; if (cli.update_health(dcd::DirectoryRecordId(u(t.at(1))), static_cast<dcd::Health>(in(t.at(2))), env(f), err)) ok("health"); else er("health", cli.last_error(), cli.last_error_text()); fence = f + 1; }
      else if (cmd == "update-reachability") { unsigned long long f = t.size()>3?u(t.at(3)):fence; if (cli.update_reachability(dcd::DirectoryRecordId(u(t.at(1))), static_cast<dcd::Reachability>(in(t.at(2))), env(f), err)) ok("reach"); else er("reach", cli.last_error(), cli.last_error_text()); fence = f + 1; }
      else if (cmd == "update-integrity") { unsigned long long f = t.size()>3?u(t.at(3)):fence; if (cli.update_integrity(dcd::DirectoryRecordId(u(t.at(1))), static_cast<dcd::Integrity>(in(t.at(2))), env(f), err)) ok("integrity"); else er("integrity", cli.last_error(), cli.last_error_text()); fence = f + 1; }
      else if (cmd == "renew-lease") { unsigned long long f = t.size()>2?u(t.at(2)):fence; if (cli.renew_lease(dcd::LeaseId(u(t.at(1))), env(f), err)) ok("lease"); else er("lease", cli.last_error(), cli.last_error_text()); fence = f + 1; }
      else if (cmd == "invalidate") { dcd::protocol::InvalidateMsg m; m.kind = static_cast<std::uint8_t>(in(t.at(1))); m.target = u(t.at(2)); m.generation = t.size()>3?u(t.at(3)):0; m.env = env(t.size()>4?u(t.at(4)):fence); m.reason = t.size()>5?t.at(5):"invalidate"; if (cli.invalidate(m, err)) ok("invalidate"); else er("invalidate", cli.last_error(), cli.last_error_text()); ++fence; }
      else if (cmd == "tombstone") { dcd::TombstoneRecord tb; tb.tombstone_id = dcd::TombstoneId(u(t.at(1))); tb.target.kind = dcd::TombstoneKind::STATE; tb.target.state = dcd::StateId(u(t.at(2))); tb.target.generation_floor = u(t.at(3)); tb.epoch = epoch; tb.worker_boot = boot; tb.authority_generation = dcd::DirectoryGeneration(fence); tb.reason = t.size()>4?t.at(4):"tombstone"; tb.timestamp_ns = dcd::wall_clock_ns(); if (cli.tombstone(tb, err)) ok("tombstone"); else er("tombstone", cli.last_error(), cli.last_error_text()); ++fence; }
      else if (cmd == "save") { if (cli.save(t.at(1), err)) ok("saved"); else er("save", cli.last_error(), cli.last_error_text()); }
      else if (cmd == "recover") { if (cli.recover(t.at(1), err)) ok("recovered"); else er("recover", cli.last_error(), cli.last_error_text()); }
      else er("unknown", dcd::DirectoryError::INTERNAL, cmd);
    } catch (const std::exception& ex) { er("exception", dcd::DirectoryError::INTERNAL, ex.what()); }
  }
  cli.close();
  return 0;
}
