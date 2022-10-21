#include "nudl/analysis/testing/analysis_test.h"

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
def method maxlen(l: Iterable<Iterable<{X}>>) => l.map(len).max()

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
}

TEST_F(AnalysisTest, Alias) {
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
}

TEST_F(AnalysisTest, JustPrepare) {
  PrepareCode("general_test", "prepare_test", "x = null", true);
}

}  // namespace analysis
}  // namespace nudl
