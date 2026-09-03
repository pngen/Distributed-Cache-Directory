# Distributed Cache Directory

Distributed Cache Directory is a vendor-neutral C++20 runtime that answers one
systems question precisely:

> **Who has what reusable state, where is it, at what generation, with what
> integrity and freshness, and what will it cost to access?**

It is a **distributed location/replica directory** for reusable AI state. Modern
AI infrastructure accumulates KV state, prefix state, tensors, model artifacts,
compiled kernels, execution graphs, checkpoints, buffers, storage objects and
other reusable machine-produced state across workers, nodes, accelerators,
caches, filesystems, and remote tiers. Distributed Cache Directory records where
that state physically and logically lives *right now*, which copies are current
and reachable, which locations are stale or invalid, which replica should be
contacted, and which access path is justified by current evidence.

## Systems boundary

Distributed Cache Directory owns one boundary, and intentionally does not become
any of the following:

- **State Index** — it does not answer *what content matches a query*; it answers
  *where a known, reusable copy lives*.
- **Runtime Registry** — it does not track live services, jobs, or RPC endpoints.
- **Storage Fabric / KV Fabric / Tensor Cache** — it does not own, move, or
  serve the state bytes.

Distributed Cache Directory is the distributed location/replica directory for
reusable AI state. State identity and distributed location remain separate: the
same StateId/StateGeneration may have many directory records, each describing one
cache copy/location under explicit authority.

## Provenance and honesty

Distributed Cache Directory never claims a physical class it cannot prove:

- real local process/host/filesystem and CUDA-device locations are physically
  validated where the proofs pass;
- remote/multi-node and synthetic cache locations are synthetic where physical
  infrastructure is unavailable;
- there are no physical RDMA, shared-cache-appliance, or remote object-store
  claims;
- unknown physical facts remain UNKNOWN;
- process-local and CUDA-device locations require revalidation after a process or
  coordinator restart.

## Cache model

Caches are explicit, versioned descriptors, not implicit maps. A cache belongs to
a node, worker and worker incarnation, has a domain, reachability, health,
freshness, and a current generation. The runtime never claims a physical cache
class it cannot establish.

## Directory records

Each directory record describes one cache copy/location under explicit authority:

- identities (DirectoryRecordId, StateId, CacheId, CacheEntryId, ReplicaId,
  LocationId, NodeId, WorkerId, WorkerBootId, DeviceId)
- state generation and state kind
- memory/storage domain, logical and physical bytes, content digest
- location/replica/entry generations
- health, freshness, integrity, reachability
- lease/liveness, access estimate, authority envelope
- lifecycle state, current/historical flag, timestamps, semantic digest

A record is **not** AVAILABLE merely because its metadata exists. Availability
requires current authority, acceptable freshness, a reachable location, non-corrupt
integrity, acceptable cache/node/worker health, and valid lease/liveness when
required.

## Locations, replicas, freshness, health, integrity, reachability

These are independent state machines. They are never collapsed into a single
boolean. A recovered process-local/CUDA location does not silently become CURRENT.
One unhealthy replica does not invalidate all replicas of a state.

## Leases and liveness

Leases are bounded, holder-scoped and generation-fenced. A stale lease renewal
from an old boot is rejected. They are never wall-clock-TTL-only; session/boot
liveness is the primary model.

## Queries and deterministic ranking

Queries support candidate elimination (hard filters) first, then deterministic
ranking by named factors (exact generation, same process/device/node/NUMA,
domain preference, reachability, health, freshness, integrity, latency,
bandwidth, transfer bytes, staging, restore, replica diversity, policy
preference). Tie-breaking is deterministic. Queries return outcomes (FOUND_LOCAL,
FOUND_REMOTE, FOUND_MULTIPLE, NOT_FOUND, STALE_ONLY, UNREACHABLE_ONLY,
CORRUPT_ONLY, INCOMPATIBLE_ONLY, INSUFFICIENT_EVIDENCE, UNKNOWN), the selected
candidate, rejection reasons, and explanations. UNKNOWN/insufficient evidence
never silently becomes FOUND_LOCAL/FOUND_REMOTE.

## Invalidation, supersession, tombstones

Mutations are generation-fenced and authority-scoped. Stale invalidation cannot
invalidate fresh state. Publishing a fresher generation supersedes older current
records (history is preserved, not erased). Tombstones block resurrection: a
stale worker cannot republish a record covered by a current tombstone. A
numerically larger generation from an old WorkerBootId never fences a fresh
process incarnation.

