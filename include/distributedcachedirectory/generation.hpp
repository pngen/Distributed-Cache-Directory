#pragma once
#include "strong_type.hpp"
#include <cstdint>
#include <string>

namespace distributedcachedirectory {

// Explicit comparison of generations. Generation order is not used as a single
// global fence: authority is incarnation-scoped (see authority.hpp). Within the
// same authority scope a generation relation is well defined; across scopes the
// relation is reported as INCOMPARABLE so a numerically larger generation from
// a stale WorkerBootId can never be treated as newer than a fresh incarnation.
enum class GenerationRelation : std::uint8_t {
  LESS = 0,
  EQUAL = 1,
  GREATER = 2,
  INCOMPARABLE = 3,
};

inline const char* to_string(GenerationRelation g) {
  switch (g) {
    case GenerationRelation::LESS: return "LESS";
    case GenerationRelation::EQUAL: return "EQUAL";
    case GenerationRelation::GREATER: return "GREATER";
    case GenerationRelation::INCOMPARABLE: return "INCOMPARABLE";
  }
  return "INCOMPARABLE";
}

namespace detail {
template <typename Tag>
class GenerationValue : public StrongValue<Tag> {
 public:
  using StrongValue<Tag>::StrongValue;
  using StrongValue<Tag>::operator=;

  constexpr GenerationRelation compare(const GenerationValue& other) const {
    if (this->value() < other.value()) return GenerationRelation::LESS;
    if (this->value() > other.value()) return GenerationRelation::GREATER;
    return GenerationRelation::EQUAL;
  }
  constexpr bool precedes(const GenerationValue& o) const { return compare(o) == GenerationRelation::LESS; }
  constexpr bool is_at_least(const GenerationValue& o) const { return compare(o) != GenerationRelation::LESS; }
  constexpr bool is_at_most(const GenerationValue& o) const { return compare(o) != GenerationRelation::GREATER; }
};
}  // namespace detail

}  // namespace distributedcachedirectory

#define DCD_GEN(NAME)                                        \
  namespace distributedcachedirectory {                      \
  class NAME : public detail::GenerationValue<struct NAME##Tag> { \
   public:                                                    \
    using detail::GenerationValue<struct NAME##Tag>::GenerationValue; \
    using detail::GenerationValue<struct NAME##Tag>::operator=;      \
  };                                                          \
  }                                                            \
  namespace std {                                              \
  template <> struct hash<distributedcachedirectory::NAME> {   \
    std::size_t operator()(const distributedcachedirectory::NAME& v) const { \
      return distributedcachedirectory::strong_hash(v.value()); \
    }                                                          \
  };                                                           \
  }

DCD_GEN(CoordinatorEpoch)
DCD_GEN(DirectoryGeneration)
DCD_GEN(RecordGeneration)
DCD_GEN(StateGeneration)
DCD_GEN(CacheGeneration)
DCD_GEN(EntryGeneration)
DCD_GEN(ReplicaGeneration)
DCD_GEN(LocationGeneration)
DCD_GEN(NodeGeneration)
DCD_GEN(WorkerGeneration)
DCD_GEN(DeviceGeneration)
DCD_GEN(BackendGeneration)
DCD_GEN(CompatibilityGeneration)
DCD_GEN(ProvenanceGeneration)
DCD_GEN(LeaseGeneration)
DCD_GEN(ObservationGeneration)
DCD_GEN(QueryGeneration)
DCD_GEN(AttemptGeneration)
DCD_GEN(DispatchGeneration)
DCD_GEN(PolicyGeneration)
