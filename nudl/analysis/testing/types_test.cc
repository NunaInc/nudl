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

#include "nudl/analysis/types.h"

#include <set>
#include <string>

#include "absl/base/macros.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nudl/analysis/type_store.h"
#include "nudl/analysis/vars.h"
#include "nudl/grammar/dsl.h"
#include "nudl/status/testing.h"
#include "nudl/testing/protobuf_matchers.h"

namespace nudl {
namespace analysis {

class TypesTest : public ::testing::Test {
 protected:
  absl::StatusOr<const TypeSpec*> FindType(std::string_view type_name,
                                           std::string_view scope_name = "") {
    ASSIGN_OR_RETURN(auto scope, ScopeName::Parse(scope_name),
                     _ << "For scope_name: `" << scope_name << "`");
    ASSIGN_OR_RETURN(auto type_spec, grammar::ParseTypeSpec(type_name),
                     _ << "For type_name: `" << type_name << "`");
    return store_.FindType(scope, *type_spec);
  }
  void CheckTypes(absl::Span<const absl::string_view> names,
                  absl::Span<const bool> is_ancestor,
                  absl::Span<const bool> is_convertible,
                  absl::Span<const bool> is_bound, int print_result = 0);

  GlobalTypeStore store_;
};

static const char* const kBaseTypeNames[] = {
    "Any",          "Null",      "Numeric", "Int",      "Int8",      "Int16",
    "Int32",        "UInt",      "UInt8",   "UInt16",   "UInt32",    "String",
    "Bytes",        "Bool",      "Float32", "Float64",  "Date",      "DateTime",
    "TimeInterval", "Timestamp", "Decimal", "Iterable", "Array",     "Tuple",
    "Set",          "Map",       "Struct",  "Function", "Generator", "Integral",
    "Container"};

static const char* const kNumericTypeNames[] = {
    "Int",    "Int8",   "Int16",   "Int32",   "UInt",    "UInt8",
    "UInt16", "UInt32", "Float32", "Float64", "Decimal", "Integral"};

std::string GetDefaultValue(absl::string_view type_name) {
  static const auto kDefaultValues =
      new absl::flat_hash_map<std::string, std::string>({
          {"Null", R"(literal { null_value: NULL_VALUE })"},
          {"Int", R"(literal { int_value: 0 })"},
          {"Int8", R"(literal { int_value: 0 })"},
          {"Int16", R"(literal { int_value: 0 })"},
          {"Int32", R"(literal { int_value: 0 })"},
          {"UInt", R"(literal { uint_value: 0 })"},
          {"UInt8", R"(literal { uint_value: 0 })"},
          {"UInt16", R"(literal { uint_value: 0 })"},
          {"UInt32", R"(literal { uint_value: 0 })"},
          {"String", R"(literal { str_value: "" })"},
          {"Bytes", R"(literal { bytes_value: "" })"},
          {"Bool", R"(literal { bool_value: true })"},
          {"Float32", R"(literal { float_value: 0 })"},
          {"Float64", R"(literal { double_value: 0 })"},
          {"Date", R"(function_call { identifier { name: "default_date" } })"},
          {"DateTime",
           R"(function_call { identifier { name: "default_datetime" } })"},
          {"TimeInterval", R"(literal { time_range { } })"},
          {"Timestamp",
           R"(function_call { identifier { name: "default_timestamp" } })"},
          {"Decimal",
           R"(function_call { identifier { name: "default_decimal" } })"},
      });
  auto it = kDefaultValues->find(type_name);
  if (it == kDefaultValues->end()) {
    return "";
  }
  return it->second;
}

absl::string_view BoolValue(bool value) {
  if (value) {
    return "true";
  } else {
    return "false";
  }
}

void TypesTest::CheckTypes(absl::Span<const absl::string_view> names,
                           absl::Span<const bool> is_ancestor,
                           absl::Span<const bool> is_convertible,
                           absl::Span<const bool> is_bound, int print_result) {
  CHECK_EQ(names.size() * names.size(), is_ancestor.size());
  CHECK_EQ(is_convertible.size(), is_ancestor.size());
  CHECK_EQ(names.size(), is_bound.size());
  size_t i = 0;
  if (print_result) {
    std::cout << "{" << std::endl;
  }
  for (absl::string_view name_i : names) {
    ASSERT_OK_AND_ASSIGN(auto type_i, FindType(name_i));
    size_t j = 0;
    for (absl::string_view name_j : names) {
      ASSERT_OK_AND_ASSIGN(auto type_j, FindType(name_j));
      const size_t index = i * names.size() + j;
      const bool ancestor = type_i->IsAncestorOf(*type_j);
      const bool convertible = type_i->IsConvertibleFrom(*type_j);
      if (!print_result) {
        EXPECT_EQ(ancestor, is_ancestor[index])
            << "For: " << name_i << " / " << name_j;
        EXPECT_EQ(i == j, type_i->IsEqual(*type_j))
            << "For: " << name_i << " / " << name_j;
        EXPECT_EQ(convertible, is_convertible[index])
            << "For: " << name_i << " / " << name_j;
      } else {
        std::cout << BoolValue(print_result == 1 ? ancestor : convertible)
                  << ", ";
      }
      ++j;
    }
    if (print_result) {
      std::cout << std::endl;
    }
    ++i;
  }
  if (print_result) {
    std::cout << "}" << std::endl;
  }
}

TEST_F(TypesTest, BaseTypes) {
  ASSERT_OK_AND_ASSIGN(auto any_type, FindType("Any"));
  std::set<std::string> kUnBoundTypes(
      {"Any", "Numeric", "Iterable", "Array", "Set", "Map", "Struct",
       "Function", "Tuple", "Container", "Integral", "Generator"});
  for (absl::string_view name : kBaseTypeNames) {
    ASSERT_OK_AND_ASSIGN(auto typespec, FindType(name));
    // std::cout << "Found type : `" << typespec->full_name() << "` - "
    //           << typespec->IsBound() << "\n";
    EXPECT_TRUE(any_type->IsAncestorOf(*typespec)) << "For: " << name;
    EXPECT_TRUE(any_type->IsConvertibleFrom(*typespec)) << "For: " << name;
    EXPECT_TRUE(typespec->IsAncestorOf(*typespec)) << "For: " << name;
    EXPECT_TRUE(typespec->IsConvertibleFrom(*typespec)) << "For: " << name;
    EXPECT_TRUE(typespec->IsEqual(*typespec)) << "For: " << name;
    if (typespec->IsBound()) {
      EXPECT_EQ(kUnBoundTypes.count(std::string(name)), 0) << "For: " << name;
    } else {
      EXPECT_EQ(kUnBoundTypes.count(std::string(name)), 1) << "For: " << name;
    }
    auto default_val = typespec->DefaultValueExpression(ScopeName());
    auto expected_default = GetDefaultValue(name);
    if (default_val.ok()) {
      EXPECT_THAT(default_val.value(), EqualsProto(expected_default));
    } else {
      EXPECT_EQ(expected_default, "");
    }
    auto clone = typespec->Clone();
    EXPECT_EQ(clone->type_id(), typespec->type_id());
  }
  ASSERT_OK_AND_ASSIGN(auto numeric_type, FindType("Numeric"));
  for (absl::string_view name : kNumericTypeNames) {
    ASSERT_OK_AND_ASSIGN(auto typespec, FindType(name));
    EXPECT_TRUE(numeric_type->IsAncestorOf(*typespec)) << "For: " << name;
    EXPECT_TRUE(numeric_type->IsConvertibleFrom(*typespec)) << "For: " << name;
  }
}

TEST_F(TypesTest, Iterables) {
  static const absl::string_view kTypeNames[] = {
      "Iterable",   "Iterable<Int>",    "Array",       "Array<Numeric>",
      "Array<Int>", "Array<Float32>",   "Array<Int8>", "Set",
      "Set<Int>",   "Map<Int, String>",
  };
  // clang-format off
  static const bool kIsAncestor[] = {
    // Iterable
    true, true, true, true, true, true, true, true, true, true,
    // Iterable<Int>
    false, true, false, false, true, false, true, false, true, false,
    // Array
    false, false, true, true, true, true, true, false, false, false,
    // Array<Numeric>
    false, false, false, true, true, true, true, false, false, false,
    // Array<Int>
    false, false, false, false, true, false, true, false, false, false,
    // Array<Float32>
    false, false, false, false, false, true, false, false, false, false,
    // Array<Int8>
    false, false, false, false, false, false, true, false, false, false,
    // Set
    false, false, false, false, false, false, false, true, true, false,
    // Set<Int>
    false, false, false, false, false, false, false, false, true, false,
    // Map<Int, String>
    false, false, false, false, false, false, false, false, false, true,
  };
  static const bool kIsConvertible[] = {
    // Iterable
    true, true, true, true, true, true, true, true, true, true,
    // Iterable<Int>
    false, true, false, false, true, false, false, false, true, false,
    // Array
    false, false, true, true, true, true, true, false, false, false,
    // Array<Numeric>
    false, false, false, true, true, true, true, false, false, false,
    // Array<Int>
    false, false, false, false, true, false, false, false, false, false,
    // Array<Float32>
    false, false, false, false, false, true, false, false, false, false,
    // Array<Int8>
    false, false, false, false, false, false, true, false, false, false,
    // Set
    false, false, false, false, false, false, false, true, true, false,
    // Set<Int>
    false, false, false, false, false, false, false, false, true, false,
    // Map<Int, String>
    false, false, false, false, false, false, false, false, false, true,
  };
  static const bool kIsBound[] = {
    false, false, false, false, true, true, true, false, true, true,
  };
  // clang-format on
  CheckTypes(kTypeNames, kIsAncestor, kIsConvertible, kIsBound);
}

TEST_F(TypesTest, Functions) {
  static const absl::string_view kTypeNames[] = {
      "Any",
      "Array<Int>",
      "Function",
      "Function<Numeric>",
      "Function<Int>",
      "Function<Numeric, Iterable<Numeric>, Numeric>",
      "Function<Int, Iterable<Int>, Numeric>",
      "Function<Numeric, Iterable<Int>, Int>",
      "Function<Int, Iterable<Int>, Int>",
      "Function<Int8, Iterable<Int8>, Int8>",
      "Function<String>"};
  // clang-format off
  static const bool kIsBound[] = {
    false, true, false, false, true, false, false, false, true, true, true,
  };
  static const bool kIsAncestor[] = {
    // Any
    true, true, true, true, true, true, true, true, true, true, true,
    // Array <int>
    false, true, false, false, false, false, false, false, false, false, false,
    // Function
    false, false, true, true, true, true, true, true, true, true, true,
    // Function<Numeric>
    false, false, false, true, true, false, false, false, false, false, false,
    // Function<Int>
    false, false, false, false, true, false, false, false, false, false, false,
    // Function<Numeric, Iterable<Numeric>, Numeric>
    false, false, false, false, false, true, true, true, true, true, false,
    // Function<Int, Iterable<Int>, Numeric>
    false, false, false, false, false, false, true, false, true, true, false,
    // Function<Numeric, Iterable<Int>, Int>
    false, false, false, false, false, false, false, true, true, true, false,
    // Function<Int, Iterable<Int>, Int>
    false, false, false, false, false, false, false, false, true, true, false,
    // Function<Int8, Iterable<Int8>, Int8>
    false, false, false, false, false, false, false, false, false, true, false,
    // Function<String>
    false, false, false, false, false, false, false, false, false, false, true,
  };
  static const bool kIsConvertible[] = {
    // Any
    true, true, true, true, true, true, true, true, true, true, true,
    // Array<Int>
    false, true, false, false, false, false, false, false, false, false, false,
    // Function
    false, false, true, true, true, true, true, true, true, true, true,
    // Function<Numeric>
    false, false, false, true, true, false, false, false, false, false, false,
    // Function<Int>
    false, false, false, false, true, false, false, false, false, false, false,
    // Function<Numeric, Iterable<Numeric>, Numeric>
    false, false, false, false, false, true, true, true, true, true, false,
    // Function<Int, Iterable<Int>, Numeric>
    false, false, false, false, false, false, true, false, true, false, false,
    // Function<Numeric, Iterable<Int>, Int>
    false, false, false, false, false, false, false, true, true, false, false,
    // Function<Int, Iterable<Int>, Int>
    false, false, false, false, false, false, false, false, true, false, false,
    // Function<Int8, Iterable<Int8>, Int8>
    false, false, false, false, false, false, false, false, false, true, false,
    // Function<String>
    false, false, false, false, false, false, false, false, false, false, true,
  };
  // clang-format on
  CheckTypes(kTypeNames, kIsAncestor, kIsConvertible, kIsBound);
  ASSERT_OK_AND_ASSIGN(auto fun, FindType("Function"));
  auto ffun = static_cast<const TypeFunction*>(fun);
  EXPECT_RAISES_WITH_MESSAGE_THAT(ffun->Bind({}).status(), InvalidArgument,
                                  testing::HasSubstr("Empty binding"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(ffun->BindWithFunction(*ffun).status(),
                                  InvalidArgument,
                                  testing::HasSubstr("Cannot bind abstract"));
}

TEST_F(TypesTest, Structs) {
  static const absl::string_view kTypeNames[] = {
      "Any",
      "Int",
      "Struct",
      "Struct<Numeric, Numeric, Function<Numeric>>",
      "Struct<Int, Int, Function<Numeric>>",
      "Struct<Int, Int8, Function<Int>>",
      "Struct<Int, Int, Function<Int>, Int>",
      "Struct<Float32, Float64>",
      "Struct<Struct<Numeric, String>, Numeric, Any>",
      "Struct<Struct<Int, String>, Float64, Function<Numeric>>",
      "Struct<Struct<Float64, String>, Int, Function<Int>>",
  };
  // clang-format off
  static const bool kIsBound[] = {
    false, true, false, false, false, true, true, true, false, false, true,
  };
  static const bool kIsAncestor[] = {
    // Any
    true, true, true, true, true, true, true, true, true, true, true,
    // Int
    false, true, false, false, false, false, false, false, false, false, false,
    // Struct
    false, false, true, true, true, true, true, true, true, true, true,
    // Struct<Numeric, Numeric, Function<Numeric>>
    false, false, false, true, true, true, false, false, false, false, false,
    // Struct<Int, Int, Function<Numeric>>
    false, false, false, false, true, true, false, false, false, false, false,
    // Struct<Int, Int8, Function<Int>>
    false, false, false, false, false, true, false, false, false, false, false,
    // Struct<Int, Int, Function<Int>, Int>
    false, false, false, false, false, false, true, false, false, false, false,
    // Struct<Float32, Float64>
    false, false, false, false, false, false, false, true, false, false, false,
    // Struct<Struct<Numeric, String>, Numeric, Any>
    false, false, false, false, false, false, false, false, true, true, true,
    // Struct<Struct<Int, String>, Float64, Function<Numeric>>
    false, false, false, false, false, false, false, false, false, true, false,
    // Struct<Struct<Float64, String>, Int, Function<Int>>
    false, false, false, false, false, false, false, false, false, false, true,
  };
  static const bool kIsConvertible[] = {
    // Any
    true, true, true, true, true, true, true, true, true, true, true,
    // Int
    false, true, false, false, false, false, false, false, false, false, false,
    // Struct
    false, false, true, true, true, true, true, true, true, true, true,
    // Struct<Numeric, Numeric, Function<Numeric>>
    false, false, false, true, true, true, false, false, false, false, false,
    // Struct<Int, Int, Function<Numeric>>
    false, false, false, false, true, true, false, false, false, false, false,
    // Struct<Int, Int8, Function<Int>>
    false, false, false, false, false, true, false, false, false, false, false,
    // Struct<Int, Int, Function<Int>, Int>
    false, false, false, false, false, false, true, false, false, false, false,
    // Struct<Float32, Float64>
    false, false, false, false, false, false, false, true, false, false, false,
    // Struct<Struct<Numeric, String>, Numeric, Any>
    false, false, false, false, false, false, false, false, true, true, true,
    // Struct<Struct<Int, String>, Float64, Function<Numeric>>
    false, false, false, false, false, false, false, false, false, true, false,
    // Struct<Struct<Float64, String>, Int, Function<Int>>
    false, false, false, false, false, false, false, false, false, false, true
  };
  // clang-format on
  CheckTypes(kTypeNames, kIsAncestor, kIsConvertible, kIsBound);
}

TEST_F(TypesTest, Unions) {
  static const absl::string_view kTypeNames[] = {
      "Int",
      "String",
      "Union",
      "Null",
      "Union<Bool, Numeric, Struct>",
      "Union<Numeric, String>",
      "Nullable<String>",
      "Nullable<Numeric>",
      "Union<Bool, Int, Struct<Nullable<Numeric>, String>>",
      "Union<Bool, Int32, Struct<Int, String>>",
      "Union<Int, String>",
      "Nullable<Int>",
  };
  // clang-format off
  static const bool kIsBound[] = {
    true, true, false, true, false, false,
    true, false, false, false, false, true,
  };
  static const bool kIsAncestor[] = {
    // Int
    true, false, false, false, false, false,
    false, false, false, false, false, false,
    // String
    false, true, false, false, false, false,
    false, false, false, false, false, false,
    // Union
    false, false, true, false, true, true,
    false, false, true, true, true, false,
    // Null
    false, false, false, true, false, false,
    false, false, false, false, false, false,
    // Union<Bool, Numeric, Struct>
    true, false, false, false, true, false,
    false, false, true, true, false, false,
    // Union<Numeric, String>
    true, true, false, false, false, true,
    false, false, false, false, true, false,
    // Nullable<String>
    false, true, false, true, false, false,
    true, false, false, false, false, false,
    // Nullable<Numeric>
    true, false, false, true, false, false,
    false, true, false, false, false, true,
    // Union<Bool, Int, Struct<Nullable<Numeric>, String>>
    true, false, false, false, false, false,
    false, false, true, true, false, false,
    // Union<Bool, Int32, Struct<Int, String>>
    false, false, false, false, false, false,
    false, false, false, true, false, false,
    // Union<Int, String>
    true, true, false, false, false, false,
    false, false, false, false, true, false,
    // Nullable<Int>
    true, false, false, true, false, false,
    false, false, false, false, false, true,
  };
  static const bool kIsConvertible[] = {
    // Int
    true, false, false, false, false, false,
    false, false, false, false, false, false,
    // String
    false, true, false, false, false, false,
    false, false, false, false, false, false,
    // Union
    false, false, true, false, true, true,
    false, false, true, true, true, false,
    // Null
    false, false, false, true, false, false,
    false, false, false, false, false, false,
    // Union<Bool, Numeric, Struct>
    true, false, false, false, true, false,
    false, false, true, true, false, false,
    // Union<Numeric, String>
    true, true, false, false, false, true,
    false, false, false, false, true, false,
    // Nullable<String>
    false, true, false, true, false, false,
    true, false, false, false, false, false,
    // Nullable<Numeric>
    true, false, false, true, false, false,
    false, true, false, false, false, true,
    // Union<Bool, Int, Struct<Nullable<Numeric>, String>>
    true, false, false, false, false, false,
    false, false, true, true, false, false,
    // Union<Bool, Int32, Struct<Int, String>>
    true, false, false, false, false, false,
    false, false, false, true, false, false,
    // Union<Int, String>
    true, true, false, false, false, false,
    false, false, false, false, true, false,
    // Nullable<Int>
    true, false, false, true, false, false,
    false, false, false, false, false, true,
  };
  // clang-format on
  CheckTypes(kTypeNames, kIsAncestor, kIsConvertible, kIsBound);

  ASSERT_OK_AND_ASSIGN(auto union1, FindType("Union<Numeric, String>"));
  ASSERT_OK_AND_ASSIGN(auto bound1, union1->Bind({FindType("Int").value()}));
  EXPECT_EQ(bound1->type_id(), pb::TypeId::INT_ID);
  ASSERT_OK_AND_ASSIGN(auto bound2, union1->Bind({FindType("String").value()}));
  EXPECT_EQ(bound2->type_id(), pb::TypeId::STRING_ID);
  ASSERT_OK_AND_ASSIGN(auto bound3,
                       union1->Bind({FindType("Union<Int8, String>").value()}));
  EXPECT_EQ(bound3->type_id(), pb::TypeId::UNION_ID);
  EXPECT_RAISES(union1->Bind({FindType("Bytes").value()}).status(),
                InvalidArgument);
  EXPECT_RAISES(union1->Bind({FindType("Nullable<Int>").value()}).status(),
                InvalidArgument);
  ASSERT_OK_AND_ASSIGN(auto union2, FindType("Nullable<Numeric>"));
  ASSERT_OK_AND_ASSIGN(auto bound4, union2->Bind({FindType("Int").value()}));
  EXPECT_EQ(bound4->type_id(), pb::TypeId::INT_ID);
  EXPECT_RAISES(FindType("Union<Int>").status(), InvalidArgument);
  EXPECT_TRUE(FindType("Nullable<Int>")
                  .value()
                  ->IsEqual(*FindType("Nullable<Nullable<Int>>").value()));
  EXPECT_RAISES(FindType("Nullable<Null>").status(), InvalidArgument);
  ASSERT_OK_AND_ASSIGN(auto null_type, FindType("Null"));
  ASSERT_OK_AND_ASSIGN(auto int_type, FindType("Int"));
  EXPECT_RAISES(null_type->Bind({int_type, int_type}).status(),
                InvalidArgument);
  EXPECT_EQ(null_type->Bind({int_type}).value()->full_name(), "Nullable<Int>");
  EXPECT_EQ(
      null_type->Bind({FindType("Nullable<Int>").value()}).value()->full_name(),
      "Nullable<Int>");
  ASSERT_OK_AND_ASSIGN(auto nullable_type, FindType("Nullable"));
  auto nullable_int = FindType("Nullable<Int>").value();
  EXPECT_TRUE(nullable_type->Bind({int_type, null_type})
                  .value()
                  ->IsEqual(*nullable_int));
  EXPECT_RAISES(nullable_int->Bind({FindType("String").value()}).status(),
                InvalidArgument);
  EXPECT_TRUE(nullable_int->Bind({FindType("Int8").value()})
                  .value()
                  ->IsEqual(*FindType("Int8").value()));
  EXPECT_TRUE(nullable_int->Clone()->IsEqual(*nullable_int));
}

TEST_F(TypesTest, Tuples) {
  static const absl::string_view kTypeNames[] = {
      "Tuple",
      "Tuple<Numeric, String>",
      "Tuple<Int, String>",
      "Tuple<Any, Struct, Numeric>",
      "Tuple<Int, Struct<Int, String>, Integral>",
      "Tuple<Int, Struct<Int, String>, String>",
      "Tuple<Int8, Struct<Int8, String>, UInt>",
      "Tuple<Int, Nullable<String>>",
  };
  // clang-format off
  static const bool kIsBound[] = {
    false, false, true, false, false, true, true, true
  };
  static const bool kIsAncestor[] = {
    // Tuple
    true, true, true, true, true, true, true, true,
    // Tuple<Numeric, Nullable<String>>
    false, true, true, false, false, false, false, false,
    // Tuple<Int, String>
    false, false, true, false, false, false, false, false,
    // Tuple<Any, Struct, Numeric>
    false, false, false, true, true, false, true, false,
    // Tuple<Int, Struct<Int, String>, Integral>
    false, false, false, false, true, false, true, false,
    // Tuple<Int, Struct<Int, String>, String>
    false, false, false, false, false, true, false, false,
    // Tuple<Int8, Struct<Int8, String>, UInt>
    false, false, false, false, false, false, true, false,
    // Tuple<Int, Nullable<String>>
    false, false, true, false, false, false, false, true,
  };
  static const bool kIsConvertible[] = {
    // Tuple
    true, true, true, true, true, true, true, true,
    // Tuple<Numeric, String>
    false, true, true, false, false, false, false, false,
    // Tuple<Int, String>
    false, false, true, false, false, false, false, false,
    // Tuple<Any, Struct, Numeric>
    false, false, false, true, true, false, true, false,
    // Tuple<Int, Struct<Int, String>, Integral>
    false, false, false, false, true, false, false, false,
    // Tuple<Int, Struct<Int, String>, String>
    false, false, false, false, false, true, false, false,
    // Tuple<Int8, Struct<Int8, String>, UInt>
    false, false, false, false, false, false, true, false,
    // Tuple<Int, Nullable<String>>
    false, false, false, false, false, false, false, true,
  };
  // clang-format on
  CheckTypes(kTypeNames, kIsAncestor, kIsConvertible, kIsBound);
}

TEST_F(TypesTest, Stores) {
  ASSERT_OK_AND_ASSIGN(auto scope_name1, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name1)));
  ASSERT_OK_AND_ASSIGN(auto scope_name2, ScopeName::Parse("foo.bar.baz"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name2)));
  EXPECT_RAISES(store_.AddScope(std::make_shared<ScopeName>(scope_name2)),
                AlreadyExists);
  ASSERT_OK_AND_ASSIGN(auto t1, FindType("{T: Iterable<Numeric>}", "foo.bar"));
  ASSERT_OK_AND_ASSIGN(auto t2, FindType("{T: Int}", "foo.bar.baz"));
  ASSERT_OK_AND_ASSIGN(auto t3, FindType("T", "foo.bar.baz"));
  EXPECT_EQ(t3, t2);
  ASSERT_OK_AND_ASSIGN(auto t4, FindType("T", "foo.bar"));
  EXPECT_EQ(t4, t1);
  EXPECT_RAISES(FindType("{T: String}", "foo.bar").status(), AlreadyExists);
  ASSERT_OK_AND_ASSIGN(
      auto t5, FindType("{C: Iterable<{D: Array<Numeric>}>}", "foo.bar"));
  ASSERT_OK_AND_ASSIGN(auto t6, FindType("{D: Array<Numeric>}", "foo.bar.baz"));
  ASSERT_OK_AND_ASSIGN(auto t7, FindType("D", "foo.bar"));
  ASSERT_OK_AND_ASSIGN(auto t8, FindType("C", "foo.bar"));
  EXPECT_TRUE(t7->IsEqual(*t6));
  EXPECT_EQ(t5, t8);
}

TEST_F(TypesTest, FunctionResultBinding) {
  ASSERT_OK_AND_ASSIGN(auto scope_name1, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name1)));
  ASSERT_OK_AND_ASSIGN(auto f1,
                       FindType("Function<{X:Numeric}, X, X>", "foo.bar"));
  EXPECT_EQ(f1->full_name(),
            "Function<{ X : Numeric }"
            "(arg_1: { X : Numeric }, arg_2: { X : Numeric })>");
  for (const auto param : f1->parameters()) {
    EXPECT_EQ(param->full_name(), "{ X : Numeric }");
  }
  ASSERT_OK_AND_ASSIGN(auto f2, FindType("X", "foo.bar"));
  EXPECT_EQ(f2->full_name(), "{ X : Numeric }");
  LocalNamesRebinder rebinder_1;
  EXPECT_OK(rebinder_1.ProcessType(f1->parameters().front(),
                                   FindType("Int").value()));
  ASSERT_OK_AND_ASSIGN(auto f3, rebinder_1.RebuildType(f1, f1));
  EXPECT_EQ(f3->full_name(), "Function<Int(arg_1: Int, arg_2: Int)>");

  ASSERT_OK_AND_ASSIGN(
      auto f4, FindType("Function<Nullable<{Y:Numeric}>, Int, Y>", "foo.bar"));
  EXPECT_EQ(f4->full_name(),
            "Function<{ Y : Numeric }"
            "(arg_1: Nullable<{ Y : Numeric }>, arg_2: Int)>");
  ASSERT_OK_AND_ASSIGN(auto fy, FindType("Y", "foo.bar"));
  EXPECT_EQ(fy->full_name(), "{ Y : Numeric }");
  LocalNamesRebinder rebinder_2;
  EXPECT_OK(rebinder_2.ProcessType(f4->parameters().front(),
                                   FindType("Nullable<Int>").value()));
  ASSERT_OK_AND_ASSIGN(auto f5, rebinder_2.RebuildType(f4, f4));
  EXPECT_EQ(f5->full_name(), "Function<Int(arg_1: Nullable<Int>, arg_2: Int)>");
  EXPECT_OK(
      rebinder_2.ProcessType(f4->parameters()[1], FindType("Int32").value()));
  ASSERT_OK_AND_ASSIGN(auto f6, rebinder_2.RebuildType(f4, f4));
  EXPECT_EQ(f6->full_name(), "Function<Int(arg_1: Nullable<Int>, arg_2: Int)>");
}

