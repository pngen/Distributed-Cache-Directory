#include "distributedcachedirectory/directory.hpp"
#include "test_framework.hpp"
#include "mp_child.hpp"
#include "distributedcachedirectory/client.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace dcd = distributedcachedirectory;

static std::string wait_for(Child& c, const std::string& marker, int timeout_ms = 5000) {
  std::string acc;
  int elapsed = 0;
  while (elapsed < timeout_ms) {
    acc += c.read_available();
    if (acc.find(marker) != std::string::npos) return acc;
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); elapsed += 50;
  }
  return acc;
}

// Send a command line and return the worker's response line: the first complete
// line that starts with OK, ERR or RESULT. Candidate detail lines are skipped.
static std::string send_cmd(Child& c, const std::string& cmd, int timeout_ms = 5000) {
  c.write_stdin(cmd + "\r\n");
  std::string acc;
  int elapsed = 0;
  while (elapsed < timeout_ms) {
    acc += c.read_available();
    std::size_t start = 0;
    for (;;) {
      const std::size_t e = acc.find('\n', start);
      if (e == std::string::npos) break;
      std::string line = acc.substr(start, e - start);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.rfind("OK ", 0) == 0 || line.rfind("ERR ", 0) == 0 || line.rfind("RESULT ", 0) == 0) return line;
      start = e + 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30)); elapsed += 30;
  }
  return acc;
}

static bool is_ok(const std::string& resp) { return resp.rfind("OK ", 0) == 0; }
static bool has(const std::string& resp, const std::string& what) { return resp.find(what) != std::string::npos; }

// Parse "DCD_PORT=<n>" out of the coordinator's start-up output.
static unsigned short parse_port(const std::string& out) {
  const std::size_t p = out.find("DCD_PORT=");
  if (p == std::string::npos) return 0;
  return static_cast<unsigned short>(std::strtoul(out.c_str() + p + 9, nullptr, 10));
}

