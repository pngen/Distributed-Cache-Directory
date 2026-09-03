#include "setup.hpp"
#include "test_framework.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
namespace dcd = distributedcachedirectory;
int main() {
  auto t = tst::register_one(dcd::StateId(1), dcd::StateGeneration(1), "wa");
  std::mutex m; std::atomic<int> failures{0};
  auto workload = [&](int id) {
    // Concurrent mixed read/write against the Directory serialized by a narrow
    // lock (the same discipline the coordinator uses), exercising the call
    // graph for re-entrancy/self-deadlock hazards.
    for (int i = 0; i < 200; ++i) {
      std::lock_guard<std::mutex> lock(m);
      dcd::DirectoryQuery q; q.exact_state = true; q.state = dcd::StateId(1); q.requester_process_tag = "wa";
      auto r = t.dir.query(q);
      if (!r.ok) ++failures;
      if (i % 7 == 0) {
        // register a unique extra record (from the same worker) -> should be unique.
        dcd::DirectoryRecord rec = tst::make_record(t);
        rec.state_id = dcd::StateId(1000 + id); rec.state_generation = dcd::StateGeneration(1);
        auto rr = t.dir.register_entry(rec);
        if (!rr.ok && rr.error != dcd::DirectoryError::DUPLICATE_IDENTITY) ++failures;
      }
      if (i % 11 == 0) {
        t.dir.update_health(dcd::DirectoryRecordId(1), dcd::Health::HEALTHY, tst::env(t.dir, t.boot, (unsigned long long)(1+i)));
      }
    }
  };
  std::vector<std::thread> th;
  for (int i = 0; i < 8; ++i) th.emplace_back(workload, i);
  for (auto& x : th) x.join();
  DCD_CHECK_EQ(failures.load(), 0);
  DCD_CHECK(t.dir.validate_indexes().ok);
  std::string prob; DCD_CHECK(t.dir.accounting().validate(prob));
  return dcdtest::summary("test_concurrency");
}