TEST_F(TypesTest, SubFunctionResultBinding) {
  ASSERT_OK_AND_ASSIGN(auto scope_name1, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name1)));
  ASSERT_OK_AND_ASSIGN(
      auto f1,
      FindType("Function<Iterable<{X : Any}>, Function<X, {Y: Any}>, Array<Y>>",
               "foo.bar"));
  std::vector<const TypeSpec*> types{FindType("Array<Int>").value(),
                                     FindType("Function<Any, String>").value(),
                                     f1->parameters().back()};
  LocalNamesRebinder rebinder;
  EXPECT_OK(rebinder.ProcessType(f1->parameters()[0], types[0]));
  EXPECT_OK(rebinder.ProcessType(f1->parameters()[1], types[1]));
  ASSERT_OK_AND_ASSIGN(auto f2,
                       rebinder.RebuildFunctionWithComponents(f1, types));
  EXPECT_EQ(
      "Function<Array<String>(arg_1: Array<Int>, "
      "arg_2: Function<String(arg_1: Int)>)>",
      f2->full_name());
}

TEST_F(TypesTest, UnionResultBinding) {
  ASSERT_OK_AND_ASSIGN(auto scope_name1, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name1)));
  ASSERT_OK_AND_ASSIGN(
      auto f1,
      FindType("Function<Array<{X : Union<String, Bytes, Numeric>}>, X>",
               "foo.bar"));
  std::vector<const TypeSpec*> types{FindType("Array<Int>").value(),
                                     f1->parameters().back()};
  LocalNamesRebinder rebinder;
  EXPECT_OK(rebinder.ProcessType(f1->parameters()[0], types[0]));
  ASSERT_OK_AND_ASSIGN(auto f2,
                       rebinder.RebuildFunctionWithComponents(f1, types));
  EXPECT_EQ("Function<Int(arg_1: Array<Int>)>", f2->full_name());
}

