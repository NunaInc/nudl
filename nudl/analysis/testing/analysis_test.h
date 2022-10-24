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
 protected:
  void SetUp() override;
  Environment* env() const;
  absl::StatusOr<Module*> ImportCode(absl::string_view module_name,
                                     absl::string_view module_content) const;

  // Normal mode of operation: used to check an existing
  // prepared proto file test agains code.
  void CheckCode(absl::string_view test_name, absl::string_view module_name,
                 absl::string_view code) const;
  // Development mode: used to visually check the conversion
  // of a piece of code and preparing a proto file for it.
  void PrepareCode(absl::string_view test_name, absl::string_view module_name,
                   absl::string_view code, bool skip_write = false) const;

  // Checks that the provided code raises an error.
  void CheckError(absl::string_view module_name, absl::string_view code,
                  absl::string_view expected_error) const;

  std::unique_ptr<Environment> env_;
  std::string builtin_file_{"nudl/analysis/testing/testdata/builtin.ndl"};
  std::string search_path_{"nudl/analysis/testing/testdata"};
  size_t next_id_ = 0;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_TESTING_ANALYSIS_TEST_H__
