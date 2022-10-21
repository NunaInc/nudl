#include "nudl/analysis/testing/analysis_test.h"

#include <fstream>
#include <iostream>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "nudl/analysis/basic_converter.h"
#include "nudl/status/status.h"
#include "nudl/status/testing.h"
#include "nudl/testing/protobuf_matchers.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);

namespace nudl {
namespace analysis {

void AnalysisTest::SetUp() {
  absl::SetFlag(&FLAGS_nudl_short_analysis_proto, true);
  ASSERT_OK_AND_ASSIGN(env_, Environment::Build(builtin_file_, {search_path_}));
}

Environment* AnalysisTest::env() const { return env_.get(); }

absl::StatusOr<Module*> AnalysisTest::ImportCode(
    absl::string_view module_name, absl::string_view module_content) const {
  env_->module_store()->set_module_code(module_name, module_content);
  return env_->module_store()->ImportModule(module_name);
}

void AnalysisTest::CheckCode(absl::string_view test_name,
                             absl::string_view module_name,
                             absl::string_view code) const {
  ASSERT_OK_AND_ASSIGN(auto module, ImportCode(module_name, code));
  auto proto = module->ToProto();
  // Not messing with generally incompatible std::filesystem
  const std::string proto_file(
      absl::StrCat(search_path_, "/", test_name, "/", module_name, ".pb"));
  std::ifstream infile(proto_file, std::ios::in | std::ios::binary);
  CHECK(infile.is_open()) << " For: " << proto_file;
  infile.seekg(0, std::ios::end);
  const size_t size = infile.tellg();
  infile.seekg(0);
  std::string proto_text(size, '\0');
  infile.read(&proto_text[0], size);
  infile.close();
  EXPECT_THAT(proto, EqualsProto(std::string(proto_text)));
  // This just exercises the basic converter, don't want to check
  // pseudocode against anything:
  ASSERT_OK_AND_ASSIGN(auto pseudocode, BasicConverter().ConvertModule(module));
  EXPECT_FALSE(pseudocode.empty());
  EXPECT_FALSE(module->DebugString().empty());
}

void AnalysisTest::PrepareCode(absl::string_view test_name,
                               absl::string_view module_name,
                               absl::string_view code, bool force_write) const {
  auto module_result = ImportCode(module_name, code);
  CHECK_OK(module_result.status()) << "For code:" << std::endl
                                   << code << std::endl
                                   << "---" << std::endl;
  auto module = std::move(module_result).value();
  auto proto = module->ToProto();
  ASSERT_OK_AND_ASSIGN(auto pseudocode, BasicConverter().ConvertModule(module));
  std::cout << "  CheckCode(" << std::endl
            << "    R\"(" << code << ")\", R\"(" << std::endl
            << proto.DebugString() << ")\");" << std::endl
            << "Pseudocode:" << std::endl
            << pseudocode << std::endl
            << "---" << std::endl
            << "Original:" << std::endl
            << code << std::endl
            << "---" << std::endl;
  const std::string proto_file(
      absl::StrCat(search_path_, "/", test_name, "/", module_name, ".pb"));
  std::cout << "Writing to: " << proto_file << std::endl << "Confirm [Y/n]: ";
  std::string confirm("Y");
  if (!force_write) {
    std::getline(std::cin, confirm);
  }
  if (confirm.empty() || absl::StartsWith(confirm, "Y") ||
      absl::StartsWith(confirm, "y")) {
    std::ofstream ofile(proto_file, std::ios::out | std::ios::trunc);
    CHECK(ofile.is_open()) << " For: " << proto_file;
    const std::string proto_str(proto.DebugString());
    ofile.write(proto_str.data(), proto_str.size());
    ofile.close();
    std::cout << ".. File written" << std::endl;
  } else {
    std::cout << ".. Skipping" << std::endl;
  }
}

void AnalysisTest::CheckError(absl::string_view module_name,
                              absl::string_view code,
                              absl::string_view expected_error) const {
  auto module_result = ImportCode(module_name, code);
  ASSERT_FALSE(module_result.ok()) << "For: " << std::endl << code << std::endl;
  EXPECT_THAT(
      absl::StrJoin(nudl::analysis::ExtractErrorLines(module_result.status()),
                    "\n"),
      ::testing::HasSubstr(expected_error))
      << "For: " << std::endl
      << code << std::endl
      << module_result.status();
}

}  // namespace analysis
}  // namespace nudl