## Distributed authority

Authority is incarnation-scoped by WorkerBootId plus a per-(record, boot) fence.
The coordinator (a real OS process) is the single source of truth; workers (real
OS processes) assert registrations/updates over real loopback TCP. Worker loss is
observed when the connection closes. The multiprocess proof kills and restarts a
worker as a real process and verifies every stale class is rejected.

## Persistence and recovery

Versioned deterministic binary persistence (magic, version, bounded counts,
CRC-32, SHA-256 semantic digest) stores canonical directory state, never hash
bucket layout. Secondary indexes are rebuilt deterministically on recovery.
Atomic writes: temp → flush → close → rename. Recovery clears live worker
authority, flags process-local/CUDA locations REVALIDATION_REQUIRED, clears
active leases, keeps current tombstones authoritative, and never commits
incomplete mutations.

## Real local proof

Real local records are proven: exact identity registration, multiple replicas,
multiple domains, content digest, exact query, multi-candidate ranking, health/
reachability/freshness transitions, lease expiration/revalidation, invalidation,
supersession, tombstone, historical query, persistence/recovery, and stale
location exclusion.

## CUDA proof

With `DCD_ENABLE_CUDA_PROOF=ON` and a CUDA device present, the CUDA proof
allocates a real device buffer, populates it, copies it back and verifies CPU
parity, registers a CUDA_DEVICE location (by locator string, never a raw device
pointer), returns it from a CUDA-preferring query, frees the buffer and excludes
the freed location on a stale replay, then registers a fresh location generation
under the current boot. Device memory returns to baseline.

On an NVIDIA GeForce RTX 5090 (sm_120, CUDA 13.1/12.9) the proof passes with
CPU parity verified.

## Synthetic distributed scenarios

Where physical multi-node/RDMA infrastructure is unavailable, deterministic
synthetic scenarios are available (all explicitly `provenance=SYNTHETIC`):
same state on two nodes, GPU-local + host-local, local + remote cache, exact
remote replica, stale local replica, degraded local vs healthy remote, corrupt
replica, unreachable replica, expired lease, fresh lease renewal, worker restart,
cache/node/backend generation rollover, compatibility mismatch, exact digest
farther away, local compatible vs remote exact, tombstone resurrection
prevention, replica count below requirement, equal candidate deterministic
tie-break, unknown freshness/reachability/integrity, and policy preference
change.

## CLI

The `distributed-cache-directory` tool provides `cache-register`,
`entry-register`, `show`, `query`, `replicas`, `locations`, `health`,
`reachability`, `lease-renew`, `invalidate`, `tombstone`, `history`,
`explain`, `simulate`, `save`, `recover`, `benchmark`. It exposes
StateId, StateGeneration, CacheId, ReplicaId, LocationId, NodeId, WorkerBootId,
domain, freshness, health, integrity, reachability, lease state, authority,
provenance, query outcome, selected candidate, and rejection reasons.

## Examples

`examples/` contains 15 runnable programs (01–15) covering cache identity,
state registration, multi-replica, multi-domain, local vs remote, health
ranking, reachability, lease liveness, invalidation, supersession, tombstone,
historical query, persistence/recovery, multiprocess authority, and CUDA
location, each using the real library API.

## Benchmarks

`benchmarks/` measures completed work: registration, StateId lookup, index
rebuild, persistence serialize/recover, and protocol encode/decode at 1k and 10k
records, plus read-heavy 100k benchmarks. It reports ops/s, ns/op, record count,
thread count, and wall time. Registration remains accounting-O(n) and is reported
honestly; it does not benchmark empty loops.

## Build

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DDCD_BUILD_TESTS=ON -DDCD_BUILD_EXAMPLES=ON -DDCD_BUILD_BENCHMARKS=ON
cmake --build build
ctest --test-dir build
```

CUDA is optional: add `-DDCD_ENABLE_CUDA_PROOF=ON`.

## Downstream consumption

```
find_package(DistributedCacheDirectory CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE DistributedCacheDirectory::distributedcachedirectory)
```

## Limitations

- real local/process/CUDA locations are physically validated where proofs pass;
- remote/multi-node cache locations are synthetic where unavailable;
- no physical RDMA/cache-appliance claims;
- Distributed Cache Directory locates reusable state; it does not own state
  compatibility/provenance/storage semantics;
- process-local/CUDA locations require revalidation after restart;
- unknown physical facts remain UNKNOWN.

## License
Apache License 2.0. Copyright 2026 Summon Software Labs. No telemetry transmission.
