#include "distributedcachedirectory/directory.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("08_lease_liveness\n"); dcd::Directory dir; dcd::WorkerSession s; s.boot=dcd::WorkerBootId(1); s.worker=dcd::WorkerId(1); s.node=dcd::NodeId(1); s.tag="w"; dir.register_worker_session(s);
  dcd::LeaseDescriptor lease; lease.lease_id=dcd::LeaseId(1); lease.holder=s.boot; lease.expires=true; lease.start_ns=dcd::wall_clock_ns(); lease.expiry_ns=lease.start_ns+1000000000ull; lease.state=dcd::LeaseState::ACTIVE; lease.authority.epoch=dir.epoch(); lease.authority.boot=s.boot; lease.generation=dcd::LeaseGeneration(1); dir.register_lease(lease);
  auto rn = dir.renew_lease(dcd::LeaseId(1), {dir.epoch(), s.boot, dcd::DirectoryGeneration(0), 2});
  std::printf("renew: %s gen=%llu\n", rn.ok?"ok":rn.error_text.c_str(), (unsigned long long)rn.value.generation.value());
  auto stale = dir.renew_lease(dcd::LeaseId(1), {dir.epoch(), s.boot, dcd::DirectoryGeneration(0), 1});
  std::printf("stale renewal rejected: %s (%s)\n", stale.ok?"NO":"yes", stale.error_text.c_str());
  dir.expire_lease(dcd::LeaseId(1), {dir.epoch(), s.boot, dcd::DirectoryGeneration(0), 3}, "ttl");
  std::printf("lease state now: %s\n", dcd::to_string(dir.leases().at(dcd::LeaseId(1)).state));
  return 0; }
