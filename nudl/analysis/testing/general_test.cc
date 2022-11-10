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

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "nudl/analysis/testing/analysis_test.h"
#include "nudl/status/testing.h"

ABSL_DECLARE_FLAG(bool, nudl_accept_abstract_function_objects);

namespace nudl {
namespace analysis {

TEST_F(AnalysisTest, SimpleVars) {
  CheckCode("general_test", "lambda_vars", R"(
import cdm
param x: Int = 20

// Untyped function - bound upon calling a'la C++ templates.
def foo(p, q) => p + q

// Top level constant
y: Int = foo(x, 30)
)");
}

TEST_F(AnalysisTest, SimpleFunctions) {
  CheckCode("general_test", "extract_full_name", R"(
import cdm
def ExtractFullName(name: cdm.HumanName) : String => {
  concat([concat(name.prefix, " "),
          ensure(name.family),
          ensure(name.given),
          concat(name.suffix, " ")], " ")
}
)");
  CheckCode("general_test", "filter_name", R"(
def IsDillinger(name: cdm.HumanName) =>
  ensure(name.family) == "Dillinger" and ensure(name.given) == "John"
)");
  CheckCode("general_test", "max_termination", R"(
def MaxTermination(name: cdm.HumanName) =>
  max([len(name.prefix), len(name.suffix)])
)");
  CheckCode("general_test", "maxlen_method", R"(
// Returns the max len of a list of lists or so.
// Also makes it a member of Iterable, so we can call it fluently:
def method maxlen(l: Iterable<Container<{X}>>) => l.map(len).max()

// Same as above but uses maxlen (in a fluent way):
def MaxTermination(name: cdm.HumanName) => {
  [name.prefix, name.suffix].maxlen()
}
)");
  CheckCode("general_test", "maxlen_untype", R"(
// Untyped maxlen from above:
def maxlen(l) => l.map(len).max()

// Using maxlen2 in a fluent way:
def MaxTermination(name: cdm.HumanName) => {
  maxlen([name.prefix, name.suffix])
}
)");
  CheckCode("general_test", "function_member", R"(
def f(name: cdm.HumanName) => name
def g(name: cdm.HumanName) => f(name).family
)");
  CheckCode("general_test", "native_function", R"(
def f(x: Int) : Int =>
[[pyinline]]
${x}
[[end]]
)");
  CheckCode("general_test", "if_binding", R"(
def f(x: Int) : Nullable<Int> => {
  x > 10 ? (null, x - 10)
}
y = f(20)
def g() => y
)");
}

TEST_F(AnalysisTest, BasicLiterals) {
  CheckCode("general_test", "literal_int8", "x: Int8 = 10");
  CheckCode("general_test", "literal_int16", "x: Int16 = 0x10");
  CheckCode("general_test", "literal_int32", "x: Int32 = 234734");
  CheckCode("general_test", "literal_int64", "x: Int = 2347340934204");
  CheckCode("general_test", "literal_uint8", "x: UInt8 = 123u");
  CheckCode("general_test", "literal_uint16", "x: UInt16 = 0xffffu");
  CheckCode("general_test", "literal_uint32", "x: UInt32 = 23247493u");
  CheckCode("general_test", "literal_uint64", "x: UInt = 239038247498u");
  CheckCode("general_test", "literal_float32", "x: Float32 = 2.33f");
  CheckCode("general_test", "literal_float64", "x: Float64 = 2.33");
  CheckCode("general_test", "literal_string", "x: String = \"abc\"");
  CheckCode("general_test", "literal_bytes", "x: Bytes = b\"abc\"");
  CheckCode("general_test", "literal_bool", "x: Bool = true");
  CheckCode("general_test", "literal_tinterval", "x: TimeInterval = 2hours");
  CheckCode("general_test", "literal_nullable",
            "x: Nullable<Int> = null; y: Nullable<Int> = 123");
  CheckError("coerce_int_int", "x: Int = 123u", "Cannot coerce");
  CheckError("coerce_int_uint", "x: UInt = 123", "Cannot coerce");
  CheckError("coerce_int_uint2", "x: UInt = 12.3", "Cannot coerce");
  CheckError("coerce_int_string", "x: String = 12.3", "Cannot coerce");
  CheckError("coerce_int_int2", "x: Int = \"\"", "Cannot coerce");
  CheckError("coerce_int_string2", "x: String = b\"foo\"", "Cannot coerce");
  CheckError("coerce_int_string3", "x: String = true", "Cannot coerce");
  CheckError("coerce_int_int3", "x: Int = true", "Cannot coerce");
}

TEST_F(AnalysisTest, IndexExpression) {
  CheckCode("general_test", "simple_index", R"(
def Access(l: Array<String>, n: Int) => l[n]
)");
  CheckCode("general_test", "literal_index", R"(
def Access(n: Int) => [1,2,3][n]
)");
  CheckCode("general_test", "literal_index_uint", R"(
def Access(n: UInt) => [1,2,3][n]
)");
  CheckCode("general_test", "set_index", R"(
def Access(s: Set<String>, n: String) => {
  s[n]
}
)");
  CheckCode("general_test", "literal_set_index", R"(
def Access(n: String) => {
  s: Set<String> = ["a", "b", "c"];
  s[n]
}
)");
  CheckCode("general_test", "map_index", R"(
def Access(s: Map<String, Int>, n: String) => {
  s[n]
}
)");
  CheckCode("general_test", "map_literal_index", R"(
def Access(n: String) => {
  s = ["a": 1, "b": 2, "c": 3]; s[n]
}
)");
  CheckCode("general_test", "tuple_index", R"(
def Access(s: Tuple<String, Int>) => s[1]
)");
  CheckCode("general_test", "tuple_index_uint", R"(
def Access(s: Tuple<String, Int>) => s[1u]
)");
  CheckCode("general_test", "empty_list", "x: Array<Int> = []");
  CheckCode("general_test", "empty_set", "x: Set<Int> = []");
  CheckCode("general_test", "empty_map", "x: Map<Int, String> = []");
  CheckError("bad_empty", "a = []", "Empty iterable");
  CheckError("bad_elements1", "a = [1, \"a\"]", "Cannot coerce");
  CheckError("bad_elements2", "m = [1: \"a\", 2: 3]", "Cannot coerce");
  CheckError("bad_elements3", "m = [1: \"a\", \"x\": \"y\"]", "Cannot coerce");
  CheckError("bad_index1", "m = [1, 2, 3][\"m\"]", "Cannot coerce");
  CheckError("bad_index2", "m = [1: 2, 3: 4][\"m\"]", "Cannot coerce");
  CheckError("bad_index3", "def f(t: Tuple<Int, String>, n: Int) => t[n]",
             "Tuples require a static integer index");
  CheckError("bad_index4", "def f(t: Tuple<Int, String>, n: Int) => t[2]",
             "out of tuple type range");
  CheckError("bad_coerce1", "m : Int = []", "cannot be coerced into");
  CheckError("bad_coerce2", "m : Array<String> = [1,2,3]",
             "originally declared as: Array<String>");
  CheckError("bad_coerce3", "m : Array<Int> = [\"foo\"]",
             "originally declared as: Array<Int>");
}

TEST_F(AnalysisTest, Imports) {
  CheckCode("general_test", "import_alias", R"(
import foo = cdm;
schema Foo = {
  bar: String;
  baz: Nullable<Int>;
  qux: foo.HumanName;
}
def f(name: foo.HumanName) => len(name.prefix)
def g(x: Foo) => len(x.qux.prefix)
def h(x: Foo) => len(x.qux.prefix)
)");
  CheckCode("general_test", "submodule_init", R"(
import submodule
z = submodule.area(10)
)");
  CheckCode("general_test", "submodule_module", R"(
import submodule.compute
z = submodule.compute.square_circle_area(10)
)");
  CheckCode("general_test", "submodule_module2", R"(
import submodule
z = submodule.area(10)
import submodule.compute
zz = submodule.compute.square_circle_area(10)
)");
  ASSERT_TRUE(env()->module_store()->HasModule("submodule_init"));
  ASSERT_TRUE(env()->module_store()->HasModule("submodule"));
  ASSERT_TRUE(env()->module_store()->HasModule("submodule.compute"));
  ASSERT_TRUE(env()->module_store()->modules().contains("submodule.compute"));
  auto module = env()->module_store()->GetModule("submodule");
  ASSERT_TRUE(module.has_value());
  EXPECT_FALSE(module.value()->file_path().native().empty());
  EXPECT_EQ(module.value()->type_spec()->type_id(), pb::TypeId::MODULE_ID);
  CheckError("module_bad_assign", R"(
import submodule
submodule.pi = 1.2
)",
             "Only parameters can be set for external scopes");
  EXPECT_RAISES(ImportCode("module_not_found1", R"(
import foobarsky
z = foobarsky.call(10)
)")
                    .status(),
                NotFound);
  EXPECT_RAISES(ImportCode("module_not_found2", R"(
import submodule.foobarsky
z = submodule.foobarsky.call(10)
)")
                    .status(),
                NotFound);
  CheckError("chain_import", R"(
import chain_import
x = 10;
)",
             "Chain detected in import order");
  CheckError("parse_error", "10x; x = 2$", "Parse errors in code");
}

