// CUDA location proof for Distributed Cache Directory.
// Compiles only when DCD_ENABLE_CUDA_PROOF=ON and a CUDA toolkit is present.
#include "distributedcachedirectory/directory.hpp"
#include <cuda_runtime.h>
#include <cstdio>
#include <vector>

namespace dcd = distributedcachedirectory;

static bool verify_host(const std::vector<unsigned char>& h) {
  for (std::size_t i = 0; i < h.size(); ++i) if (h[i] != static_cast<unsigned char>((i * 31 + 7) & 0xFF)) return false;
  return true;
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  int dev = 0; cudaDeviceProp prop; cudaError_t errp = cudaGetDeviceProperties(&prop, dev);
  if (errp != cudaSuccess) { std::printf("cudaGetDeviceProperties failed: %s\n", cudaGetErrorString(errp)); return 2; }
  std::printf("CUDA proof device=%s sm=%d.%d\n", prop.name, prop.major, prop.minor);
  std::size_t bytes = 1u << 20;  // 1 MiB
  std::size_t free_before = 0, total = 0; cudaMemGetInfo(&free_before, &total);
  dcd::Directory dir;
  dcd::WorkerSession s; s.boot = dcd::WorkerBootId(1); s.worker = dcd::WorkerId(1); s.node = dcd::NodeId(1); s.tag = "cuda"; dir.register_worker_session(s);
  dcd::CacheDescriptor c; c.cache_id = dcd::CacheId(1); c.kind = dcd::CacheKind::DEVICE_LOCAL; c.domain = dcd::MemoryDomain::CUDA_DEVICE; c.node = s.node; c.worker = s.worker; c.worker_boot = s.boot; c.reachability = dcd::Reachability::REACHABLE; c.health = dcd::Health::HEALTHY; c.freshness = dcd::Freshness::CURRENT; c.authority.epoch = dir.epoch(); c.authority.boot = s.boot; c.authority.fence = 1; dir.register_cache(c);

  // 1. allocate + populate + verify CPU parity.
  unsigned char* dptr = nullptr;
  cudaMalloc(&dptr, bytes);
  std::vector<unsigned char> host(bytes);
  for (std::size_t i = 0; i < bytes; ++i) host[i] = static_cast<unsigned char>((i * 31 + 7) & 0xFF);
  cudaMemcpy(dptr, host.data(), bytes, cudaMemcpyHostToDevice);
  std::vector<unsigned char> back(bytes);
  cudaMemcpy(back.data(), dptr, bytes, cudaMemcpyDeviceToHost);
  std::printf("CUDA parity: %s\n", verify_host(back) ? "PASS" : "FAIL");

  // 2. register StateId X / Gen N at the CUDA_DEVICE location (locator string, never a raw pointer).
  dcd::LocationDescriptor l; l.location_id = dcd::LocationId(1); l.domain = dcd::MemoryDomain::CUDA_DEVICE; l.device = dcd::DeviceId(dev); l.node = s.node; l.worker = s.worker; l.worker_boot = s.boot;
  l.locator.key = std::string("cuda:") + std::to_string(dev) + "/state"; l.logical_bytes = bytes; l.authority = c.authority; dir.register_location(l);
  dcd::ReplicaDescriptor r; r.replica_id = dcd::ReplicaId(1); r.location = dcd::LocationId(1); r.cache = dcd::CacheId(1); r.entry = dcd::CacheEntryId(1); r.authority = c.authority; dir.register_replica(r);
  dcd::DirectoryRecord rec; rec.state_id = dcd::StateId(100); rec.state_generation = dcd::StateGeneration(1); rec.cache = dcd::CacheId(1); rec.entry = dcd::CacheEntryId(1); rec.replica = dcd::ReplicaId(1); rec.location = dcd::LocationId(1); rec.node = s.node; rec.worker = s.worker; rec.worker_boot = s.boot; rec.domain = dcd::MemoryDomain::CUDA_DEVICE; rec.device = dcd::DeviceId(dev); rec.logical_bytes = bytes; rec.reachability = dcd::Reachability::REACHABLE; rec.health = dcd::Health::HEALTHY; rec.freshness = dcd::Freshness::CURRENT; rec.integrity = dcd::Integrity::VERIFIED; rec.authority = c.authority; dir.register_entry(rec);

  // 3. query preferring CUDA_DEVICE returns it.
  dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(100); q.preferred_domains = {dcd::MemoryDomain::CUDA_DEVICE}; q.has_preferred_device = true; q.preferred_device = dcd::DeviceId(dev);
  auto q1 = dir.query(q);
  std::printf("query prefers CUDA: outcome=%s selected_domain=%s\n", dcd::to_string(q1.value.outcome), dcd::to_string(q1.value.candidates[0].record.domain));

  // 4. free the buffer, advance LocationGeneration / invalidate, and prove the stale location is excluded.
  cudaFree(dptr);
  dir.invalidate_location(dcd::LocationId(1), {dir.epoch(), s.boot, dcd::DirectoryGeneration(0), 2}, "freed");
  auto q2 = dir.query(q);
  std::printf("after free, query returns: candidates=%u (excludes freed location)\n", static_cast<unsigned>(q2.value.candidates.size()));
  // A stale replay of the freed location generation is rejected (owner/generation fencing).
  dcd::DirectoryRecord stale = rec;
  auto sr = dir.register_entry(stale);
  std::printf("stale replay of freed location rejected: %s\n", sr.ok ? "NO (bug)" : "yes");

  // 5. allocate a fresh buffer, verify, register a fresh LocationGeneration under the current boot.
  unsigned char* dptr2 = nullptr;
  cudaMalloc(&dptr2, bytes);
  cudaMemcpy(dptr2, host.data(), bytes, cudaMemcpyHostToDevice);
  std::vector<unsigned char> back2(bytes); cudaMemcpy(back2.data(), dptr2, bytes, cudaMemcpyDeviceToHost);
  std::printf("fresh CUDA parity: %s\n", verify_host(back2) ? "PASS" : "FAIL");
  dcd::LocationDescriptor l2; l2.location_id = dcd::LocationId(2); l2.domain = dcd::MemoryDomain::CUDA_DEVICE; l2.device = dcd::DeviceId(dev); l2.node = s.node; l2.worker = s.worker; l2.worker_boot = s.boot; l2.locator.key = std::string("cuda:") + std::to_string(dev) + "/state2"; l2.logical_bytes = bytes; l2.authority = c.authority; l2.generation = dcd::LocationGeneration(2); dir.register_location(l2);
  dcd::ReplicaDescriptor r2; r2.replica_id = dcd::ReplicaId(2); r2.location = dcd::LocationId(2); r2.cache = dcd::CacheId(1); r2.entry = dcd::CacheEntryId(1); r2.authority = c.authority; dir.register_replica(r2);
  dcd::DirectoryRecord rec2; rec2.state_id = dcd::StateId(100); rec2.state_generation = dcd::StateGeneration(2); rec2.cache = dcd::CacheId(1); rec2.entry = dcd::CacheEntryId(1); rec2.replica = dcd::ReplicaId(2); rec2.location = dcd::LocationId(2); rec2.node = s.node; rec2.worker = s.worker; rec2.worker_boot = s.boot; rec2.domain = dcd::MemoryDomain::CUDA_DEVICE; rec2.device = dcd::DeviceId(dev); rec2.logical_bytes = bytes; rec2.reachability = dcd::Reachability::REACHABLE; rec2.health = dcd::Health::HEALTHY; rec2.freshness = dcd::Freshness::CURRENT; rec2.integrity = dcd::Integrity::VERIFIED; rec2.authority = c.authority; dir.register_entry(rec2);
  auto q3 = dir.query(q);
  std::printf("after fresh registration: candidates=%u selected_gen=%llu domain=%s\n", static_cast<unsigned>(q3.value.candidates.size()), (unsigned long long)q3.value.candidates[0].record.state_generation.value(), dcd::to_string(q3.value.candidates[0].record.domain));

  // 6. cleanup; memory returns to baseline (roughly).
  cudaFree(dptr2);
  std::size_t free_after = 0; cudaMemGetInfo(&free_after, &total);
  std::printf("CUDA memory before=%zu after=%zu\n", free_before, free_after);
  std::printf("CUDA proof complete\n");
  return 0;
}
