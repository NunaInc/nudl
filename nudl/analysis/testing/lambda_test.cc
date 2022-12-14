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

ABSL_DECLARE_FLAG(bool, analyze_log_bindings);

namespace nudl {
namespace analysis {

TEST_F(AnalysisTest, SimpleLambdas) {
  // absl::SetFlag(&FLAGS_analyze_log_bindings, true);
  // Fully bound types:
  CheckCode("lambda_test", "lambda_simple", R"(
// Types fully specified:
def ProcessNames(names: Array<String>) : UInt =>
  sum(map(names, (s : String) : UInt => len(s)))
)");
  // Types inferred from lambda up.
  CheckCode("lambda_test", "lambda_infer", R"(
// Lambda bound on call:
def ProcessNames(names: Array<String>) : UInt =>
  sum(map(names, s => len(s)))
)");
  // Types inferred two functions up.
  CheckCode("lambda_test", "lambda_double_infer", R"(
import cdm

// `names` and lambda bound on call:
def ProcessNames(names) =>
  sum(map(names, s => len(s)))

// Binds the ProcessNames3 fully upon call:
def UseProcessNames(name: cdm.HumanName) =>
  ProcessNames(name.prefix)
)");
  CheckCode("lambda_test", "lambda_builtin", R"(
// Using a standard function:
def ProcessNames(names: Array<String>) : UInt =>
  sum(map(names, len))
)");
  CheckCode("lambda_test", "lambda_external", R"(
// Using external function:
def ProcessNames(names: Array<cdm.HumanName>) =>
  sum(map(names, s => len(cdm.GetFamilyName(s))))
)");
  CheckCode("lambda_test", "lambda_fluent", R"(
// Showing fluent calls:
def ProcessNames(names: Array<String>) =>
  names.map(s => len(s)).sum()
)");
  CheckCode("lambda_test", "lambda_default_value", R"(
def FilterName(names: Array<String>, extra: String) => {
  filtered = names.filter((name, arg=extra) => { len(name) > len(arg) });
  return not filtered.empty();
})");
  CheckCode("lambda_test", "lambda_default_value_with_return", R"(
def ProcessNames(names: Array<String>, min_len: UInt) => {
  names.map((s, m=min_len) => len(s) - m).sum()
})");
}

TEST_F(AnalysisTest, LambdaReturns) {
  absl::SetFlag(&FLAGS_analyze_log_bindings, true);
  // Function returning a lambda:
  CheckCode("lambda_test", "return_lambda", R"(
def get_fun() => s : Int => s + 10
)");
  CheckError("abstract_return", R"(
def get_fun() => { s => s + 10 }
)",
             "Please add non-abstract type specifications");
}

TEST_F(AnalysisTest, LambdaVars) {
  absl::SetFlag(&FLAGS_analyze_log_bindings, true);
  CheckCode("lambda_test", "call_lambda", R"(
def add(x: Int) => {
  adder = (s: Int) : Int => s + 10;
  adder(x)
})");
  // This was a nice try, but I reverted - could not close
  // all the corner cases in a timely fashion, so giving up
  // for now.
  /*
  CheckCode("lambda_test", "abstract_lambda_var", R"(
def add(x: Int) => {
  adder = (s => s + 10);
  adder(x)
})");
  */
  // This is still an error:
  CheckError("abstract_lambda_var", R"(
def add(x: Int) => {
  adder = (s => s + 10);
  adder(x)
})",
             "Please add non-abstract type specifications");
}

TEST_F(AnalysisTest, FunctionParameters) {
  absl::SetFlag(&FLAGS_analyze_log_bindings, false);
  CheckCode("lambda_test", "arg_passed", R"(
def f(x: Function<{X}, X>, val: {X}) => x(val)
def g(x: Int, y: Int = 1) => x + y
def h(x: Int) => f(g, x)
)");
  CheckCode("lambda_test", "multiple_choices", R"(
def f(x: Function<{X}, X>, val: {X}) => x(val)
def g(x: Int, y: Int = 1) => x + y
def g(x: Float64, y: Float64 = 1.2) => x + y
def h(x: Int) => f(g.g__i0, x)
)");
  CheckCode("lambda_test", "full_any", R"(
def f(x: {X}, val: Int) => x(val)
def g(x: Int, y: Int = 1) => x + y
def h(x: Int) => f(g, x)
)");
  CheckError("unknown_choice", R"(
def f(x: Function<{X}, X>, val: {X}) => x(val)
def g(x: Int, y: Int = 1) => x + y
def g(x: Float64, y: Float64 = 1.2) => x + y
def h(x: Int) => f(g, x)
)",
             "Cannot process type for rebinding");
  CheckError("signature_redefined", R"(
def f(x: Function<{X}, X>, val: {X}) => x(val)
def g(x: Int, y: Int = 1) => x + y
def g(x: Int, y: Int = 1) => 2 * x + y
def h(x: Int) => f(g, x)
)",
             "function with the same name and signature");
}

}  // namespace analysis
}  // namespace nudl

int main(int argc, char** argv) {
  return nudl::analysis::AnalysisTest::Main(argc, argv);
}