TEST_F(AnalysisTest, Ifs) {
  CheckCode("general_test", "if_simple", R"(
def f(x: Int) => {
  if (x == 0) {
    return 0
  }
  return 1
}
)");
  CheckCode("general_test", "if_yield", R"(
def f(x: Int) => {
  if (x % 2 == 0) {
    yield x
  }
  pass
}
)");
  CheckCode("general_test", "if_elseif", R"(
def f(x: Int) => {
  if (x % 2 == 0) {
    yield x / 2
  } else if (x % 3 == 0) {
    yield x / 3
  }
  pass
}
)");
  CheckCode("general_test", "if_elseif_else", R"(
def f(x: Int) => {
  if (x % 2 == 0) {
    return x / 2
  } else if (x % 3 == 0) {
    return x / 3
  } else {
    return x
  }
}
)");
  CheckError("non_bool_if", R"(
def f(x: Int) => {
  if (x % 2) {
     return x + 1;
  } else {
     return x
  }
})",
             "does not return a boolean value");
}

TEST_F(AnalysisTest, Pragmas) {
  CheckCode("general_test", "pragma_bindings", R"(
def f(x, y) => x + y
def compute(x : Int) => {
  pragma log_bindings_on
  z = f(x, x / 2)
  pragma log_bindings_off
  z + 2
})");
  CheckCode("general_test", "pragma_log_module_names", R"(
my_const = 33
def f(x, y) => x + y
pragma log_module_names
)");
  CheckCode("general_test", "pragma_log_scope_names", R"(
my_const = 33;
def f(x: Int, y: Int) => {
  z = x + y
  pragma log_scope_names
  z
})");
  CheckCode("general_test", "pragma_log_expression", R"(
my_const: Int32 = 33;
pragma log_expression { my_const }
)");
  CheckCode("general_test", "pragma_log_proto", R"(
my_const: Int32 = 33;
pragma log_proto { my_const }
)");
  CheckCode("general_test", "pragma_log_type", R"(
my_const: Int32 = 33;
pragma log_type { my_const }
)");
  CheckError("pragma_no_expression", "x = 10 pragma log_expression",
             "requires an expression");
  CheckError("pragma_extra_expression", "x = 10 pragma log_bindings_on {x}",
             "does not require an expression");
  CheckError("pragma_unknown_expression", "x = 10 pragma foobarsky",
             "Unknown pragma");
  CheckError("pragma_no_return", "def f() => pragma log_scope_names",
             "does not have any proper expressions defined");
}

