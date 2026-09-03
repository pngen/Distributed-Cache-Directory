#include "distributedcachedirectory/server.hpp"
#include "distributedcachedirectory/client.hpp"
#include <cstdio>
namespace dcd = distributedcachedirectory;
int main(){ std::printf("14_multiprocess_authority (run via test_multiprocess for the full real-OS-process proof)\n");
  // This example documents the coordinator/worker split; the real proof runs in test_multiprocess.
  dcd::server::CoordinatorServer srv; std::string err;
  if(!srv.start(0, "", err)){ std::printf("coord: %s\n", err.c_str()); return 1; }
  std::printf("coordinator running on port %u (authority is incarnation-scoped)\n", srv.port());
  // NOTE: use the worker tool with a real boot/epoch to exercise the round trip;
  // the deterministic assertion of every stale class is in test_multiprocess.
  srv.stop();
  return 0; }
