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

#include "nudl/analysis/testing/analysis_test.h"

#include <fstream>
#include <iostream>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nudl/conversion/pseudo_converter.h"
#include "nudl/conversion/python_converter.h"
#include "nudl/status/status.h"
#include "nudl/status/testing.h"
#include "nudl/testing/protobuf_matchers.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);
ABSL_FLAG(bool, nudl_test_update, false,
          "Goes into a Prepare stage if the generated expectations "
          "are not met");

namespace nudl {
namespace analysis {

AnalysisTest::~AnalysisTest() {
  std::cout << "Total time: " << total_duration_ << std::endl
            << "    setup_time:      " << setup_duration_ << std::endl
            << "    parse_time:      " << parse_duration_ << std::endl
            << "    analysis_time:   " << analysis_duration_ << std::endl
            << "    convert_time:    " << convert_duration_ << std::endl
            << "    read_file_time:  " << read_file_duration_ << std::endl
            << "    compare_time:    " << compare_duration_ << std::endl
            << "    regenerate_time: " << regenerate_duration_ << std::endl;
  if (env_->builtin_module()) {
    std::cout << "Builtin parse time:  "
              << env_->builtin_module()->parse_duration() << std::endl
              << "Builtin analysis time: "
              << env_->builtin_module()->analysis_duration() << std::endl;
  }
}

void AnalysisTest::SetUp() {
  absl::SetFlag(&FLAGS_nudl_short_analysis_proto, true);
  const absl::Time start_time = absl::Now();
  ASSERT_OK_AND_ASSIGN(env_, Environment::Build(builtin_file_, {search_path_}));
  setup_duration_ += absl::Now() - start_time;
}

Environment* AnalysisTest::env() const { return env_.get(); }

absl::StatusOr<Module*> AnalysisTest::ImportCode(
    absl::string_view module_name, absl::string_view module_content) const {
  env_->module_store()->set_module_code(module_name, module_content);
  ASSIGN_OR_RETURN(auto module, env_->module_store()->ImportModule(module_name),
                   _ << "\nFor test module name: " << module_name);
  return module;
}

void AnalysisTest::CheckCode(absl::string_view test_name,
                             absl::string_view module_name,
                             absl::string_view code) {
  const absl::Time start_time = absl::Now();
  ASSERT_OK_AND_ASSIGN(auto module, ImportCode(module_name, code));
  parse_duration_ += module->parse_duration();
  analysis_duration_ += module->analysis_duration();
  const absl::Time parsed_time = absl::Now();
  auto proto = module->ToProto();
  const absl::Time converted_time = absl::Now();
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
  const absl::Time read_time = absl::Now();
  if (absl::GetFlag(FLAGS_nudl_test_update)) {
    internal::ProtoComparison comp;
    comp.field_comp = internal::kProtoEqual;
    if (!internal::ProtoCompare(comp, proto, std::string(proto_text))) {
      WritePreparedCode(module, proto, test_name, module_name, code, false);
      return;
    }
  }
  EXPECT_THAT(proto, EqualsProto(std::string(proto_text)));
  const absl::Time compared_time = absl::Now();
  // This just exercises the basic converter, don't want to check
  // pseudocode against anything:
  ASSERT_OK_AND_ASSIGN(auto pseudocode,
                       conversion::PseudoConverter().ConvertModule(module));
  ASSERT_OK_AND_ASSIGN(auto pythoncode,
                       conversion::PythonConverter().ConvertModule(module));
  EXPECT_FALSE(pseudocode.empty());
  EXPECT_FALSE(pythoncode.empty());
  EXPECT_FALSE(module->DebugString().empty());
  const absl::Time regenerated_time = absl::Now();
  convert_duration_ += (converted_time - parsed_time);
  read_file_duration_ += (read_time - converted_time);
  compare_duration_ += (compared_time - read_time);
  regenerate_duration_ += (regenerated_time - compared_time);
  total_duration_ += regenerated_time - start_time;
}

void AnalysisTest::PrepareCode(absl::string_view test_name,
                               absl::string_view module_name,
                               absl::string_view code, bool skip_write) const {
  auto module_result = ImportCode(module_name, code);
  CHECK_OK(module_result.status()) << "For code:\n" << code;
  auto module = std::move(module_result).value();
  auto proto = module->ToProto();
  WritePreparedCode(module, proto, test_name, module_name, code, skip_write);
}

void AnalysisTest::WritePreparedCode(Module* module,
                                     const pb::ModuleSpec& proto,
                                     absl::string_view test_name,
                                     absl::string_view module_name,
                                     absl::string_view code,
                                     bool skip_write) const {
  ASSERT_OK_AND_ASSIGN(auto pseudocode,
                       conversion::PseudoConverter().ConvertModule(module));
  ASSERT_OK_AND_ASSIGN(auto pythoncode,
                       conversion::PythonConverter().ConvertModule(module));
  std::cout << "  CheckCode(" << std::endl
            << "    R\"(" << code << ")\", R\"(" << std::endl
            << proto.DebugString() << ")\");" << std::endl
            << "Pseudocode:" << std::endl
            << pseudocode << std::endl
            << "---" << std::endl
            << "Pythoncode:" << std::endl
            << pythoncode << std::endl
            << "---" << std::endl
            << "Original:" << std::endl
            << code << std::endl
            << "---" << std::endl;
  const std::string proto_file(
      absl::StrCat(search_path_, "/", test_name, "/", module_name, ".pb"));
  std::cout << "Writing to: " << proto_file << std::endl << "Confirm [Y/n]: ";
  std::string confirm("N");
  if (!skip_write) {
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

int AnalysisTest::Main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());
  absl::ParseCommandLine(argc, argv);
  return RUN_ALL_TESTS();
}

}  // namespace analysis
}  // namespace nudl
