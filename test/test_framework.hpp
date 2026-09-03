#pragma once
#include <cstdio>
#include <string>

namespace dcdtest {

inline int checks = 0;
inline int failures = 0;

inline bool check(bool ok, const char* file, int line, const std::string& what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL %s:%d  %s\n", file, line, what.c_str());
  }
  return ok;
}

inline int summary(const char* name) {
  std::printf("[%s] %d checks, %d failures\n", name, checks, failures);
  return failures == 0 ? 0 : 1;
}

}  // namespace dcdtest

#define DCD_CHECK(cond) dcdtest::check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)
#define DCD_CHECK_EQ(a, b) dcdtest::check((a) == (b), __FILE__, __LINE__, std::string(#a " == " #b))
#define DCD_CHECK_MSG(cond, msg) dcdtest::check(static_cast<bool>(cond), __FILE__, __LINE__, (msg))