TEST_F(TypesTest, TypeMemberStore) {
  ASSERT_OK_AND_ASSIGN(auto scope_name1, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name1)));
  auto type_int = const_cast<TypeSpec*>(FindType("Int").value());
  EXPECT_EQ(type_int->kind(), pb::ObjectKind::OBJ_TYPE);
  EXPECT_TRUE(type_int->name_store().has_value());
  EXPECT_EQ(type_int->ResultType(), nullptr);
  EXPECT_THAT(type_int->ToProto(),
              EqualsProto(R"pb(type_id: INT_ID name: "Int")pb"));
  auto type_numeric = const_cast<TypeSpec*>(FindType("Numeric").value());
  auto type_integral = const_cast<TypeSpec*>(FindType("Integral").value());
  EXPECT_EQ(type_int->type_member_store()->type_spec(), type_int);
  EXPECT_EQ(
      static_cast<TypeMemberStore*>(type_int->type_member_store())->ancestor(),
      type_integral->type_member_store());
  EXPECT_EQ(static_cast<TypeMemberStore*>(type_integral->type_member_store())
                ->ancestor(),
            type_numeric->type_member_store());
  EXPECT_EQ(type_int->type_member_store()->kind(),
            pb::ObjectKind::OBJ_TYPE_MEMBER_STORE);
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      type_int->type_member_store()->AddName("foo", type_numeric),
      InvalidArgument, testing::HasSubstr("only be fields or methods"));
  Field f("foo", type_int, type_int, type_int->type_member_store());
  EXPECT_OK(type_int->type_member_store()->AddName("foo", &f));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      type_int->type_member_store()->AddName("f--", &f), InvalidArgument,
      testing::HasSubstr("valid local name"));
  auto ftype = const_cast<TypeSpec*>(
      FindType("Function<Nullable<{Y:Numeric}>, Int, Y>", "foo.bar").value());
  EXPECT_THAT(ftype->ToProto(), EqualsProto(R"pb(
                type_id: FUNCTION_ID
                name: "Function"
                parameter {
                  type_id: NULLABLE_ID
                  name: "Nullable"
                  parameter { type_id: NULL_ID name: "Null" }
                  parameter {
                    type_id: NUMERIC_ID
                    name: "Numeric"
                    scope_name { module_name: "foo" module_name: "bar" }
                  }
                  scope_name { module_name: "foo" module_name: "bar" }
                }
                parameter { type_id: INT_ID name: "Int" }
                parameter {
                  type_id: NUMERIC_ID
                  name: "Numeric"
                  scope_name { module_name: "foo" module_name: "bar" }
                }
                parameter_name: "arg_1"
                parameter_name: "arg_2"
                scope_name { module_name: "foo" module_name: "bar" }
              )pb"));

  EXPECT_OK(ftype->set_name("foobarsky"));
  EXPECT_RAISES(ftype->set_name("foobarsky"), FailedPrecondition);
  EXPECT_EQ(ftype->name(), "foobarsky");
  EXPECT_RAISES(type_int->set_name("x-"), InvalidArgument);
}

