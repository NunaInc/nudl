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
#ifndef NUDL_ANALYSIS_TESTING_ANALYSIS_TEST_H__
#define NUDL_ANALYSIS_TESTING_ANALYSIS_TEST_H__

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "nudl/analysis/analysis.h"

namespace nudl {
namespace analysis {

//
// Base for writing tests for Nudl analysis components. Here is how to
// operate:
// 1- find a new test name, say "my_test"
// 2- create a directory "my_test" under nudl/analysis/testing/testdata.
// 3- create a file group in `nudl/analysis/testing/testdata/BUILD.bazel`
//  filegroup(
//      name="my_test",
//      srcs = glob(["my_test/*.pb"])
//  )
// 4- create a test file `my_test.cc` and a corresponding target in
//   `nudl/analysis/testing/BUILD.bazel`:
//
//  cc_test(
//      name = "my_test",
//      srcs = ["my_test.cc"],
//      deps = [
//          ":analysis_test",
//          "@com_google_googletest//:gtest_main",
//      ],
//      data = ["//nudl/analysis/testing/testdata:my_test"],
//  )
//
// 5- write a test with a Nudl code snippet, that calls `PrepareCode(..)`, in
//   `my_test.cc`
//
// #include "nudl/analysis/testing/analysis_test.h"
// namespace nuna { namespace nudl { namespace analysis {
// TEST_F(nuna::nudl::analysis::AnalysisTest, MyFirstTest) {
//   PrepareCode("my_test", "first_test", "x = 20" /* Nudl code */);
// }
// } } }  // namespaces
//
// 6- Run the test from command line, in your repo top directory:
//    ./bazel-bin/nudl/analysis/testing/lambda_test
//
// 7- The test would print whatever code the nudl code produces +
//   a pseudo code - eyeball those and make sure they correspond to
//   what you expect from the code.
// 8- If all seems ok, answer 'Y' to the prompt, and a file will be created.
// 9- Change the `PrepareCode` from above to `CheckCode`, rebuild and rerun.
//
// 10- Repeat the PrepareCode => CheckCode process from point 5- to 9- for
//   other test code snippets, but change the second string argument
//   (ie. "first_test") to other valid identifier names. Keep the "my_test"
//   as first argument.
//
class AnalysisTest : public ::testing::Test {
 public:
  // To be used as a main function in the tests:
  static int Main(int argc, char** argv);
  ~AnalysisTest() override;

 protected:
  void SetUp() override;
  Environment* env() const;
  absl::StatusOr<Module*> ImportCode(absl::string_view module_name,
                                     absl::string_view module_content) const;

  // Normal mode of operation: used to check an existing
  // prepared proto file test agains code.
  void CheckCode(absl::string_view test_name, absl::string_view module_name,
                 absl::string_view code);
  // Development mode: used to visually check the conversion
  // of a piece of code and preparing a proto file for it.
  void PrepareCode(absl::string_view test_name, absl::string_view module_name,
                   absl::string_view code, bool skip_write = false) const;

  // Checks that the provided code raises an error.
  void CheckError(absl::string_view module_name, absl::string_view code,
                  absl::string_view expected_error) const;

  // Writes the prepare proto code to the corresponding test file.
  void WritePreparedCode(Module* module, const pb::ModuleSpec& proto,
                         absl::string_view test_name,
                         absl::string_view module_name, absl::string_view code,
                         bool skip_write) const;

  std::unique_ptr<Environment> env_;
  std::string builtin_file_{"nudl/analysis/testing/testdata/nudl_builtins.ndl"};
  std::string search_path_{"nudl/analysis/testing/testdata"};
  size_t next_id_ = 0;
  absl::Duration setup_duration_ = absl::ZeroDuration();
  absl::Duration parse_duration_ = absl::ZeroDuration();
  absl::Duration analysis_duration_ = absl::ZeroDuration();
  absl::Duration convert_duration_ = absl::ZeroDuration();
  absl::Duration read_file_duration_ = absl::ZeroDuration();
  absl::Duration compare_duration_ = absl::ZeroDuration();
  absl::Duration regenerate_duration_ = absl::ZeroDuration();
  absl::Duration total_duration_ = absl::ZeroDuration();
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_TESTING_ANALYSIS_TEST_H__
