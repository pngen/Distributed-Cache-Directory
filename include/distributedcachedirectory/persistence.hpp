#pragma once
#include <cstdint>

namespace distributedcachedirectory {
namespace persistence {

// Versioned deterministic binary persistence format.
//
// Layout (little-endian):
//   magic     : u32  = DCD_PERSIST_MAGIC
//   version   : u32  = DCD_PERSIST_VERSION
//   crc32     : u32  = CRC-32 over the body
//   sha256    : [32] = SHA-256 over the body (semantic digest)
//   body      : VAR  = counts, then canonical entities
//
// The body stores canonical directory state (records, caches, locations,
// replicas, leases, tombstones, observations, generation counters); secondary
// indexes are never persisted and are rebuilt deterministically on load.
inline constexpr std::uint32_t DCD_PERSIST_MAGIC = 0x44434431u;  // "DCD1"
inline constexpr std::uint32_t DCD_PERSIST_VERSION = 1u;
inline constexpr std::uint64_t DCD_PERSIST_MAX_COUNT = 100000000ull;
inline constexpr std::size_t DCD_PERSIST_HEADER = 44;  // 4 + 4 + 4 + 32

}  // namespace persistence
}  // namespace distributedcachedirectory