TEST(TypeNames, TypeNames) {
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::ANY_ID), kTypeNameAny);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::NULL_ID), kTypeNameNull);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::NUMERIC_ID), kTypeNameNumeric);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::INT_ID), kTypeNameInt);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::INT8_ID), kTypeNameInt8);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::INT16_ID), kTypeNameInt16);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::INT32_ID), kTypeNameInt32);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::UINT_ID), kTypeNameUInt);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::UINT8_ID), kTypeNameUInt8);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::UINT16_ID), kTypeNameUInt16);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::UINT32_ID), kTypeNameUInt32);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::STRING_ID), kTypeNameString);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::BYTES_ID), kTypeNameBytes);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::BOOL_ID), kTypeNameBool);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::FLOAT32_ID), kTypeNameFloat32);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::FLOAT64_ID), kTypeNameFloat64);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::DATE_ID), kTypeNameDate);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::DATETIME_ID),
            kTypeNameDateTime);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::TIMEINTERVAL_ID),
            kTypeNameTimeInterval);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::TIMESTAMP_ID),
            kTypeNameTimestamp);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::DECIMAL_ID), kTypeNameDecimal);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::ITERABLE_ID),
            kTypeNameIterable);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::ARRAY_ID), kTypeNameArray);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::TUPLE_ID), kTypeNameTuple);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::SET_ID), kTypeNameSet);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::MAP_ID), kTypeNameMap);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::STRUCT_ID), kTypeNameStruct);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::FUNCTION_ID),
            kTypeNameFunction);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::UNION_ID), kTypeNameUnion);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::NULLABLE_ID),
            kTypeNameNullable);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::DATASET_ID), kTypeNameDataset);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::TYPE_ID), kTypeNameType);
  EXPECT_EQ(TypeUtils::BaseTypeName(pb::TypeId::MODULE_ID), kTypeNameModule);
  EXPECT_EQ(TypeUtils::BaseTypeName(static_cast<pb::TypeId>(10000)),
            kTypeNameUnknown);
}

