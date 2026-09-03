#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace distributedcachedirectory {

// Explicit, non-interchangeable identity types.
//
// Every DCD_IDENTITY_TYPE declares a distinct C++ type wrapping a uint64_t. No
// identity type converts implicitly to any other, so a StateId cannot be
// mistaken for a CacheId or a ReplicaId by any expression that would otherwise
// compile. Values compare by their underlying numeric value only for identity
// equality and total ordering used by containers; they are never used as a
// fence or epoch counter.

namespace detail {

template <typename Tag, typename Value = std::uint64_t>
class StrongValue {
 public:
  using value_type = Value;

  StrongValue() = default;
  constexpr explicit StrongValue(Value v) : value_(v) {}

  constexpr Value value() const { return value_; }
  constexpr explicit operator Value() const { return value_; }
  constexpr bool is_null() const { return value_ == Value{}; }
  constexpr bool is_zero() const { return value_ == Value{}; }

  friend constexpr bool operator==(const StrongValue& a, const StrongValue& b) { return a.value_ == b.value_; }
  friend constexpr bool operator!=(const StrongValue& a, const StrongValue& b) { return a.value_ != b.value_; }
  friend constexpr bool operator<(const StrongValue& a, const StrongValue& b) { return a.value_ < b.value_; }
  friend constexpr bool operator<=(const StrongValue& a, const StrongValue& b) { return a.value_ <= b.value_; }
  friend constexpr bool operator>(const StrongValue& a, const StrongValue& b) { return a.value_ > b.value_; }
  friend constexpr bool operator>=(const StrongValue& a, const StrongValue& b) { return a.value_ >= b.value_; }

  std::string str() const { return std::to_string(value_); }

 private:
  Value value_{};
};

}  // namespace detail

inline std::size_t strong_hash(std::uint64_t v) { return std::hash<std::uint64_t>{}(v); }

}  // namespace distributedcachedirectory

// A single macro that declares a strong identity type and provides a std::hash
// specialization for use as an unordered container key.
#define DCD_IDENTITY_TYPE(NAME)                                        \
  namespace distributedcachedirectory {                                \
  class NAME : public detail::StrongValue<struct NAME##Tag> {            \
   public:                                                              \
    using detail::StrongValue<struct NAME##Tag>::StrongValue;            \
    using detail::StrongValue<struct NAME##Tag>::operator=;              \
  };                                                                    \
  }                                                                      \
  namespace std {                                                        \
  template <> struct hash<distributedcachedirectory::NAME> {             \
    std::size_t operator()(const distributedcachedirectory::NAME& v) const { \
      return distributedcachedirectory::strong_hash(v.value());          \
    }                                                                    \
  };                                                                     \
  }

DCD_IDENTITY_TYPE(DirectoryId)
DCD_IDENTITY_TYPE(DirectoryRecordId)
DCD_IDENTITY_TYPE(StateId)
DCD_IDENTITY_TYPE(CacheId)
DCD_IDENTITY_TYPE(CacheEntryId)
DCD_IDENTITY_TYPE(ReplicaId)
DCD_IDENTITY_TYPE(LocationId)
DCD_IDENTITY_TYPE(NodeId)
DCD_IDENTITY_TYPE(WorkerId)
DCD_IDENTITY_TYPE(WorkerBootId)
DCD_IDENTITY_TYPE(DeviceId)
DCD_IDENTITY_TYPE(MemoryDomainId)
DCD_IDENTITY_TYPE(StorageBackendId)
DCD_IDENTITY_TYPE(StorageTierId)
DCD_IDENTITY_TYPE(ContentId)
DCD_IDENTITY_TYPE(CompatibilityId)
DCD_IDENTITY_TYPE(ProvenanceId)
DCD_IDENTITY_TYPE(LeaseId)
DCD_IDENTITY_TYPE(ObservationId)
DCD_IDENTITY_TYPE(QueryId)
DCD_IDENTITY_TYPE(ResultId)
DCD_IDENTITY_TYPE(TombstoneId)
DCD_IDENTITY_TYPE(AttemptId)
DCD_IDENTITY_TYPE(DispatchId)