TEST_F(AnalysisTest, DotAccess) {
  CheckCode("general_test", "dot_access_member", R"(
import cdm
def f(names: Array<cdm.HumanName>) => _ensured(names.front())
def g(names: Array<cdm.HumanName>) => f(names).prefix.len()
)");
  CheckCode("general_test", "dot_access_function", R"(
def f() => [1,2,3]
def g() => f().len()
def h() => f()[2]
)");
}

TEST_F(AnalysisTest, FunctionErrors) {
  CheckError("unbuilt_function", R"(
def f(names: Array<{X}>) => _ensured(names.front())
def g() => f().prefix.len()
)",
             "No value provided for function parameter");
  CheckError("unbuilt_function2", R"(
def g() => { z = (x => x + 1); z(3) }
)",
             "Provided function type needs to be bound.");
  CheckError("missing_arg", R"(
import cdm
def f(names: Array<cdm.HumanName>) => _ensured(names.front())
def g() => f().prefix.len()
)",
             "No value provided");
  CheckError("multibind", R"(
def f(x: Int) => { x + 1 }
def f(x: Int, y: Int = 0) => { x - y }
z = f(10)
)",
             "Found too many functions matching the "
             "provided call signature");
  CheckError("name_conflict1", R"(
def foo(x: Int) => { x - 1 }
foo : Int = 10
)",
             "Cannot assign an object of this kind");
  CheckError("name_conflict2", R"(
foo : Int = 10
def foo(x: Int) => { x - 1 }
)",
             "An object named: foo already defined");
  CheckError("name_redefined", R"(
def foo(x: Int, x: Int) => { x - 1 }
)",
             "already defined");
  CheckError("default_val_incompatible1", R"(
def foo(x: Int = "foo") => { x - 1 }
)",
             "Cannot coerce a literal of type");
  CheckError("default_val_incompatible2", R"(
def foo() : String => "foo"
def foo(x: Int = foo()) => { x - 1 }
)",
             "String is incompatible with declared type");
  CheckError("non_default_post_default", R"(
def foo(x: Int = 1, y: Int) => { x + y }
)",
             "No default value for parameter: y");
  CheckError("result_incompatible", R"(
z = "foo"
def foo(x: Int) : Int => { z }
)",
             "Cannot return: String");
  CheckCode("general_test", "multi_type_bind", R"(
def f(x: Int) => { x - 1 }
def f(x: UInt) => { x + 1u }
z = f(10)
w = f(10u)
)");
  CheckCode("general_test", "ancestor_type_bind", R"(
def f(x: Int) => { x - 1 }
def f(x: Int32) => { x + 1 }
z: Int32 = 10
w1 = f(z)
w2: Int = w1
w3 = f(w2)
)");
  CheckCode("general_test", "assign_basic", R"(
def f(x: Int) => {
  x = x + 1
  x + 3
})");
  CheckError("bad_assignment", R"(
import cdm
def foo(x: cdm.HumanName, y: cdm.HumanName) => {
  x = y;
  x.family
}
)",
             "Cannot reassign function argument: x");
  CheckError("bad_method_redefined", R"(
def method foo(x: Int) => 0
def method foo(x: Int) => 1
)",
             "the same name and signature already exists");
  CheckError("re_assign_type", R"(
def f() => {
  x: Union<Int, String> = 10;
  y: Int8 = 20;
  x = y
  x = "Foo"
}
)",
             "Cannot coerce");
  CheckCode("general_test", "member_call", R"(
def method inc(x: Int) => x + 1
def f(x: Int) => {
  x.inc()
}
)");
  CheckCode("general_test", "dot_call", R"(
def f() => (x: Int) : Int => x + 1
def g(n: Int) => f()(n)
)");
  CheckError("bad_return_call", R"(
def f() => 10
def g(n: Int) => f()(n)
)",
             "Cannot call non-function type");
  CheckError("no_function", R"(
import cdm
def f(name: cdm.HumanName) => {
  name.family()
})",
             "Cannot call non-function type");
  CheckCode("general_test", "deep_call", R"(
schema Bar = {
  subname: String
}
schema Foo = {
  name: Bar
}
def f(x: Foo) => x
def g(x: Foo) => f(x).name.subname.len()
)");
  CheckCode("general_test", "deep_call_object", R"(
schema Bar = {
  subname: String
}
schema Foo = {
  name: Bar
}
def g(x: Foo) => x.name.subname.len()
)");
  CheckCode("general_test", "late_default_bind", R"(
def f(x: {X}, y: X = 20) => x + y
def g(a: Int) => f(a)
)");
  CheckCode("general_test", "late_default_bind2", R"(
def f(x: {X} = 20) => x + 10
def g(a: Int) => f() + a
)");
  CheckCode("general_test", "late_default_bind3", R"(
def f(x: {X} = 10, y: X = 20) => x + y
def g(a: Int) => f(y=a)
)");
  CheckError("late_default_bind_error", R"(
def f(x: {X} = 10, y: X = "Foo") => x + y
def g(a: Int) => f(a)
)",
             "two incompatible (sub)argument types: Int and String");
  CheckCode("general_test", "default_bind_on_funtype", R"(
def f() => (x: Int = 10, y: Int = 20) => x + y
def g(a: Int) => f()(a)
)");
  CheckError("return_pass", R"(
def f(x: Int) => pass;
)",
             "needs to yield some values");
  CheckError("return_unbound", R"(
def f() : Numeric => [[pyinline]]0[[end]]
def g() => f()
y = g()
)",
             "is unbound and not a function");
  CheckCode("general_test", "not_too_many_binds", R"(
def f(x: Numeric, y: Int) => x + y
def f(x: Int, y: Numeric) => x + y
z = f(1, 2)
)");  // the second bind is identical - so we are fine. I guess..
  CheckCode("general_test", "not_too_many_binds2", R"(
def f(x: Numeric, y: Int) => x + y
z = f(1, 2)
)");
  CheckError("improper_function_type", R"(
def f() => {
  z: Nullable<Function> = null;
  _ensured(z)(3)
}
)",
             "binding for improper function type");
}

