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

}  // namespace analysis
}  // namespace nudl
