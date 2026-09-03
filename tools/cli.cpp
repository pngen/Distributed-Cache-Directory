#include "distributedcachedirectory/directory.hpp"
#include "distributedcachedirectory/digest.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace dcd = distributedcachedirectory;

static unsigned long long u(const std::string& s) { return std::strtoull(s.c_str(), nullptr, 0); }
static int in(const std::string& s) { return std::atoi(s.c_str()); }
static void usage(const char* p) {
  std::printf("usage: %s <command> [args...]\n", p);
  std::printf("commands: cache-register | entry-register | show | query | replicas | locations | health | reachability | lease-renew | invalidate | tombstone | history | explain | simulate | save | recover | benchmark\n");
}

int main(int argc, char** argv) {
  if (argc < 2) { usage(argv[0]); return 1; }
  std::string cmd = argv[1];
  dcd::Directory dir;
  if (cmd == "simulate") {
    std::printf("simulate: synthetic distributed scenarios (provenance=SYNTHETIC)\n");
    // Scenario 1: same state on two nodes.
    dcd::WorkerSession s1; s1.boot = dcd::WorkerBootId(10); s1.worker = dcd::WorkerId(1); s1.node = dcd::NodeId(1); s1.tag = "wa";
    dcd::WorkerSession s2; s2.boot = dcd::WorkerBootId(11); s2.worker = dcd::WorkerId(2); s2.node = dcd::NodeId(2); s2.tag = "wb";
    dir.register_worker_session(s1); dir.register_worker_session(s2);
    dcd::CacheDescriptor c1; c1.cache_id = dcd::CacheId(1); c1.kind = dcd::CacheKind::NODE_LOCAL; c1.node = s1.node; c1.worker = s1.worker; c1.worker_boot = s1.boot; c1.domain = dcd::MemoryDomain::LOCAL_FILESYSTEM; c1.authority.epoch = dir.epoch(); c1.authority.boot = s1.boot; c1.authority.fence = 1;
    dcd::CacheDescriptor c2; c2.cache_id = dcd::CacheId(2); c2.kind = dcd::CacheKind::NODE_LOCAL; c2.node = s2.node; c2.worker = s2.worker; c2.worker_boot = s2.boot; c2.domain = dcd::MemoryDomain::LOCAL_FILESYSTEM; c2.authority.epoch = dir.epoch(); c2.authority.boot = s2.boot; c2.authority.fence = 1;
    dir.register_cache(c1); dir.register_cache(c2);
    dcd::LocationDescriptor l1; l1.location_id = dcd::LocationId(1); l1.node = s1.node; l1.worker = s1.worker; l1.worker_boot = s1.boot; l1.domain = c1.domain; l1.locator.key = "fs:/node1/x"; l1.authority = c1.authority; dir.register_location(l1);
    dcd::LocationDescriptor l2; l2.location_id = dcd::LocationId(2); l2.node = s2.node; l2.worker = s2.worker; l2.worker_boot = s2.boot; l2.domain = c2.domain; l2.locator.key = "fs:/node2/x"; l2.authority = c2.authority; dir.register_location(l2);
    dcd::ReplicaDescriptor r1; r1.replica_id = dcd::ReplicaId(1); r1.location = l1.location_id; r1.cache = c1.cache_id; r1.entry = dcd::CacheEntryId(1); r1.authority = c1.authority; dir.register_replica(r1);
    dcd::ReplicaDescriptor r2; r2.replica_id = dcd::ReplicaId(2); r2.location = l2.location_id; r2.cache = c2.cache_id; r2.entry = dcd::CacheEntryId(1); r2.authority = c2.authority; dir.register_replica(r2);
    dcd::DirectoryRecord rec1; rec1.state_id = dcd::StateId(100); rec1.state_generation = dcd::StateGeneration(1); rec1.kind = dcd::StateKind::TENSOR_STATE; rec1.cache = c1.cache_id; rec1.entry = dcd::CacheEntryId(1); rec1.replica = r1.replica_id; rec1.location = l1.location_id; rec1.node = s1.node; rec1.worker = s1.worker; rec1.worker_boot = s1.boot; rec1.domain = c1.domain; rec1.logical_bytes = 4096; rec1.reachability = dcd::Reachability::REACHABLE; rec1.health = dcd::Health::HEALTHY; rec1.freshness = dcd::Freshness::CURRENT; rec1.integrity = dcd::Integrity::VERIFIED; rec1.authority = c1.authority; dir.register_entry(rec1);
    dcd::DirectoryRecord rec2; rec2.state_id = dcd::StateId(100); rec2.state_generation = dcd::StateGeneration(1); rec2.kind = dcd::StateKind::TENSOR_STATE; rec2.cache = c2.cache_id; rec2.entry = dcd::CacheEntryId(1); rec2.replica = r2.replica_id; rec2.location = l2.location_id; rec2.node = s2.node; rec2.worker = s2.worker; rec2.worker_boot = s2.boot; rec2.domain = c2.domain; rec2.logical_bytes = 4096; rec2.reachability = dcd::Reachability::REACHABLE; rec2.health = dcd::Health::HEALTHY; rec2.freshness = dcd::Freshness::CURRENT; rec2.integrity = dcd::Integrity::VERIFIED; rec2.authority = c2.authority; dir.register_entry(rec2);
    dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(100); q.current_only = true;
    auto res = dir.query(q);
    std::printf("scenario 1 (same state on two nodes, SYNTHETIC): outcome=%s candidates=%u\n", dcd::to_string(res.value.outcome), static_cast<unsigned>(res.value.candidates.size()));
    return 0;
  }
  // index-based args: 0-based positional after command.
  int idx = 2;
  auto arg = [&]()->std::string { return idx < argc ? std::string(argv[idx++]) : std::string(); };
  dcd::WorkerBootId boot; dcd::WorkerId worker; dcd::NodeId node;
  if (cmd == "cache-register") {
    dcd::WorkerSession s; s.boot = dcd::WorkerBootId(u(arg())); s.worker = dcd::WorkerId(u(arg())); s.node = dcd::NodeId(u(arg())); s.tag = arg();
    auto sr = dir.register_worker_session(s); if (!sr) { std::printf("ERR %s\n", sr.error_text.c_str()); return 1; }
    dcd::CacheDescriptor c; c.cache_id = dcd::CacheId(u(arg())); c.kind = static_cast<dcd::CacheKind>(in(arg())); c.node = s.node; c.worker = s.worker; c.worker_boot = s.boot;
    c.domain = static_cast<dcd::MemoryDomain>(in(arg())); c.capacity = u(arg()); c.capacity_known = true; c.reachability = dcd::Reachability::REACHABLE; c.health = dcd::Health::HEALTHY; c.freshness = dcd::Freshness::CURRENT;
    c.generation = dcd::CacheGeneration(1); c.authority.epoch = dir.epoch(); c.authority.boot = s.boot; c.authority.fence = 1;
    auto r = dir.register_cache(c); if (!r) { std::printf("ERR %s\n", r.error_text.c_str()); return 1; }
    std::printf("OK cache %llu registered\n", c.cache_id.value());
  } else if (cmd == "entry-register") {
    dcd::WorkerSession s; s.boot = dcd::WorkerBootId(u(arg())); s.worker = dcd::WorkerId(u(arg())); s.node = dcd::NodeId(u(arg())); s.tag = arg();
    dir.register_worker_session(s);
    dcd::CacheDescriptor c; c.cache_id = dcd::CacheId(u(arg())); c.kind = dcd::CacheKind::PROCESS_LOCAL; c.node = s.node; c.worker = s.worker; c.worker_boot = s.boot; c.domain = static_cast<dcd::MemoryDomain>(in(arg())); c.authority.epoch = dir.epoch(); c.authority.boot = s.boot; c.authority.fence = 1;
    dir.register_cache(c);
    dcd::LocationDescriptor l; l.location_id = dcd::LocationId(u(arg())); l.node = s.node; l.worker = s.worker; l.worker_boot = s.boot; l.domain = c.domain; l.locator.key = arg(); l.logical_bytes = u(arg()); l.authority = c.authority;
    dir.register_location(l);
    dcd::ReplicaDescriptor rep; rep.replica_id = dcd::ReplicaId(u(arg())); rep.location = l.location_id; rep.cache = c.cache_id; rep.entry = dcd::CacheEntryId(u(arg())); rep.authority = c.authority;
    dir.register_replica(rep);
    dcd::DirectoryRecord rec; rec.state_id = dcd::StateId(u(arg())); rec.state_generation = dcd::StateGeneration(u(arg())); rec.kind = static_cast<dcd::StateKind>(in(arg())); rec.cache = c.cache_id; rec.entry = rep.entry; rec.replica = rep.replica_id; rec.location = l.location_id; rec.node = s.node; rec.worker = s.worker; rec.worker_boot = s.boot; rec.domain = c.domain; rec.logical_bytes = l.logical_bytes; rec.reachability = dcd::Reachability::REACHABLE; rec.health = dcd::Health::HEALTHY; rec.freshness = dcd::Freshness::CURRENT; rec.integrity = dcd::Integrity::VERIFIED; rec.authority = c.authority;
    auto r = dir.register_entry(rec); if (!r) { std::printf("ERR %s\n", r.error_text.c_str()); return 1; }
    std::printf("OK entry StateId %llu Gen %llu record %llu\n", rec.state_id.value(), rec.state_generation.value(), r.value.record_id.value());
  } else if (cmd == "show") {
    for (auto& r : dir.records()) std::printf("record %llu StateId %llu Gen %llu Replica %llu Loc %llu dom %s lifecycle %s current %d\n", r.record_id.value(), r.state_id.value(), r.state_generation.value(), r.replica.value(), r.location.value(), dcd::to_string(r.domain), dcd::to_string(r.lifecycle), r.current?1:0);
  } else if (cmd == "query") {
    dcd::DirectoryQuery q; q.query_id = dcd::QueryId(1); q.exact_state = true; q.state = dcd::StateId(u(arg())); q.requester_process_tag = arg();
    auto res = dir.query(q);
    std::printf("outcome %s candidates %u\n", dcd::to_string(res.value.outcome), static_cast<unsigned>(res.value.candidates.size()));
    for (auto& c : res.value.candidates) std::printf("  replica %s stategen %s dom %s integ %s fresh %s reach %s score %f\n", c.record.replica.str().c_str(), c.record.state_generation.str().c_str(), dcd::to_string(c.record.domain), dcd::to_string(c.record.integrity), dcd::to_string(c.record.freshness), dcd::to_string(c.record.reachability), c.score);
  } else if (cmd == "replicas") { for (auto& kv : dir.replicas()) std::printf("replica %llu loc %llu cache %llu health %s\n", kv.first.value(), kv.second.location.value(), kv.second.cache.value(), dcd::to_string(kv.second.health)); }
  else if (cmd == "locations") { for (auto& kv : dir.locations()) std::printf("location %llu dom %s reach %s fresh %s integrity %s\n", kv.first.value(), dcd::to_string(kv.second.domain), dcd::to_string(kv.second.reachability), dcd::to_string(kv.second.freshness), dcd::to_string(kv.second.integrity)); }
  else if (cmd == "health") { for (auto& r : dir.records()) std::printf("record %llu health %s\n", r.record_id.value(), dcd::to_string(r.health)); }
  else if (cmd == "reachability") { for (auto& r : dir.records()) std::printf("record %llu reachability %s\n", r.record_id.value(), dcd::to_string(r.reachability)); }
  else if (cmd == "lease-renew") { dcd::WorkerBootId b(u(arg())); unsigned long long f = u(arg()); dcd::AuthorityEnvelope e; e.epoch = dir.epoch(); e.boot = b; e.fence = f; auto r = dir.renew_lease(dcd::LeaseId(u(arg())), e); if (!r) std::printf("ERR %s\n", r.error_text.c_str()); else std::printf("OK lease renewed gen %llu\n", r.value.generation.value()); }
  else if (cmd == "invalidate") { dcd::WorkerBootId b(u(arg())); dcd::AuthorityEnvelope e; e.epoch = dir.epoch(); e.boot = b; e.fence = u(arg()); auto r = dir.invalidate_state(dcd::StateId(u(arg())), dcd::StateGeneration(u(arg())), e, arg()); if (!r) std::printf("ERR %s\n", r.error_text.c_str()); else std::printf("OK invalidated %u\n", r.value.affected_records); }
  else if (cmd == "tombstone") { dcd::TombstoneRecord t; t.tombstone_id = dcd::TombstoneId(1); t.target.kind = dcd::TombstoneKind::STATE; t.target.state = dcd::StateId(u(arg())); t.target.generation_floor = u(arg()); t.epoch = dir.epoch(); t.worker_boot = dcd::WorkerBootId(u(arg())); t.authority_generation = dcd::DirectoryGeneration(1); t.reason = arg(); auto r = dir.tombstone(t); if (!r) std::printf("ERR %s\n", r.error_text.c_str()); else std::printf("OK tombstone\n"); }
  else if (cmd == "history") { for (auto& r : dir.records()) if (!r.current) std::printf("historical record %llu StateId %llu Gen %llu lifecycle %s\n", r.record_id.value(), r.state_id.value(), r.state_generation.value(), dcd::to_string(r.lifecycle)); }
  else if (cmd == "explain") { auto res = dir.explain_recovery(); for (auto& e : res) std::printf("[%s] %s\n", e.kind.c_str(), e.text.c_str()); }
  else if (cmd == "save") { auto r = dir.save(arg()); if (!r) std::printf("ERR %s\n", r.error_text.c_str()); else std::printf("OK saved\n"); }
  else if (cmd == "recover") { auto r = dir.recover(arg()); if (!r) std::printf("ERR %s\n", r.error_text.c_str()); else std::printf("OK recovered\n"); }
  else if (cmd == "benchmark") { std::printf("OK benchmark used via dedicated benchmark executable\n"); }
  else { usage(argv[0]); return 1; }
  return 0;
}