TEST_F(AnalysisTest, ReturnValues) {
  CheckCode("general_test", "compatible_nullable_results1", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    return x
  }
  return null
}
)");
  CheckCode("general_test", "compatible_nullable_results2", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    return null
  }
  return x
}
)");
  CheckCode("general_test", "compatible_nullable_results3", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    yield null
  }
  yield x
}
)");
  CheckCode("general_test", "compatible_nullable_results4", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    yield x
  }
  yield null
}
)");
  CheckCode("general_test", "compatible_yeld_results", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    pass
  }
  yield x / 2
}
)");
  CheckCode("general_test", "compatible_nullable_results5", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    yield null
  }
}
)");
  CheckError("incompatible_returns1", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    yield x
  }
  return x / 2
}
)",
             "Cannot `return` in a function that uses `yield`");
  CheckError("incompatible_returns2", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    pass
  }
  return x / 2
}
)",
             "Can only `yield` in a function");
  CheckError("incompatible_returns3", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    return x / 2
  }
  yield x
}
)",
             "Cannot `yield` or `pass` in functions that use `return`");
  CheckError("incompatible_returns4", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    return x / 2
  }
  pass
}
)",
             "Cannot `yield` or `pass` in functions that use `return`");
  CheckError("incompatible_returns5", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    return x / 2
  }
  return "Foo"
}
)",
             "String is incompatible with previous");
  CheckError("no_body", R"(
def f(x: Int) => pragma log_expression { x }
)",
             "does not have any proper expressions defined");
  CheckError("missing_return1", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    return x / 2
  }
  // Need an error here
})",
             "Please explicitly return or yield");
  CheckError("missing_return2", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    x = x + 1
  } else {
    return x
  }
  // Need an error here
})",
             "Please explicitly return or yield");
  CheckError("after_return1", R"(
def foo(x: Int) => {
  return x / 2
  // Need an error here:
  x = x + 1
})",
             "Meaningless expression after function return");
  CheckError("after_return2", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    x = x + 1
  } else {
    return x
    x = x + 2
  }
})",
             "Meaningless expression after function return");
}
TEST_F(AnalysisTest, BadScopeAccess) {
  CheckError("bad_if_access1", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    y = x + 10
  }
  y + x
})",
             "Cannot find name: `y`");

  CheckError("bad_if_access2", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    x = x + 10
  } else {
    z = x + 1
  }
  z
})",
             "Cannot find name: `z`");
  CheckError("bad_if_access3", R"(
def foo(x: Int) => {
  if (x % 2 == 0) {
    y = x + 10
  } else {
    z = x + y
  }
  x
})",
             "Cannot find name: `y`");
}

