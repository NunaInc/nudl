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

#include <iostream>
#include <string>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "nudl/conversion/convert_flags.h"
#include "nudl/conversion/convert_tool.h"
#include "nudl/status/status.h"

ABSL_DECLARE_FLAG(std::string, status_annotate_joiner);
DECLARE_bool(alsologtostderr);

int main(int argc, char* argv[]) {
  std::cout << "Running Nudl Converter under: "
            << std_filesystem::current_path().native()
            << " with command line:" << std::endl;
  for (int i = 0; i < argc; ++i) {
    std::cout << argv[i] << " ";
  }
  std::cout << std::endl;
  google::InitGoogleLogging(argv[0]);
  absl::InitializeSymbolizer(argv[0]);
  absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());
  absl::ParseCommandLine(argc, argv);
  absl::SetFlag(&FLAGS_status_annotate_joiner, ";\n    ");
  FLAGS_alsologtostderr = true;
  return nudl::LogErrorLines(
      "Running nudl conversion",
      nudl::RunConvertTool(nudl::ConvertOptionsFromFlags()));
}
