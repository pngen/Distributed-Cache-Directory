#pragma once
#include "distributedcachedirectory/directory.hpp"
#include <string>

namespace dcd = distributedcachedirectory;

// Test scaffolding: builds a Directory and registers one state with the given
// replica topology quickly, returning the record id and the creating boot.
namespace tst {

struct Topology {
  dcd::Directory dir;
  dcd::WorkerBootId boot;
  dcd::WorkerId worker;
  dcd::NodeId node;
  dcd::CacheId cache;
  dcd::CacheEntryId entry;
  dcd::ReplicaId replica;
  dcd::LocationId location;
  dcd::StateId state;
  dcd::StateGeneration gen;
};

inline dcd::AuthorityEnvelope env(const dcd::Directory& d, dcd::WorkerBootId boot, unsigned long long fence) {
  dcd::AuthorityEnvelope e;
  e.epoch = d.epoch(); e.boot = boot; e.directory_generation = d.generation(); e.fence = fence;
  return e;
}

inline dcd::DirectoryRecord make_record(const Topology& t) {
  dcd::DirectoryRecord r;
  r.state_id = t.state; r.state_generation = t.gen; r.kind = dcd::StateKind::TENSOR_STATE;
  r.cache = t.cache; r.entry = t.entry; r.replica = t.replica; r.location = t.location;
  r.node = t.node; r.worker = t.worker; r.worker_boot = t.boot; r.device = dcd::DeviceId(0);
  r.domain = dcd::MemoryDomain::HOST_MEMORY; r.logical_bytes = 4096; r.physical_bytes = 4096; r.physical_bytes_known = true;
  r.reachability = dcd::Reachability::REACHABLE; r.health = dcd::Health::HEALTHY;
  r.freshness = dcd::Freshness::CURRENT; r.integrity = dcd::Integrity::VERIFIED;
  r.authority = env(t.dir, t.boot, 1);
  return r;
}

// Register a worker, cache, location, replica, and a single current record.
inline Topology register_one(dcd::StateId state, dcd::StateGeneration gen, const std::string& tag = "w") {
  Topology t;
  t.boot = dcd::WorkerBootId(100); t.worker = dcd::WorkerId(1); t.node = dcd::NodeId(1);
  t.cache = dcd::CacheId(1); t.entry = dcd::CacheEntryId(1); t.replica = dcd::ReplicaId(1);
  t.location = dcd::LocationId(1); t.state = state; t.gen = gen;
  dcd::WorkerSession s; s.boot = t.boot; s.worker = t.worker; s.node = t.node; s.tag = tag;
  t.dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id = t.cache; c.kind = dcd::CacheKind::PROCESS_LOCAL; c.node = t.node; c.worker = t.worker; c.worker_boot = t.boot;
  c.domain = dcd::MemoryDomain::HOST_MEMORY; c.capacity = 1024; c.capacity_known = true;
  c.reachability = dcd::Reachability::REACHABLE; c.health = dcd::Health::HEALTHY; c.freshness = dcd::Freshness::CURRENT;
  c.authority = env(t.dir, t.boot, 1);
  t.dir.register_cache(c);
  dcd::LocationDescriptor l; l.location_id = t.location; l.node = t.node; l.worker = t.worker; l.worker_boot = t.boot;
  l.domain = dcd::MemoryDomain::HOST_MEMORY; l.locator.key = "host:/tmp/state"; l.logical_bytes = 4096; l.process_tag = tag;
  l.reachability = dcd::Reachability::REACHABLE; l.health = dcd::Health::HEALTHY; l.freshness = dcd::Freshness::CURRENT; l.integrity = dcd::Integrity::VERIFIED;
  l.authority = env(t.dir, t.boot, 1);
  t.dir.register_location(l);
  dcd::ReplicaDescriptor rep; rep.replica_id = t.replica; rep.location = t.location; rep.cache = t.cache; rep.entry = t.entry;
  rep.reachability = dcd::Reachability::REACHABLE; rep.health = dcd::Health::HEALTHY; rep.integrity = dcd::Integrity::VERIFIED; rep.freshness = dcd::Freshness::CURRENT;
  rep.authority = env(t.dir, t.boot, 1);
  t.dir.register_replica(rep);
  dcd::DirectoryRecord rec = make_record(t);
  t.dir.register_entry(rec);
  return t;
}

// Add an additional replica of the same state/gen under a different boot/cache/location.
inline void register_extra(Topology& t, dcd::WorkerBootId boot, dcd::WorkerId worker, dcd::NodeId node,
                           dcd::CacheId cache, dcd::CacheEntryId entry, dcd::ReplicaId replica,
                           dcd::LocationId location, const std::string& tag) {
  dcd::WorkerSession s; s.boot = boot; s.worker = worker; s.node = node; s.tag = tag;
  t.dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id = cache; c.kind = dcd::CacheKind::REMOTE_NODE_CACHE; c.node = node; c.worker = worker; c.worker_boot = boot;
  c.domain = dcd::MemoryDomain::LOCAL_FILESYSTEM; c.authority = env(t.dir, boot, 1);
  t.dir.register_cache(c);
  dcd::LocationDescriptor l; l.location_id = location; l.node = node; l.worker = worker; l.worker_boot = boot;
  l.domain = dcd::MemoryDomain::LOCAL_FILESYSTEM; l.locator.key = "fs:/node" + std::to_string(node.value()) + "/s"; l.logical_bytes = 4096; l.process_tag = tag;
  l.reachability = dcd::Reachability::REACHABLE; l.health = dcd::Health::HEALTHY; l.freshness = dcd::Freshness::CURRENT; l.integrity = dcd::Integrity::VERIFIED;
  l.authority = env(t.dir, boot, 1);
  t.dir.register_location(l);
  dcd::ReplicaDescriptor rep; rep.replica_id = replica; rep.location = location; rep.cache = cache; rep.entry = entry;
  rep.reachability = dcd::Reachability::REACHABLE; rep.health = dcd::Health::HEALTHY; rep.integrity = dcd::Integrity::VERIFIED; rep.freshness = dcd::Freshness::CURRENT;
  rep.authority = env(t.dir, boot, 1);
  t.dir.register_replica(rep);
  dcd::DirectoryRecord rec = make_record(t);
  rec.cache = cache; rec.entry = entry; rec.replica = replica; rec.location = location; rec.node = node; rec.worker = worker; rec.worker_boot = boot;
  rec.domain = dcd::MemoryDomain::LOCAL_FILESYSTEM; rec.authority = env(t.dir, boot, 1);
  t.dir.register_entry(rec);
}

}  // namespace tst