TEST_F(AnalysisTest, Tuples) {
  CheckCode("general_test", "tuple_build", R"(
def f(n: Int) => n + 1
x: Tuple = [1, "foo", f]
)");
  CheckCode("general_test", "tuple_bind", R"(
x: Tuple = [1, "foo", 3.4]
def g(t: Tuple) => t[1]
z = g(x)
)");
  CheckCode("general_test", "tuple_construct", R"(
v = Tuple([1, "foo", 2.3])
)");
  CheckCode("general_test", "tuple_direct_call", R"(
def g(t: Tuple) => t[2]
z = g([1, "foo", 2.3])
)");
  CheckError("tuple_badtype", R"(
v = Tuple([1, "foo", 2.3])
v = Tuple([1, 3, 2.3])
)",
             "Type mismatch");
  CheckError("tuple_index", R"(
def h(t: Tuple) => t[4]
zz = h([1, "foo", "bar"])
)",
             "Tuples index: 4 out of tuple type range");
  CheckCode("general_test", "tuple_direct_call", R"(
def g(t: Tuple) => t[2]
z = g([1, "foo", 2.3])
)");
}

TEST_F(AnalysisTest, AbstractFunctions) {
  // We set this to accept binds:
  absl::SetFlag(&FLAGS_nudl_accept_abstract_function_objects, true);
  CheckCode("general_test", "with_default_arg", R"(
def f(x: Array<Int>, y: Int = 3) =>
  x.map((p, q = y) => p + q).sum()

z = f([1,2,3,4])
)");
  CheckCode("general_test", "var_function", R"(
def ff(x, y) => x + y
gg = ff
v1 = gg(1, 2)
v2 = gg(1.1, 2.2)
)");
  /* TODO(catalin):
     This fails in conversion - we never get to bind the lambda
     of the second assignment.
  PrepareCode("general_test", "var_function2", R"(
def ff(x, y) => x + y
gg = ff
v1 = gg(1, 2)
gg = (x, y) => x * y
v2 = gg(1.1, 2.2)
)");
  */
  CheckCode("general_test", "var_function3", R"(
def f(x: {T}, y: T, fun: Function<T, T>) => {
  if (x > y) {
    fun = (x => x * x)
  }
  fun(x)
}
def g(x) => x + x
def h(x) => x - x

ff = g
v1 = f(100, 20, ff)
ff = h
v2 = f(20.0, 100.0, ff)
)");
  CheckCode("general_test", "var_function4", R"(
g = {a = 1, b = 2.3, c = (s, t) => s + t}
h = g
v1 = g[2](333, 22)
v2 = g[2](333.3, 3.5)
)");
  CheckCode("general_test", "named_tuples", R"(
def f(x) => x + 1
x = { a = 3, b = "foo", c = f }
)");
  CheckCode("general_test", "named_tuples2", R"(
def f(x) => x + 1
x = { a = 3, b = "foo", c = f }
g = x[2](4)
)");
  CheckError("bad_reassignment1", R"(
def ff(x, y) => x + y
gg = ff
v1 = gg(1, 2)
def fff(x, y, z) => x + y + z
gg = fff
)",
             "Type mismatch in assignment");
  CheckError("bad_reassignment2", R"(
def ff(x, y) => x + y
gg = ff
v1 = gg(1, 2)
def fff(x, y, z = 1) => x + y + z
gg = fff
)",
             "Type mismatch in assignment");
  CheckError("bad_reassignment3", R"(
def ff(x, y) => x + y
def ff(x, y, z) => x + y + z
gg = ff
v1 = gg(1, 2)
)",
             "Cannot call non-function type");
  // There is a discussion here:
  //   - this raises an error and after some thought it
  //   definitely should.
  //   - the problem is that as defined, ff provided no
  //   relation between the input types and output types,
  //   so, when we call `main(ff)`, and we bind gg, there
  //   is no way to determine the type of gg from the actual
  //   provided bind (ie. Function<Any, Any, Any>)
  //   - We can do something and because we call with ff,
  //   we know that for that particular call we would get
  //   an int, but for the general purpose, not.
  CheckError("untyped_function", R"(
def ff(x, y) => x * y
def gg(f) => {
  f(2, 3)
}
v1 = gg(ff)
)",
             "is unbound and not a function");

  // TODO(catalin): while this runs correctly, it does so because
  // of the dynamic nature of python. There are some issues here..
  CheckCode("general_test", "reassigned_multiple_binds", R"(
def ff(x: {T}, y: {T}) : T => x * y
def gg(x, y): Int => x + y
def main(f) => {
  f(2, 3)
}
def main2(f) => {
   f(2.0, 3.0)
}
v1 = main(ff)
v2 = main(gg)
v3 = main2(ff)
)");
  // This exercises a previous crash during error recovery:
  CheckError("bad_array_coercion", R"(
def agg(t: {T}): Tuple<T, Tuple<T>> => {
  result = [t, {_arg = t}]
  pragma log_type {result}
  return result
}
x = agg(0)
)",
             "Cannot coerce Tuple type to: Int");
}

TEST_F(AnalysisTest, JustPrepare) {
  PrepareCode("general_test", "prepare_test", "x = null", true);
}

}  // namespace analysis
}  // namespace nudl

int main(int argc, char** argv) {
  return nudl::analysis::AnalysisTest::Main(argc, argv);
}
