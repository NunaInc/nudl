//
// Copyright 2022 Nuna inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef NUDL_STATUS_TESTING_H_
#define NUDL_STATUS_TESTING_H_

#include <utility>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace nudl {
namespace status {
namespace internal {
inline absl::Status AnyToStatus(const absl::Status& st) { return st; }
inline absl::Status AnyToStatus(absl::Status&& st) { return std::move(st); }
template <typename T>
absl::Status AnyToStatus(const absl::StatusOr<T>& st) {
  return st.status();
}
template <typename T>
absl::Status AnyToStatus(absl::StatusOr<T>&& st) {
  return std::move(st).status();
}
}  // namespace internal
}  // namespace status
}  // namespace nudl

#define TEST_STATUS_MACROS_STRINGIFY(x) #x
#define TEST_STATUS_MACROS_CONCAT_NAME_INNER(x, y) x##y
#define TEST_STATUS_MACROS_CONCAT_NAME(x, y) \
  TEST_STATUS_MACROS_CONCAT_NAME_INNER(x, y)

#define EXPECT_OK(expr)                                               \
  do {                                                                \
    auto _res = (expr);                                               \
    ::absl::Status _st = ::nudl::status::internal::AnyToStatus(_res); \
    EXPECT_TRUE(_st.ok()) << "'" TEST_STATUS_MACROS_STRINGIFY(        \
                                 expr) "' failed with "               \
                          << _st.ToString();                          \
  } while (false)

#define ASSERT_OK(expr)                                               \
  do {                                                                \
    auto _res = (expr);                                               \
    ::absl::Status _st = ::nudl::status::internal::AnyToStatus(_res); \
    ASSERT_TRUE(_st.ok()) << "'" TEST_STATUS_MACROS_STRINGIFY(        \
                                 expr) "' failed with "               \
                          << _st.ToString();                          \
  } while (false)

#define ASSERT_OK_AND_ASSIGN_IMPL(status_name, lhs, rexpr) \
  auto&& status_name = (rexpr);                            \
  ASSERT_OK(status_name.status());                         \
  lhs = std::move(status_name).value()

#define ASSERT_OK_AND_ASSIGN(lhs, rexpr)                                  \
  ASSERT_OK_AND_ASSIGN_IMPL(                                              \
      TEST_STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, \
      rexpr);

#define STATUSEXP(expr, ENUM, _st)                                            \
  "Expecting '" TEST_STATUS_MACROS_STRINGIFY(                                 \
      expr) "' to fail with " TEST_STATUS_MACROS_STRINGIFY(ENUM) ", but got " \
      << _st.ToString()

#define ASSERT_RAISES(expr, ENUM)                                     \
  do {                                                                \
    auto _res = (expr);                                               \
    ::absl::Status _st = ::nudl::status::internal::AnyToStatus(_res); \
    ASSERT_TRUE(::absl::Is##ENUM(_st)) << STATUSEXP(expr, ENUM, _st); \
  } while (false)

#define EXPECT_RAISES(expr, ENUM)                                     \
  do {                                                                \
    auto _res = (expr);                                               \
    ::absl::Status _st = ::nudl::status::internal::AnyToStatus(_res); \
    EXPECT_TRUE(::absl::Is##ENUM(_st)) << STATUSEXP(expr, ENUM, _st); \
  } while (false)

#define ASSERT_RAISES_WITH_MESSAGE(expr, ENUM, message)               \
  do {                                                                \
    auto _res = (expr);                                               \
    ::absl::Status _st = ::nudl::status::internal::AnyToStatus(_res); \
    ASSERT_TRUE(::absl::Is##ENUM(_st)) << STATUSEXP(expr, ENUM, _st); \
    ASSERT_EQ((message), _st.ToString());                             \
  } while (false)

#define EXPECT_RAISES_WITH_MESSAGE(expr, ENUM, message)               \
  do {                                                                \
    auto _res = (expr);                                               \
    ::absl::Status _st = ::nudl::status::internal::AnyToStatus(_res); \
    EXPECT_TRUE(::absl::Is##ENUM(_st)) << STATUSEXP(expr, ENUM, _st); \
    EXPECT_EQ((message), _st.ToString());                             \
  } while (false)

#define EXPECT_RAISES_WITH_MESSAGE_THAT(expr, ENUM, matcher)          \
  do {                                                                \
    auto _res = (expr);                                               \
    ::absl::Status _st = ::nudl::status::internal::AnyToStatus(_res); \
    EXPECT_TRUE(::absl::Is##ENUM(_st)) << STATUSEXP(expr, ENUM, _st); \
    EXPECT_THAT(_st.ToString(), (matcher));                           \
  } while (false)

#endif  // NUDL_STATUS_TESTING_H_