TEST_F(TypesTest, Decimal) {
  ASSERT_OK_AND_ASSIGN(auto type_dec, FindType("Decimal"));
  EXPECT_EQ(type_dec->full_name(), "Decimal");
  EXPECT_EQ(type_dec->type_spec()->type_id(), pb::TypeId::TYPE_ID);
  EXPECT_EQ(type_dec->type_spec()->Clone()->type_id(), pb::TypeId::TYPE_ID);
  ASSERT_OK_AND_ASSIGN(auto td, type_dec->Bind({10, 3}));
  EXPECT_EQ(td->full_name(), "Decimal<10, 3>");
  EXPECT_THAT(td->ToProto(), EqualsProto(R"pb(type_id: DECIMAL_ID
                                              name: "Decimal"
                                              parameter_value: 10
                                              parameter_value: 3)pb"));
  EXPECT_THAT(td->ToTypeSpecProto(ScopeName()), EqualsProto(R"pb(
                identifier { name: "Decimal" }
                argument { int_value: 10 }
                argument { int_value: 3 }
              )pb"));
  EXPECT_RAISES(td->Bind({10, 2}).status(), InvalidArgument);
  EXPECT_RAISES(type_dec->Bind({10, 11}).status(), InvalidArgument);
  EXPECT_RAISES(type_dec->Bind({0, 0}).status(), InvalidArgument);
  EXPECT_RAISES(type_dec->Bind({10, -1}).status(), InvalidArgument);
  EXPECT_RAISES(type_dec->Bind({10, 1, 22}).status(), InvalidArgument);
  EXPECT_RAISES(type_dec->Bind({TypeDecimal::kMaxPrecision + 1, 1}).status(),
                InvalidArgument);
}

TEST_F(TypesTest, Dataset) {
  ASSERT_OK_AND_ASSIGN(auto type_dset, FindType("Dataset"));
  EXPECT_EQ(type_dset->full_name(), "Dataset<Any>");
  EXPECT_EQ(type_dset->ResultType()->full_name(), "Any");
  EXPECT_EQ(type_dset->Clone()->full_name(), "Dataset<Any>");
  auto type_int = const_cast<TypeSpec*>(FindType("Int").value());
  ASSERT_OK_AND_ASSIGN(auto td2, type_dset->Bind({type_int}));
  EXPECT_EQ(td2->full_name(), "Dataset<Int>");
  EXPECT_EQ(td2->ResultType()->full_name(), "Int");
  EXPECT_RAISES(type_dset->Bind({}).status(), InvalidArgument);
  EXPECT_RAISES(type_dset->Bind({type_int, type_int}), InvalidArgument);
  EXPECT_RAISES(type_dset->Bind({FindType("Function").value()}),
                InvalidArgument);
  EXPECT_RAISES(td2->Bind({FindType("String").value()}), InvalidArgument);
}

TEST_F(TypesTest, Struct) {
  ASSERT_OK_AND_ASSIGN(auto type_struct, FindType("Struct"));
  EXPECT_RAISES(type_struct->Bind({}).status(), InvalidArgument);
  auto type_int = FindType("Int").value();
  auto type_int8 = FindType("Int8").value();
  ASSERT_OK_AND_ASSIGN(auto struct1, type_struct->Bind({type_int}));
  EXPECT_THAT(struct1->ToProto(), EqualsProto(R"pb(
                type_id: STRUCT_ID
                name: "Struct"
                parameter { type_id: INT_ID name: "Int" }
                parameter_name: "field_0"
              )pb"));
  ASSERT_OK_AND_ASSIGN(auto struct2, struct1->Bind({type_int8}));
  EXPECT_THAT(struct2->ToProto(), EqualsProto(R"pb(
                type_id: STRUCT_ID
                name: "Struct"
                parameter { type_id: INT8_ID name: "Int8" }
                parameter_name: "field_0"
              )pb"));
  StructMemberStore sm(type_struct, type_struct->type_member_store_ptr());
  EXPECT_RAISES(sm.AddFields({TypeStruct::Field{"x-", type_int}}),
                InvalidArgument);
  EXPECT_RAISES(sm.AddFields({TypeStruct::Field{"x", type_int},
                              TypeStruct::Field{"x", type_int}}),
                AlreadyExists);
  EXPECT_RAISES(sm.AddFields({}), FailedPrecondition);
}

TEST_F(TypesTest, StoredTypeSpec) {
  auto type_int = FindType("Int").value();
  StoredTypeSpec stored_type(&store_, pb::TypeId::BOOL_ID, "Foo", nullptr, true,
                             type_int, {});
  EXPECT_EQ(stored_type.type_spec()->type_id(), pb::TypeId::TYPE_ID);
  EXPECT_EQ(stored_type.Clone()->type_id(), pb::TypeId::BOOL_ID);
}

TEST_F(TypesTest, Unknown) {
  EXPECT_EQ(TypeUnknown::Instance()->type_id(), pb::TypeId::UNKNOWN_ID);
  EXPECT_EQ(TypeUnknown::Instance()->type_spec(), TypeUnknown::Instance());
  EXPECT_TRUE(TypeUnknown::Instance()->scope_name().name().empty());
}

TEST_F(TypesTest, TypeStore) {
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name)));
  EXPECT_OK(store_.AddAlias(scope_name, ScopeName::Parse("qux").value()));
  EXPECT_RAISES(store_.AddAlias(ScopeName::Parse("foo.bar1").value(),
                                ScopeName::Parse("qux1").value()),
                NotFound);
  EXPECT_RAISES(store_.AddAlias(scope_name, ScopeName::Parse("qux").value()),
                AlreadyExists);
  EXPECT_EQ(store_.scope_name().name(), "");
  EXPECT_FALSE(store_.DebugNames().empty());
  EXPECT_FALSE(store_.mutable_base_store()->DebugNames().empty());
  EXPECT_FALSE(store_.base_store().DebugNames().empty());
  ASSERT_OK_AND_ASSIGN(
      auto f1,
      FindType("Function<Array<{X : Union<String, Bytes, Numeric>}>, X>",
               "foo.bar"));
  EXPECT_EQ(f1->type_id(), pb::TypeId::FUNCTION_ID);
  auto type_int = const_cast<TypeSpec*>(FindType("Int").value());
  ASSERT_OK_AND_ASSIGN(auto zoom_name, ScopeName::Parse("zoom"));
  ASSERT_OK_AND_ASSIGN(
      auto boom, store_.DeclareType(zoom_name, "Boom", type_int->Clone()));
  EXPECT_EQ(boom->type_id(), type_int->type_id());
  pb::TypeSpec ts;
  ts.mutable_identifier()->add_name("foo");
  EXPECT_RAISES(store_.FindType(ScopeName::Parse("some").value(), ts).status(),
                NotFound);
  auto boom_store = store_.FindStore("zoom");
  ASSERT_TRUE(boom_store.has_value());
  EXPECT_FALSE(boom_store.value()->DebugNames().empty());
  pb::TypeSpec ts2;
  ts2.mutable_identifier()->add_name("Boom2");
  EXPECT_RAISES(boom_store.value()->FindType(zoom_name, ts2).status(),
                NotFound);
  EXPECT_RAISES(boom_store.value()
                    ->DeclareType(zoom_name, "Boom", type_int->Clone())
                    .status(),
                AlreadyExists);
  // Test creation w/ provided base types store:
  GlobalTypeStore s2(std::make_unique<BaseTypesStore>(&store_));
  // Test missing scope on local type:
  ASSERT_OK_AND_ASSIGN(auto type_spec, grammar::ParseTypeSpec("{Foobar}"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(s2.FindType(scope_name, *type_spec).status(),
                                  NotFound, testing::HasSubstr("not created"));
  // Test dedup:
  EXPECT_TRUE(TypeUtils::DedupTypes({}).empty());
  std::vector<const TypeSpec*> types;
  types.emplace_back(type_int);
  types.emplace_back(FindType("Bool").value());
  types.emplace_back(type_int);
  EXPECT_EQ(TypeUtils::DedupTypes({&types[0], types.size()}).size(), 2);
}

TEST_F(TypesTest, TypesFromBindings) {
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name)));
  ASSERT_OK_AND_ASSIGN(
      auto f, FindType("Function<Array<Int>, String, Int>", "foo.bar"));

  auto type_int = FindType("Int").value();
  EXPECT_RAISES_WITH_MESSAGE_THAT(type_int->TypesFromBindings({type_int}),
                                  InvalidArgument,
                                  testing::HasSubstr("Expecting 0 arguments"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(f->TypesFromBindings({20}), InvalidArgument,
                                  testing::HasSubstr("Expecting only types"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(f->TypesFromBindings({type_int}),
                                  InvalidArgument,
                                  testing::HasSubstr("Expecting an argument"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(f->TypesFromBindings({}, true, 1),
                                  InvalidArgument,
                                  testing::HasSubstr("Expecting at least 1"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(f->TypesFromBindings({}), InvalidArgument,
                                  testing::HasSubstr("Expecting 3 arguments"));
}

TEST_F(TypesTest, FunctionAncestry) {
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name)));
  ASSERT_OK_AND_ASSIGN(auto f1,
                       FindType("Function<Union<Integral, String>, Bool>"));
  ASSERT_OK_AND_ASSIGN(
      auto f2, FindType("Function<Nullable<Union<Integral, String>>, Bool>"));
  EXPECT_TRUE(f2->IsAncestorOf(*f1));
  EXPECT_FALSE(f1->IsAncestorOf(*f2));
  EXPECT_FALSE(f1->IsEqual(*f2));
}

TEST_F(TypesTest, TupleJoinsSimple) {
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar"));
  ASSERT_OK(store_.AddScope(std::make_shared<ScopeName>(scope_name)));
  ASSERT_OK_AND_ASSIGN(auto t1, FindType("Tuple<Int, String, Float64>"));
  ASSERT_OK_AND_ASSIGN(auto tt1, FindType("TupleJoin<Int, String, Float64>"));
  EXPECT_TRUE(t1->IsEqual(*tt1))
      << t1->full_name() << " / " << tt1->full_name();
  EXPECT_TRUE(tt1->Clone()->IsEqual(*tt1));
  ASSERT_OK_AND_ASSIGN(auto tt2,
                       FindType("TupleJoin<Int, Tuple<String, Float64>>"));
  EXPECT_RAISES(tt2->Bind({}), InvalidArgument);
  EXPECT_TRUE(!t1->IsEqual(*tt2));
  ASSERT_OK_AND_ASSIGN(auto tt2_bound,
                       tt2->Bind({FindType("Int").value(),
                                  FindType("Tuple<String, Float64>").value()}));
  EXPECT_TRUE(t1->IsEqual(*tt2_bound))
      << t1->full_name() << " / " << tt2_bound->full_name();
  EXPECT_TRUE(tt2->IsAncestorOf(*tt2_bound));
  EXPECT_TRUE(tt2->IsConvertibleFrom(*tt2_bound));
  ASSERT_OK_AND_ASSIGN(auto tt3,
                       FindType("TupleJoin<Tuple<Int, String, Float64>>"));
  EXPECT_TRUE(!t1->IsEqual(*tt3));
  ASSERT_OK_AND_ASSIGN(
      auto tt3_bound,
      tt3->Bind({FindType("Tuple<Int, String, Float64>").value()}));
  EXPECT_TRUE(t1->IsEqual(*tt3_bound))
      << t1->full_name() << " / " << tt3_bound->full_name();
  ASSERT_OK_AND_ASSIGN(auto tt4,
                       FindType("TupleJoin<Tuple<Int, String>, Float64>>"));
  EXPECT_TRUE(!t1->IsEqual(*tt4));
  ASSERT_OK_AND_ASSIGN(auto tt4_bound,
                       tt4->Bind({FindType("Tuple<Int, String>").value(),
                                  FindType("Float64").value()}));
  EXPECT_TRUE(t1->IsEqual(*tt4_bound))
      << t1->full_name() << " / " << tt4_bound->full_name();
}

TEST_F(TypesTest, FunctionDefault) {
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar"));
  ASSERT_OK_AND_ASSIGN(auto f1, FindType("Function<Int, String, Bool>"));
  ASSERT_OK_AND_ASSIGN(auto f1_default, f1->DefaultValueExpression(scope_name));
  EXPECT_THAT(
      f1_default, EqualsProto(R"pb(
        lambda_def {
          param {
            name: "arg_1"
            type_spec { identifier { name: "Int" } }
          }
          param {
            name: "arg_2"
            type_spec { identifier { name: "String" } }
          }
          result_type { identifier { name: "Bool" } }
          expression_block { expression { literal { bool_value: true } } }
        })pb"));
}

TEST_F(TypesTest, FunctionBindWithComponents) {
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar"));
  ASSERT_OK_AND_ASSIGN(auto f, FindType("Function"));
  auto args = std::vector<TypeFunction::Argument>(
      {TypeFunction::Argument{"int_arg", "", FindType("Int").value()},
       TypeFunction::Argument{"str_arg", "",
                              FindType("Array<String>").value()}});
  auto res_type = FindType("Bool").value();
  auto ff = static_cast<const TypeFunction*>(f);
  EXPECT_RAISES(ff->BindWithComponents(args, res_type, 3).status(),
                FailedPrecondition);
  ASSERT_OK_AND_ASSIGN(auto f2, ff->BindWithComponents(args, res_type, {}));
  EXPECT_EQ(f2->full_name(),
            "Function<Bool(int_arg: Int, str_arg: Array<String>)>");
  auto ff2 = static_cast<const TypeFunction*>(f2.get());
  ASSERT_OK_AND_ASSIGN(auto f3, ff2->BindWithComponents(args, res_type, {}));
  EXPECT_EQ(f3->full_name(),
            "Function<Bool(int_arg: Int, str_arg: Array<String>)>");
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      ff2->BindWithComponents(args, FindType("String").value(), {}).status(),
      InvalidArgument,
      testing::HasSubstr("is not compatible with function result type"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      ff2->BindWithComponents(std::vector<TypeFunction::Argument>({args[0]}),
                              res_type, {})
          .status(),
      InvalidArgument, testing::HasSubstr("Not enough arguments"));
  auto args_many = args;
  args_many.emplace_back(TypeFunction::Argument{"bool_arg", "", res_type});
  args_many.emplace_back(TypeFunction::Argument{"bool_arg2", "", res_type});
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      ff2->BindWithComponents(args_many, res_type, {}).status(),
      InvalidArgument, testing::HasSubstr("Too many arguments"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      ff2->BindWithComponents(args_many, res_type, 3).status(), InvalidArgument,
      testing::HasSubstr("Too many arguments"));
  auto args_bad = std::vector<TypeFunction::Argument>(
      {TypeFunction::Argument{"int_arg", "", FindType("Int").value()},
       TypeFunction::Argument{"str_arg", "", FindType("String").value()}});
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      ff2->BindWithComponents(args_bad, res_type, {}).status(), InvalidArgument,
      testing::HasSubstr("Bind argument at index: 1"));
  const_cast<TypeFunction*>(ff2)->set_first_default_value_index(1);
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      ff2->BindWithComponents(args, res_type, {}).status(), InvalidArgument,
      testing::HasSubstr("Cannot bind function"));
}

struct AggregateTupleBuilder {
  AggregateTupleBuilder(const TypeTuple* base_type, const TypeSpec* val_type)
      : base_type(base_type), val_type(val_type) {
    names.emplace_back("_agg");
    types.emplace_back(val_type);
  }
  AggregateTupleBuilder& AddAggregate(absl::string_view aggregate_type,
                                      absl::string_view field_name,
                                      const TypeSpec* type_spec) {
    new_types.emplace_back(std::make_unique<TypeTuple>(
        base_type->type_store(), base_type->type_member_store_ptr(),
        std::vector<const TypeSpec*>({type_spec}),
        std::vector<std::string>({std::string(field_name)})));
    names.emplace_back(aggregate_type);
    types.emplace_back(new_types.back().get());
    return *this;
  }
  const TypeSpec* Build() {
    new_types.emplace_back(std::make_unique<TypeTuple>(
        base_type->type_store(), base_type->type_member_store_ptr(),
        std::move(types), std::move(names)));
    return new_types.back().get();
  }
  const TypeTuple* base_type;
  const TypeSpec* val_type;
  std::vector<std::string> names;
  std::vector<const TypeSpec*> types;
  std::vector<std::unique_ptr<TypeSpec>> new_types;
};

TEST_F(TypesTest, Aggregate) {
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(FindType("DatasetAggregate<1>").status(),
                                  InvalidArgument,
                                  testing::HasSubstr("Extracting types"));
  ASSERT_OK_AND_ASSIGN(auto agg_type, FindType("DatasetAggregate"));
  ASSERT_OK_AND_ASSIGN(auto ttype, FindType("Tuple"));
  auto tuple_type = static_cast<const TypeTuple*>(ttype);
  ASSERT_OK_AND_ASSIGN(auto struct_type, FindType("Struct<Int, String>"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      agg_type->Bind({FindType("Int").value()}).status(), InvalidArgument,
      testing::HasSubstr("expected to be a tuple"));

  AggregateTupleBuilder builder1(tuple_type, struct_type);
  builder1.AddAggregate("count", "my_count", FindType("Int").value())
      .AddAggregate("sum", "my_sum", FindType("Int").value())
      .AddAggregate("to_set", "my_set", FindType("String").value())
      .AddAggregate("to_array", "my_array", FindType("Bytes").value());
  ASSERT_OK_AND_ASSIGN(auto agg1, agg_type->Bind({builder1.Build()}));
  EXPECT_EQ(agg1->type_id(), pb::TypeId::DATASET_ID);
  EXPECT_TRUE(agg_type->IsAncestorOf(*agg1));
  EXPECT_TRUE(agg_type->IsConvertibleFrom(*agg1));
  ASSERT_TRUE(agg1->ResultType() != nullptr);
  auto pb = agg1->ResultType()->ToProto();
  pb.clear_name();
  EXPECT_THAT(pb, EqualsProto(R"pb(
                type_id: STRUCT_ID
                parameter { type_id: INT_ID name: "Int" }
                parameter { type_id: INT_ID name: "Int" }
                parameter {
                  type_id: SET_ID
                  name: "Set"
                  parameter { type_id: STRING_ID name: "String" }
                }
                parameter {
                  type_id: ARRAY_ID
                  name: "Array"
                  parameter { type_id: BYTES_ID name: "Bytes" }
                }
                parameter_name: "my_count"
                parameter_name: "my_sum"
                parameter_name: "my_set"
                parameter_name: "my_array"
              )pb"));
  EXPECT_TRUE(agg1->Clone()->IsEqual(*agg1));
  ASSERT_OK_AND_ASSIGN(
      auto agg_base,
      agg_type->Build({FindType("Tuple<Struct<Int, String>, Tuple>").value()}));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      agg_type->Bind({FindType("Int").value(), FindType("Int").value()})
          .status(),
      InvalidArgument, testing::HasSubstr("Expecting exactly one"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      agg_base->Bind({FindType("Int").value()}).status(), InvalidArgument,
      testing::HasSubstr("Extracting types"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      agg_type->Bind({FindType("Int").value()}).status(), InvalidArgument,
      testing::HasSubstr("expected to be a tuple"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      agg_type->Bind({FindType("Tuple<Int, Int>").value()}).status(),
      InvalidArgument, testing::HasSubstr("badly built at index: 1"));
  AggregateTupleBuilder builder2(tuple_type, struct_type);
  builder2.AddAggregate("sum", "my_sum", FindType("String").value());
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      agg_type->Bind({builder2.Build()}).status(), InvalidArgument,
      testing::HasSubstr("expects a numeric value"));
  AggregateTupleBuilder builder3(tuple_type, struct_type);
  builder3.AddAggregate("sum", "my_sum", FindType("Function").value());
  EXPECT_RAISES_WITH_MESSAGE_THAT(agg_type->Bind({builder3.Build()}).status(),
                                  InvalidArgument,
                                  testing::HasSubstr("Abstract function"));
  AggregateTupleBuilder builder4(tuple_type, struct_type);
  builder4.AddAggregate("count", "my_count", FindType("Int").value())
      .AddAggregate("sum", "my_count", FindType("Int").value());
  EXPECT_RAISES_WITH_MESSAGE_THAT(agg_type->Bind({builder4.Build()}).status(),
                                  InvalidArgument,
                                  testing::HasSubstr("Duplicated field name"));
  AggregateTupleBuilder builder5(tuple_type, struct_type);
  builder5.AddAggregate("count", "arg_2", FindType("Int").value())
      .AddAggregate("sum", "", FindType("Int").value())
      .AddAggregate("count", "", FindType("Int").value())
      .AddAggregate("sum", "", FindType("Int").value());
  ASSERT_OK_AND_ASSIGN(auto agg2, agg_type->Bind({builder5.Build()}));
  pb = agg2->ResultType()->ToProto();
  pb.clear_name();
  EXPECT_THAT(pb, EqualsProto(R"pb(
                type_id: STRUCT_ID
                parameter { type_id: INT_ID name: "Int" }
                parameter { type_id: INT_ID name: "Int" }
                parameter { type_id: INT_ID name: "Int" }
                parameter { type_id: INT_ID name: "Int" }
                parameter_name: "arg_2"
                parameter_name: "arg_3"
                parameter_name: "arg_4"
                parameter_name: "arg_5"
              )pb"));
}

struct JoinTupleBuilder {
  JoinTupleBuilder(const TypeTuple* base_type, const TypeSpec* dataset_type)
      : base_type(base_type), dataset_type(dataset_type) {}
  JoinTupleBuilder& AddJoin(absl::string_view join_type,
                            absl::string_view join_field,
                            const TypeSpec* struct_type,
                            const TypeSpec* fun_type) {
    new_types.emplace_back(dataset_type->Bind({struct_type}).value());
    new_types.emplace_back(std::make_unique<TypeTuple>(
        base_type->type_store(), base_type->type_member_store_ptr(),
        std::vector<const TypeSpec*>({new_types.back().get(), fun_type}),
        std::vector<std::string>({std::string(join_type), "key"})));
    names.emplace_back(join_field);
    types.emplace_back(new_types.back().get());
    return *this;
  }
  const TypeSpec* Build() {
    new_types.emplace_back(std::make_unique<TypeTuple>(
        base_type->type_store(), base_type->type_member_store_ptr(),
        std::move(types), std::move(names)));
    return new_types.back().get();
  }
  const TypeTuple* base_type;
  const TypeSpec* dataset_type;
  std::vector<std::string> names;
  std::vector<const TypeSpec*> types;
  std::vector<std::unique_ptr<TypeSpec>> new_types;
};

TEST_F(TypesTest, Joins) {
  // Preparations
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar"));
  ASSERT_OK_AND_ASSIGN(auto join_type, FindType("DatasetJoin"));
  ASSERT_OK_AND_ASSIGN(auto dataset_type, FindType("Dataset"));
  ASSERT_OK_AND_ASSIGN(auto ttype, FindType("Tuple"));
  auto tuple_type = static_cast<const TypeTuple*>(ttype);
  ASSERT_OK_AND_ASSIGN(auto struct_type, FindType("Struct<Int, String>"));
  ASSERT_OK_AND_ASSIGN(auto int_fun_type,
                       FindType("Function<Struct<Int, String>, Int>"));
  ASSERT_OK_AND_ASSIGN(auto string_fun_type,
                       FindType("Function<Struct<Int, String>, String>"));

  EXPECT_RAISES_WITH_MESSAGE_THAT(
      FindType("DatasetJoin<Int>").status(), InvalidArgument,
      testing::HasSubstr("Expecting exactly three"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      FindType("DatasetJoin<1, 2, 3>").status(), InvalidArgument,
      testing::HasSubstr("Extracting types from bindings"));

  JoinTupleBuilder builder(tuple_type, dataset_type);
  builder.AddJoin("right", "foo", FindType("Struct<UInt, Float64>").value(),
                  int_fun_type);
  builder.AddJoin("right_multi", "bar",
                  FindType("Struct<Float32, String>").value(), int_fun_type);
  ASSERT_OK_AND_ASSIGN(
      auto j1, join_type->Bind({struct_type, int_fun_type, builder.Build()}));
  EXPECT_EQ(j1->type_id(), pb::TypeId::DATASET_ID);
  EXPECT_TRUE(join_type->IsAncestorOf(*j1));
  EXPECT_TRUE(join_type->IsConvertibleFrom(*j1));
  ASSERT_TRUE(j1->ResultType() != nullptr);
  auto pb = j1->ResultType()->ToProto();
  pb.clear_name();
  EXPECT_THAT(pb, EqualsProto(R"pb(
                type_id: STRUCT_ID
                parameter { type_id: INT_ID name: "Int" }
                parameter { type_id: STRING_ID name: "String" }
                parameter {
                  type_id: NULLABLE_ID
                  name: "Nullable"
                  parameter { type_id: NULL_ID name: "Null" }
                  parameter {
                    type_id: STRUCT_ID
                    name: "Struct"
                    parameter { type_id: UINT_ID name: "UInt" }
                    parameter { type_id: FLOAT64_ID name: "Float64" }
                    parameter_name: "field_0"
                    parameter_name: "field_1"
                  }
                }
                parameter {
                  type_id: ARRAY_ID
                  name: "Array"
                  parameter {
                    type_id: STRUCT_ID
                    name: "Struct"
                    parameter { type_id: FLOAT32_ID name: "Float32" }
                    parameter { type_id: STRING_ID name: "String" }
                    parameter_name: "field_0"
                    parameter_name: "field_1"
                  }
                }
                parameter_name: "field_0"
                parameter_name: "field_1"
                parameter_name: "foo"
                parameter_name: "bar"
              )pb"));

  EXPECT_RAISES_WITH_MESSAGE_THAT(
      join_type->Bind({struct_type}), InvalidArgument,
      testing::HasSubstr("Expecting exactly three arguments"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      join_type->Bind({struct_type, 0}), InvalidArgument,
      testing::HasSubstr("Expecting exactly three"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      join_type->Bind({struct_type, int_fun_type, FindType("Int").value()}),
      InvalidArgument, testing::HasSubstr("Expecting the third type argument"));

  JoinTupleBuilder builder2(tuple_type, dataset_type);
  builder2.AddJoin("right", "foo", FindType("Struct<UInt, Float64>").value(),
                   string_fun_type);
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      join_type->Bind({struct_type, int_fun_type, builder2.Build()}).status(),
      InvalidArgument,
      testing::HasSubstr("Right side expression of a join differs"));

  EXPECT_RAISES_WITH_MESSAGE_THAT(
      join_type
          ->Bind({struct_type, int_fun_type, FindType("Tuple<Int>").value()})
          .status(),
      InvalidArgument, testing::HasSubstr("Invalid tuple type argument"));

  JoinTupleBuilder builder3(tuple_type, dataset_type);
  builder3.AddJoin("right", "foo", FindType("Int").value(), int_fun_type);
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      join_type->Bind({struct_type, int_fun_type, builder3.Build()}).status(),
      InvalidArgument,
      testing::HasSubstr(
          "Join dataset inner type not specified or not a structure"));

  EXPECT_RAISES_WITH_MESSAGE_THAT(
      join_type->Bind({FindType("Int").value(), int_fun_type, builder.Build()})
          .status(),
      InvalidArgument, testing::HasSubstr("Expecting a dataset type"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      join_type->Bind({struct_type, FindType("Int").value(), builder.Build()})
          .status(),
      InvalidArgument, testing::HasSubstr("Expecting a valid function type"));
}

}  // namespace analysis
}  // namespace nudl