int main(int argc, char** argv) {
  const std::string coord = (argc > 1) ? argv[1] : DCD_COORDINATOR_EXE;
  const std::string worker = (argc > 2) ? argv[2] : DCD_WORKER_EXE;
  const std::string persist = "mp_proof.bin";
  const std::string savePath = "mp_save.bin";
  // State left by an aborted run must not be recovered by this one.
  std::remove(persist.c_str()); std::remove(savePath.c_str());
  std::printf("multiprocess authority proof: coordinator=%s worker=%s\n", coord.c_str(), worker.c_str());

  std::vector<Child*> children;
  auto cleanup = [&]() { for (Child* c : children) c->kill(); std::remove(persist.c_str()); std::remove(savePath.c_str()); };

  // 1. Start the coordinator.
  std::string cerr;
  Child coordc = Child::spawn(coord, {"--port", "0", "--persist", persist.c_str()}, cerr);
  if (!coordc.hproc) { std::printf("FAIL spawn coordinator: %s\n", cerr.c_str()); return 1; }
  children.push_back(&coordc);
  const unsigned short port = parse_port(wait_for(coordc, "DCD_EPOCH=", 6000));
  if (port == 0) { std::printf("FAIL no port from coordinator\n"); cleanup(); return 1; }
  const std::string addr = "127.0.0.1:" + std::to_string(port);
  std::printf("coordinator started at %s\n", addr.c_str());

  Child a, b, a2, stale_worker, coord2, b2;
  auto mk_worker = [&](Child& slot, const std::vector<std::string>& args) {
    std::string e; slot = Child::spawn(worker, args, e);
    if (!slot.hproc) std::printf("FAIL spawn worker: %s\n", e.c_str());
    else children.push_back(&slot);
    return slot.hproc != nullptr;
  };

  // 2. Worker A (boot 101) and Worker B (boot 202), each a real OS process.
  DCD_CHECK(mk_worker(a, {"--coordinator", addr, "--boot", "101", "--worker", "1", "--node", "1", "--tag", "wa"}));
  DCD_CHECK(mk_worker(b, {"--coordinator", addr, "--boot", "202", "--worker", "2", "--node", "2", "--tag", "wb"}));
  DCD_CHECK(has(wait_for(a, "READY", 5000), "READY epoch=1"));
  DCD_CHECK(has(wait_for(b, "READY", 5000), "READY epoch=1"));

  // 3. Each worker registers its cache, location and replica, then the same
  //    state (100, generation 1) as its own replica.
  //    register-entry: state gen kind cache entry replica location device domain bytes digest locgen repgen entgen fence
  DCD_CHECK(is_ok(send_cmd(a, "register-cache 1 0 2 4096")));
  DCD_CHECK(is_ok(send_cmd(a, "register-location 5 2 0 host:/a 4096")));
  DCD_CHECK(is_ok(send_cmd(a, "register-replica 9 5 1 7 1")));
  DCD_CHECK(is_ok(send_cmd(a, "register-entry 100 1 2 1 7 9 5 0 2 4096 - 1 1 1 2")));
  DCD_CHECK(is_ok(send_cmd(b, "register-cache 2 4 3 4096")));
  DCD_CHECK(is_ok(send_cmd(b, "register-location 6 3 0 fs:/b 4096")));
  DCD_CHECK(is_ok(send_cmd(b, "register-replica 10 6 2 8 1")));
  DCD_CHECK(is_ok(send_cmd(b, "register-entry 100 1 2 2 8 10 6 0 3 4096 - 1 1 1 2")));

  // 4. Both replicas are discovered.
  const std::string q1 = send_cmd(b, "query 100 wb");
  std::printf("query after dual register: %s\n", q1.c_str());
  DCD_CHECK(has(q1, "outcome=FOUND_MULTIPLE") && has(q1, "count=2"));

  // 5. Worker A degrades its own copy (record 1 is the first registered entry).
  //    A degraded copy is still a candidate.
  DCD_CHECK(is_ok(send_cmd(a, "update-health 1 1 3")));
  DCD_CHECK(has(send_cmd(b, "query 100 wb"), "count=2"));

  // 6. Save.
  DCD_CHECK(is_ok(send_cmd(a, "save " + savePath)));

  // 7. Kill Worker A as a real OS process. The coordinator observes the loss
  //    when the connection closes; A's copy then needs revalidation and only
  //    B's replica is offered.
  a.terminate();
  std::string q_after_death;
  for (int waited = 0; waited < 5000; waited += 50) {
    q_after_death = send_cmd(b, "query 100 wb");
    if (has(q_after_death, "count=1")) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  std::printf("query after A death: %s\n", q_after_death.c_str());
  DCD_CHECK(has(q_after_death, "count=1"));

  // 8. Worker A restarts with a fresh boot (103), re-registers what its lost
  //    incarnation held and publishes generation 2.
  DCD_CHECK(mk_worker(a2, {"--coordinator", addr, "--boot", "103", "--worker", "1", "--node", "1", "--tag", "wa"}));
  DCD_CHECK(has(wait_for(a2, "READY", 5000), "READY epoch=1"));
  DCD_CHECK(is_ok(send_cmd(a2, "register-cache 1 0 2 4096")));
  DCD_CHECK(is_ok(send_cmd(a2, "register-location 5 2 0 host:/a2 4096")));
  DCD_CHECK(is_ok(send_cmd(a2, "register-replica 9 5 1 7 1")));
  DCD_CHECK(is_ok(send_cmd(a2, "register-entry 100 2 2 1 7 9 5 0 2 4096 - 1 2 1 3")));

  // 9. The lost incarnation cannot come back: a worker process that presents
  //    the old boot 101 is refused a session.
  DCD_CHECK(mk_worker(stale_worker, {"--coordinator", addr, "--boot", "101", "--worker", "1", "--node", "1", "--tag", "wa"}));
  const std::string stale_hello = wait_for(stale_worker, "ERR HELLO", 5000);
  std::printf("stale worker: %s", stale_hello.c_str());
  DCD_CHECK(has(stale_hello, "ERR HELLO"));
  DCD_CHECK(!has(stale_hello, "READY"));

  // 10. Replay every stale mutation class over a real connection, carrying the
  //     lost incarnation's envelope, and check each is rejected with its class.
  std::string err;
  dcd::client::DcdClient observer = dcd::client::DcdClient::connect("127.0.0.1", port, err);
  dcd::client::DcdClient zombie = dcd::client::DcdClient::connect("127.0.0.1", port, err);
  DCD_CHECK(observer.connected());
  DCD_CHECK(zombie.connected());
  dcd::CoordinatorEpoch epoch;
  dcd::WorkerSession obs_session; obs_session.boot = dcd::WorkerBootId(900); obs_session.worker = dcd::WorkerId(9); obs_session.node = dcd::NodeId(9); obs_session.tag = "observer";
  DCD_CHECK(observer.hello(obs_session, epoch, err));
  DCD_CHECK(epoch == dcd::CoordinatorEpoch(1));

  dcd::WorkerSession old_session; old_session.boot = dcd::WorkerBootId(101); old_session.worker = dcd::WorkerId(1); old_session.node = dcd::NodeId(1); old_session.tag = "wa";
  dcd::CoordinatorEpoch ignored;
  DCD_CHECK(!zombie.hello(old_session, ignored, err));
  DCD_CHECK(zombie.last_error() == dcd::DirectoryError::STALE_WORKER_BOOT);

  dcd::AuthorityEnvelope stale; stale.epoch = epoch; stale.boot = dcd::WorkerBootId(101); stale.fence = 99;
  auto rejected_as = [&](bool sent_ok, dcd::DirectoryError expected, const char* what) {
    const bool ok = !sent_ok && zombie.last_error() == expected;
    std::printf("%s %s: %s %s\n", ok ? "PASS" : "FAIL", what, dcd::to_string(zombie.last_error()), zombie.last_error_text().c_str());
    DCD_CHECK_MSG(ok, what);
  };
  // registration of the old generation and of the fresh one
  dcd::DirectoryRecord sr; sr.state_id = dcd::StateId(100); sr.state_generation = dcd::StateGeneration(1); sr.kind = dcd::StateKind::TENSOR_STATE;
  sr.cache = dcd::CacheId(1); sr.entry = dcd::CacheEntryId(7); sr.replica = dcd::ReplicaId(9); sr.location = dcd::LocationId(5);
  sr.node = dcd::NodeId(1); sr.worker = dcd::WorkerId(1); sr.worker_boot = dcd::WorkerBootId(101); sr.domain = dcd::MemoryDomain::HOST_MEMORY; sr.logical_bytes = 4096;
  sr.health = dcd::Health::HEALTHY; sr.freshness = dcd::Freshness::CURRENT; sr.integrity = dcd::Integrity::VERIFIED; sr.reachability = dcd::Reachability::REACHABLE;
  sr.authority = stale;
  rejected_as(zombie.register_entry(sr, err), dcd::DirectoryError::NO_AUTHORITY, "stale registration");
  sr.state_generation = dcd::StateGeneration(2);
  rejected_as(zombie.register_entry(sr, err), dcd::DirectoryError::NO_AUTHORITY, "stale registration of the current generation");
  // updates: record 1 was the lost incarnation's own, record 3 is the fresh incarnation's
  rejected_as(zombie.update_health(dcd::DirectoryRecordId(1), dcd::Health::HEALTHY, stale, err), dcd::DirectoryError::NO_AUTHORITY, "stale health update on its own record");
  rejected_as(zombie.update_freshness(dcd::DirectoryRecordId(1), dcd::Freshness::CURRENT, stale, err), dcd::DirectoryError::NO_AUTHORITY, "stale freshness update on its own record");
  rejected_as(zombie.update_health(dcd::DirectoryRecordId(3), dcd::Health::UNAVAILABLE, stale, err), dcd::DirectoryError::STALE_WORKER_BOOT, "stale health update on the fresh record");
  rejected_as(zombie.update_integrity(dcd::DirectoryRecordId(3), dcd::Integrity::CORRUPT, stale, err), dcd::DirectoryError::STALE_WORKER_BOOT, "stale integrity update on the fresh record");
  rejected_as(zombie.update_reachability(dcd::DirectoryRecordId(3), dcd::Reachability::UNREACHABLE, stale, err), dcd::DirectoryError::STALE_WORKER_BOOT, "stale reachability update on the fresh record");
  // a lease renewal (no lease can be created over the wire, so this one is unknown)
  rejected_as(zombie.renew_lease(dcd::LeaseId(1), stale, err), dcd::DirectoryError::INVALID_IDENTITY, "lease renewal for an unknown lease");
  // invalidation of the fresh state, cache, replica, location and incarnation
  dcd::protocol::InvalidateMsg im; im.env = stale; im.reason = "stale-invalidate";
  im.kind = 0; im.target = 100; im.generation = 2;
  rejected_as(zombie.invalidate(im, err), dcd::DirectoryError::NO_AUTHORITY, "stale state invalidation");
  im.generation = 0;
  im.kind = 1; im.target = 1; rejected_as(zombie.invalidate(im, err), dcd::DirectoryError::NO_AUTHORITY, "stale cache invalidation");
  im.kind = 2; im.target = 9; rejected_as(zombie.invalidate(im, err), dcd::DirectoryError::NO_AUTHORITY, "stale replica invalidation");
  im.kind = 3; im.target = 5; rejected_as(zombie.invalidate(im, err), dcd::DirectoryError::NO_AUTHORITY, "stale location invalidation");
  im.kind = 4; im.target = 103; rejected_as(zombie.invalidate(im, err), dcd::DirectoryError::NO_AUTHORITY, "stale worker-boot invalidation");
  // tombstone of the fresh generation
  dcd::TombstoneRecord tb; tb.tombstone_id = dcd::TombstoneId(1); tb.target.kind = dcd::TombstoneKind::STATE; tb.target.state = dcd::StateId(100); tb.target.generation_floor = 2;
  tb.epoch = epoch; tb.worker_boot = dcd::WorkerBootId(101); tb.reason = "stale-tombstone"; tb.timestamp_ns = dcd::wall_clock_ns();
  rejected_as(zombie.tombstone(tb, err), dcd::DirectoryError::NO_AUTHORITY, "stale tombstone");
  // cache registration
  dcd::CacheDescriptor sc; sc.cache_id = dcd::CacheId(99); sc.kind = dcd::CacheKind::PROCESS_LOCAL; sc.node = dcd::NodeId(1); sc.worker = dcd::WorkerId(1); sc.worker_boot = dcd::WorkerBootId(101);
  sc.domain = dcd::MemoryDomain::HOST_MEMORY; sc.authority = stale;
  rejected_as(zombie.register_cache(sc, err), dcd::DirectoryError::NO_AUTHORITY, "stale cache registration");
  // a stale epoch is rejected before anything else
  dcd::AuthorityEnvelope old_epoch = stale; old_epoch.epoch = dcd::CoordinatorEpoch(0); old_epoch.boot = dcd::WorkerBootId(103);
  rejected_as(zombie.update_health(dcd::DirectoryRecordId(3), dcd::Health::UNAVAILABLE, old_epoch, err), dcd::DirectoryError::STALE_EPOCH, "stale-epoch update");

  // 11. None of the replays changed current state: exactly the fresh
  //     incarnation's generation-2 record is offered, still healthy and current.
  dcd::DirectoryQuery q; q.query_id = dcd::QueryId(1); q.exact_state = true; q.state = dcd::StateId(100); q.requester_process_tag = "observer";
  dcd::QueryResult final_result;
  DCD_CHECK(observer.query(q, final_result, err));
  DCD_CHECK_EQ(final_result.candidates.size(), 1u);
  if (final_result.candidates.size() == 1u) {
    const auto& rec = final_result.candidates[0].record;
    DCD_CHECK(rec.record_id == dcd::DirectoryRecordId(3));
    DCD_CHECK(rec.state_generation == dcd::StateGeneration(2));
    DCD_CHECK(rec.worker_boot == dcd::WorkerBootId(103));
    DCD_CHECK(rec.current);
    DCD_CHECK(rec.lifecycle == dcd::LifecycleState::AVAILABLE);
    DCD_CHECK(rec.health == dcd::Health::HEALTHY && rec.integrity == dcd::Integrity::VERIFIED && rec.reachability == dcd::Reachability::REACHABLE);
  }

  // 12. One response per request: an invalidation of an unknown kind is
  //     answered once, so the next request on the connection gets its own
  //     answer; and one connection carries one session.
  dcd::protocol::InvalidateMsg bad; bad.kind = 9; bad.target = 100; bad.generation = 2; bad.env = stale; bad.reason = "bad-kind";
  DCD_CHECK(!observer.invalidate(bad, err));
  DCD_CHECK(observer.last_error() == dcd::DirectoryError::PROTOCOL_VIOLATION);
  dcd::QueryResult after_bad;
  DCD_CHECK_MSG(observer.query(q, after_bad, err), err);
  DCD_CHECK_EQ(after_bad.candidates.size(), 1u);
  dcd::WorkerSession second; second.boot = dcd::WorkerBootId(901); second.worker = dcd::WorkerId(9); second.node = dcd::NodeId(9); second.tag = "observer";
  DCD_CHECK(!observer.hello(second, ignored, err));
  DCD_CHECK(observer.last_error() == dcd::DirectoryError::PROTOCOL_VIOLATION);

  // 13. Save the final state, stop the coordinator, and start a fresh one that
  //     recovers it under a new epoch. A surviving worker gets a session, and the
  //     recovered copies are not offered until they are revalidated.
  DCD_CHECK(is_ok(send_cmd(b, "save " + savePath)));
  observer.close(); zombie.close();
  coordc.kill();
  coord2 = Child::spawn(coord, {"--port", "0", "--persist", savePath}, cerr);
  DCD_CHECK(coord2.hproc != nullptr);
  if (coord2.hproc) children.push_back(&coord2);
  const std::string co2 = coord2.hproc ? wait_for(coord2, "DCD_EPOCH=", 6000) : std::string();
  const unsigned short port2 = parse_port(co2);
  DCD_CHECK(port2 != 0);
  DCD_CHECK(has(co2, "DCD_EPOCH=2"));
  if (port2 != 0) {
    const std::string addr2 = "127.0.0.1:" + std::to_string(port2);
    DCD_CHECK(mk_worker(b2, {"--coordinator", addr2, "--boot", "202", "--worker", "2", "--node", "2", "--tag", "wb"}));
    DCD_CHECK(has(wait_for(b2, "READY", 5000), "READY epoch=2"));
    const std::string q_recover = send_cmd(b2, "query 100 wb");
    std::printf("post-recovery query: %s\n", q_recover.c_str());
    DCD_CHECK(has(q_recover, "outcome=STALE_ONLY") && has(q_recover, "count=0"));
  }

  cleanup();
  return dcdtest::summary("test_multiprocess");
}
