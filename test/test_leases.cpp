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
  // ---- regression: renewal extends the expiry; stale and lost-holder lease mutations are rejected ----
  {
    auto t2 = tst::register_one(dcd::StateId(2), dcd::StateGeneration(1), "wa");
    dcd::LeaseDescriptor l2; l2.lease_id = dcd::LeaseId(7); l2.generation = dcd::LeaseGeneration(1); l2.holder = t2.boot; l2.expires = true;
    l2.start_ns = 1000; l2.expiry_ns = 1000 + 1000000000ull; l2.state = dcd::LeaseState::ACTIVE; l2.authority = tst::env(t2.dir, t2.boot, 1);
    DCD_CHECK(t2.dir.register_lease(l2).ok);
    auto rn2 = t2.dir.renew_lease(dcd::LeaseId(7), tst::env(t2.dir, t2.boot, 2));
    DCD_CHECK(rn2.ok);
    DCD_CHECK(rn2.value.start_ns > 1000u);
    // The renewed lease runs for its full duration from the renewal.
    DCD_CHECK_EQ(rn2.value.expiry_ns - rn2.value.start_ns, 1000000000ull);
    // An expiry carrying a lower fence than the lease is stale and must not move the fence back.
    auto se = t2.dir.expire_lease(dcd::LeaseId(7), tst::env(t2.dir, t2.boot, 1), "stale");
    DCD_CHECK(!se.ok);
    DCD_CHECK(se.error == dcd::DirectoryError::STALE_AUTHORITY);
    DCD_CHECK(t2.dir.leases().at(dcd::LeaseId(7)).state == dcd::LeaseState::ACTIVE);
    // Stale-epoch expiry and revocation are rejected.
    auto old_env = tst::env(t2.dir, t2.boot, 5); old_env.epoch = dcd::CoordinatorEpoch(0);
    DCD_CHECK(t2.dir.expire_lease(dcd::LeaseId(7), old_env, "old").error == dcd::DirectoryError::STALE_EPOCH);
    DCD_CHECK(t2.dir.revoke_lease(dcd::LeaseId(7), old_env, "old").error == dcd::DirectoryError::STALE_EPOCH);
    DCD_CHECK(t2.dir.leases().at(dcd::LeaseId(7)).state == dcd::LeaseState::ACTIVE);
    // Once the holder's incarnation is lost it can neither renew nor expire the
    // lease, and an incarnation that is not live cannot revoke it.
    t2.dir.on_worker_loss(t2.boot, "killed");
    auto dead = t2.dir.renew_lease(dcd::LeaseId(7), tst::env(t2.dir, t2.boot, 9));
    DCD_CHECK(!dead.ok);
    DCD_CHECK(dead.error == dcd::DirectoryError::NO_AUTHORITY);
    DCD_CHECK(!t2.dir.expire_lease(dcd::LeaseId(7), tst::env(t2.dir, t2.boot, 9), "dead").ok);
    DCD_CHECK(!t2.dir.revoke_lease(dcd::LeaseId(7), tst::env(t2.dir, t2.boot, 9), "dead").ok);
    DCD_CHECK(t2.dir.leases().at(dcd::LeaseId(7)).state == dcd::LeaseState::REVALIDATION_REQUIRED);
  }
  return dcdtest::summary("test_leases");
}
