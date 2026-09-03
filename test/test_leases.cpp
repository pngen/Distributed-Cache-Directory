#include "setup.hpp"
#include "test_framework.hpp"
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  auto rec = t.dir.records().front();
  dcd::LeaseDescriptor lease; lease.lease_id = dcd::LeaseId(1); lease.generation = dcd::LeaseGeneration(1); lease.holder = t.boot; lease.expires = true; lease.start_ns = dcd::wall_clock_ns(); lease.expiry_ns = lease.start_ns + 1000000000ull; lease.state = dcd::LeaseState::ACTIVE;
  lease.authority = tst::env(t.dir, t.boot, 1);
  auto lr = t.dir.register_lease(lease);
  DCD_CHECK(lr.ok);
  // Renew advances the generation.
  auto rn = t.dir.renew_lease(dcd::LeaseId(1), tst::env(t.dir, t.boot, 2));
  DCD_CHECK(rn.ok);
  DCD_CHECK(rn.value.generation.value() > 1u);
  // A stale lease renewal (lower fence) rejected.
  auto stale = t.dir.renew_lease(dcd::LeaseId(1), tst::env(t.dir, t.boot, 1));
  DCD_CHECK(!stale.ok);
  DCD_CHECK(stale.error == dcd::DirectoryError::STALE_AUTHORITY);
  // A renewal from a different boot rejected.
  auto other = t.dir.renew_lease(dcd::LeaseId(1), tst::env(t.dir, dcd::WorkerBootId(999), 3));
  DCD_CHECK(!other.ok);
  DCD_CHECK(other.error == dcd::DirectoryError::STALE_WORKER_BOOT);
  // Expire.
  auto ex = t.dir.expire_lease(dcd::LeaseId(1), tst::env(t.dir, t.boot, 3), "expired");
  DCD_CHECK(ex.ok);
  DCD_CHECK(t.dir.leases().at(dcd::LeaseId(1)).state == dcd::LeaseState::EXPIRED);
  return dcdtest::summary("test_leases");
}
