#include "nudl/analysis/types.h"

#include <set>
#include <string>

#include "absl/base/macros.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nudl/analysis/type_store.h"
#include "nudl/grammar/dsl.h"
#include "nudl/status/testing.h"

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
    "Any",          "Null",      "Numeric", "Int",      "Int8",   "Int16",
    "Int32",        "UInt",      "UInt8",   "UInt16",   "UInt32", "String",
    "Bytes",        "Bool",      "Float32", "Float64",  "Date",   "DateTime",
    "TimeInterval", "Timestamp", "Decimal", "Iterable", "Array",  "Tuple",
    "Set",          "Map",       "Struct",  "Function"};

static const char* const kNumericTypeNames[] = {
    "Int",    "Int8",   "Int16",   "Int32",   "UInt",   "UInt8",
    "UInt16", "UInt32", "Float32", "Float64", "Decimal"};

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
  std::set<std::string> kUnBoundTypes({"Any", "Numeric", "Decimal", "Iterable",
                                       "Array", "Set", "Map", "Struct",
                                       "Function"});
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
    false, false, true, false, false, false, false, false, false, false, false,
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
    false, false, true, false, false, false, false, false, false, false, false,
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
    false, false, false, false, false, false,
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
    false, false, false, false, false, false,
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

}  // namespace analysis
}  // namespace nudl
