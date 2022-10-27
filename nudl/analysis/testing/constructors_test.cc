// This tests type definitions and constructors

#include "nudl/analysis/testing/analysis_test.h"
#include "nudl/status/testing.h"

namespace nudl {
namespace analysis {

TEST_F(AnalysisTest, SimpleTypdefs) {
  CheckCode("constructors_test", "typedef_simple", R"(
typedef I = Int
typedef AI = Array<Int>
typedef F = Function<AI, I>
)");
  CheckCode("constructors_test", "typedef_templated", R"(
typedef N = Numeric
typedef I = Int
typedef A = Array<N>
typedef B = A<I>
)");
  CheckCode("constructors_test", "typedef_imported", R"(
import typedef_templated
typedef C = typedef_templated.B;
c : typedef_templated.B = [1, 2, 3];
)");
  CheckCode("constructors_test", "typedef_submodule", R"(
import submodule.compute
f: submodule.compute.RadiusArray = [1.0, 2.0, 3.0]
sum_area = submodule.compute.sum_area(f)
)");

  CheckError("typedef_template_error", R"(
typedef N = Numeric
typedef A = Array<N>
typedef B = A<String>
)",
             "Expecting an argument of type: { N : Numeric }");
  CheckError("typedef_type_error", R"(
typedef F = Foo
)",
             "Cannot find type `Foo`");
}

TEST_F(AnalysisTest, SimpleConstructors) {
  CheckCode("constructors_test", "simple_constructs", R"(
x = Int(3.2)
y = Bool(x)
z = Int()
n = Int("2", Int())
m = Nullable<Int>("2")
p = int("2")
)");
  CheckCode("constructors_test", "structure_constructs", R"(
now = Timestamp()
date1 = _ensured(Nullable<Date>(2022, 10, 3))
date2 = Date(Timestamp(3300303))
x = date1.year() - date2.year()
date3 = date(2022, 10, 3)
)");
  CheckCode("constructors_test", "composed_constructs", R"(
a = Array(Timestamp())
b = Array(Date())
c = Array(Array(Date()))
)");
  CheckCode("constructors_test", "define_constructor", R"(
def constructor date_to_string(date: Date) : String =>
  [date.year(), date.month(), date.day()].map(to_string).concat("/")
z = String(Date())
)");
  CheckError("bad_comparison", R"(
date1 = Date(Timestamp())
x = date1 < 20
)",
             "T is bound to two incompatible argument types");
  CheckError("bad_constructor", R"(
date1 = Date(Timestamp(), 12, 10)
)",
             "Cannot find any function signature matching arguments");
  CheckError("bad_constructor2", R"(
ts = Timestamp("foo")
)",
             "Cannot find any function signature matching arguments");
  CheckError("bad_constructor3", R"(
def constructor some() : Union<Int, String> => "x"
)",
             "Cannot define constructors for Union types");
  CheckError("bad_constructor4", R"(
def constructor other_default_int() : Int => 1
)",
             "Adding defined function other_default_int as a constructor");
  CheckError("bad_constructor5", R"(
def constructor some_bad_constructor(x: Int) => x + 1
)",
             "needs to be declared with a result type");
  CheckError("bad_constructor6", R"(
def constructor some_bad_constructor(x: Int, y: Int): Int => yield x + y
)",
             "Cannot `yield` or `pass` in constructor");
}

}  // namespace analysis
}  // namespace nudl

int main(int argc, char** argv) {
  return nudl::analysis::AnalysisTest::Main(argc, argv);
}
