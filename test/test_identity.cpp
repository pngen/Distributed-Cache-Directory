#include "distributedcachedirectory/directory.hpp"
#include "test_framework.hpp"
#include <string>

namespace dcd = distributedcachedirectory;

int main() {
  dcd::Directory dir;
  // Verify strong identities are non-interchangeable, comparable, and hashable.
  dcd::StateId s1(1), s2(2);
  dcd::CacheId c1(10);
  DCD_CHECK(s1 != s2);
  DCD_CHECK(s1 < s2);
  DCD_CHECK_EQ(s1.value(), 1u);
  // Generation relation and comparison.
  dcd::StateGeneration g1(1), g2(2);
  DCD_CHECK(g1.compare(g2) == dcd::GenerationRelation::LESS);
  DCD_CHECK(g2.compare(g1) == dcd::GenerationRelation::GREATER);
  DCD_CHECK(g1.compare(g1) == dcd::GenerationRelation::EQUAL);
  // Enums stringify.
  DCD_CHECK_EQ(std::string(dcd::to_string(dcd::StateKind::TENSOR_STATE)), std::string("TENSOR_STATE"));
  DCD_CHECK_EQ(std::string(dcd::to_string(dcd::Freshness::REVALIDATION_REQUIRED)), std::string("REVALIDATION_REQUIRED"));
  return dcdtest::summary("test_identity");
}
