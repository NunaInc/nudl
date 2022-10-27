#include <iostream>
#include <string>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "nudl/analysis/basic_converter.h"
#include "nudl/analysis/errors.h"
#include "nudl/analysis/module.h"
#include "nudl/analysis/python_converter.h"
#include "nudl/status/status.h"

ABSL_FLAG(std::string, builtin_path, "",
          "Directory / file containing the builtin module content.");
ABSL_FLAG(std::string, search_paths, "",
          "Comma separated lists of paths to search for modules");
ABSL_FLAG(std::string, input, "", "Input module to load and convert");
ABSL_FLAG(bool, debug_modules, false,
          "If true writes out the debug strings of the modules");
ABSL_FLAG(std::string, lang, "python", "Language to convert to");
ABSL_DECLARE_FLAG(std::string, status_annotate_joiner);
DECLARE_bool(alsologtostderr);

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  // google::InstallFailureSignalHandler();
  absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());
  absl::ParseCommandLine(argc, argv);
  absl::SetFlag(&FLAGS_status_annotate_joiner, ";\n    ");
  FLAGS_alsologtostderr = true;
  CHECK(!absl::GetFlag(FLAGS_builtin_path).empty());
  std::vector<std::string> search_paths =
      absl::StrSplit(absl::GetFlag(FLAGS_search_paths), ",", absl::SkipEmpty());
  std::cout << "Building environment" << std::endl;
  auto env_result = nudl::analysis::Environment::Build(
      absl::GetFlag(FLAGS_builtin_path), std::move(search_paths));
  if (!env_result.ok()) {
    std::cout << "Error building environment: " << env_result.status().message()
              << std::endl;
    for (const auto& err :
         nudl::analysis::ExtractErrorLines(env_result.status())) {
      std::cerr << "  " << err << std::endl;
    }
    return -1;
  }
  if (absl::GetFlag(FLAGS_input).empty()) {
    std::cerr << "No input file provided\n";
    return -1;
  }
  auto store = env_result.value()->module_store();
  auto module_result = store->ImportModule(absl::GetFlag(FLAGS_input));
  if (!module_result.ok()) {
    std::cerr << "Error importing module: " << module_result.status().message()
              << std::endl;
    for (const auto& err :
         nudl::analysis::ExtractErrorLines(module_result.status())) {
      std::cerr << err << std::endl;
    }
    return -1;
  }
  std::unique_ptr<nudl::analysis::Converter> converter;
  const std::string lang_name = absl::GetFlag(FLAGS_lang);
  if (lang_name == "python") {
    converter = std::make_unique<nudl::analysis::PythonConverter>();
  } else if (lang_name == "pseudo") {
    converter = std::make_unique<nudl::analysis::BasicConverter>();
  } else {
    std::cerr << "Unknown language name: " << lang_name;
    return -1;
  }
  for (const auto& it : store->modules()) {
    if (absl::GetFlag(FLAGS_debug_modules)) {
      std::cout << it.second->DebugString() << std::endl;
    }
    auto convert_result = converter->ConvertModule(it.second);
    if (convert_result.ok()) {
      std::cout << "Module: " << it.first << std::endl
                << ">>>>>>>>>" << std::endl
                << convert_result.value() << "<<<<<<<<<" << std::endl;
    } else {
      std::cerr << "Error converting module: " << it.first << ": "
                << convert_result.status() << std::endl;
    }
  }
  std::cout << "Timing info: " << std::endl;
  std::cout << "  `builtin`:  parse: "
            << env_result.value()->builtin_module()->parse_duration()
            << ", analysis: "
            << env_result.value()->builtin_module()->analysis_duration()
            << std::endl;
  for (const auto& it : store->modules()) {
    std::cout << "  `" << it.second->name()
              << "`:  parse: " << it.second->parse_duration()
              << ", analysis: " << it.second->analysis_duration() << std::endl;
  }
  return 0;
}
