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

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 8)
#include <filesystem>
namespace std_filesystem = std::filesystem;
#else
#include <experimental/filesystem>
namespace std_filesystem = std::experimental::filesystem;
#endif

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "nudl/analysis/errors.h"
#include "nudl/analysis/module.h"
#include "nudl/conversion/pseudo_converter.h"
#include "nudl/conversion/python_converter.h"
#include "nudl/status/status.h"

ABSL_FLAG(std::string, builtin_path, "",
          "Directory / file containing the builtin module content.");
ABSL_FLAG(std::string, search_paths, "",
          "Comma separated lists of paths to search for modules");
ABSL_FLAG(std::string, input, "", "Input module to load and convert");
ABSL_FLAG(bool, debug_modules, false,
          "If true writes out the debug strings of the modules");
ABSL_FLAG(std::string, py_path, "", "Directory with nudle core library.");
ABSL_FLAG(std::string, output_dir, "",
          "If not empty, output to this directory.");
ABSL_FLAG(std::string, run_yapf, "",
          "If non empty, we run the yapf code formatter on resulting "
          "python code. This is the path to the yapf binary.");
ABSL_FLAG(bool, write_only_input, false,
          "If true we write to output only the module we got as input");
ABSL_FLAG(bool, bindings_on_use, false,
          "Convert specific function bindings only in the places where "
          "they are used");
ABSL_FLAG(std::string, lang, "python", "Language to convert to");
ABSL_DECLARE_FLAG(std::string, status_annotate_joiner);
DECLARE_bool(alsologtostderr);

namespace nudl {

enum class ConvertLang { PSEUDO_CODE, PYTHON };
std::unique_ptr<conversion::Converter> BuildConverter(ConvertLang lang,
                                                      bool bindings_on_use) {
  switch (lang) {
    case ConvertLang::PSEUDO_CODE:
      return std::make_unique<conversion::PseudoConverter>();
    case ConvertLang::PYTHON:
      return std::make_unique<conversion::PythonConverter>(bindings_on_use);
  }
  return nullptr;
}

absl::StatusOr<ConvertLang> ConvertLangFromName(absl::string_view lang_name) {
  if (lang_name == "python") {
    return ConvertLang::PYTHON;
  } else if (lang_name == "pseudo") {
    return ConvertLang::PSEUDO_CODE;
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown language: ", lang_name));
  }
}

int LogErrorLines(absl::string_view context, const absl::Status& err_status) {
  if (err_status.ok()) {
    return 0;
  }
  std::cout << "Error " << context << " - " << err_status.message()
            << std::endl;
  for (const auto& err : analysis::ExtractErrorLines(err_status)) {
    std::cerr << "  " << err << std::endl;
  }
  return -1;
}

class ConvertTool {
 public:
  ConvertTool(absl::string_view builtin_path,
              std::vector<std::string> search_paths, ConvertLang lang,
              absl::string_view run_yapf, bool write_only_input,
              bool bindings_on_use)
      : builtin_path_(builtin_path),
        search_paths_(std::move(search_paths)),
        converter_(CHECK_NOTNULL(BuildConverter(lang, bindings_on_use))),
        run_yapf_(run_yapf),
        write_only_input_(write_only_input) {}

  absl::Status Prepare() {
    ASSIGN_OR_RETURN(
        env_, nudl::analysis::Environment::Build(builtin_path_, search_paths_),
        _ << "Building environment");
    store_ = env_->module_store();
    return absl::OkStatus();
  }

  absl::Status LoadModule(absl::string_view module_name) {
    RET_CHECK(store_ != nullptr) << "Tool not properly prepared.";
    ASSIGN_OR_RETURN(module_, store_->ImportModule(module_name));
    std::cout << "Module: " << module_->module_name()
              << " loaded OK with: " << std::endl
              << module_->DebugNames() << std::endl;

    return absl::OkStatus();
  }

  absl::Status WriteOutput(absl::string_view output_path,
                           absl::string_view py_path) {
    if (output_path.empty()) {
      std::cout << "Skipping file output.";
      return absl::OkStatus();
    }
    std_filesystem::path dest_path(output_path);
    std_filesystem::create_directories(dest_path);
    absl::Status error;
    IterateModules([this, &error, &dest_path](analysis::Module* module) {
      auto convert_result = converter_->ConvertModule(module);
      if (!convert_result.ok()) {
        status::UpdateOrAnnotate(error, convert_result.status());
        return;
      }
      const std_filesystem::path file_path(PythonFileName(dest_path, module));
      PythonPreparePath(file_path, dest_path);
      status::UpdateOrAnnotate(error,
                               WriteFile(file_path, convert_result.value()));
      std::cout << "Written: " << file_path.native() << " with "
                << module->module_name() << std::endl;
      if (!run_yapf_.empty()) {
        status::UpdateOrAnnotate(error, RunYapf(file_path));
      }
    });
    if (!py_path.empty()) {
      std_filesystem::path src_path(py_path);
      for (const auto& filename :
           std_filesystem::recursive_directory_iterator(src_path)) {
        auto crt_path = filename.path();
        if (std_filesystem::is_regular_file(crt_path)) {
          const std::string crt_name = crt_path.native();
          if (absl::EndsWith(crt_name, ".py")) {
            const absl::string_view relative_py_path =
                absl::StripPrefix(absl::StripPrefix(crt_name, py_path), "/");
            auto crt_dest = dest_path / std::string(relative_py_path);
            std_filesystem::create_directories(crt_dest.parent_path());
            std_filesystem::copy_file(
                crt_path, crt_dest,
                std_filesystem::copy_options::overwrite_existing);
          }
        }
      }
    }
    return error;
  }
  absl::Status WriteConvesionToStdout() {
    absl::Status error;
    IterateModules([this, &error](analysis::Module* module) {
      auto convert_result = converter_->ConvertModule(module);
      if (!convert_result.ok()) {
        status::UpdateOrAnnotate(error, convert_result.status());
        return;
      }
      std::cout << "Module: " << module->module_name() << std::endl
                << ">>>>>>>>>" << std::endl
                << convert_result.value() << "<<<<<<<<<" << std::endl;
    });
    return error;
  }
  void WriteTimingInfo() {
    std::cout << "Timing information:" << std::endl;
    IterateModules([](analysis::Module* module) {
      std::cout << "  `" << module->name()
                << "`:  parse: " << module->parse_duration()
                << ", analysis: " << module->analysis_duration() << std::endl;
    });
  }

 private:
  static std_filesystem::path PythonFileName(
      const std_filesystem::path& base_path, analysis::Module* module) {
    if (module->built_in_scope() == module) {
      return base_path / "_nudl_builtin.py";
    } else if (module->is_init_module()) {
      return base_path /
             analysis::ModuleFileReader::ModuleNameToPath(
                 conversion::PythonSafeName(module->module_name(), module)) /
             "__init__.py";
    } else {
      return base_path /
             absl::StrCat(
                 analysis::ModuleFileReader::ModuleNameToPath(
                     conversion::PythonSafeName(module->module_name(), module)),
                 ".py");
    }
  }
  static void PythonPreparePath(const std_filesystem::path& file_path,
                                const std_filesystem::path& base_path) {
    std_filesystem::path parent_path = file_path;
    do {
      parent_path = parent_path.parent_path();
      std_filesystem::create_directories(parent_path);
      std_filesystem::path py_init_path = parent_path / "__init__.py";
      if (!std_filesystem::exists(py_init_path)) {
        std::ofstream(py_init_path, std::ios::out | std::ios::trunc);
      }
    } while (parent_path > base_path);
  }
  static absl::Status WriteFile(const std_filesystem::path& file_path,
                                const std::string& content) {
    std::ofstream ofile(file_path, std::ios::out | std::ios::trunc);
    if (!ofile.is_open()) {
      return status::InternalErrorBuilder()
             << "Cannot open file: " << file_path.native();
    }
    ofile.write(content.data(), content.size());
    ofile.close();
    return absl::OkStatus();
  }
  absl::Status RunYapf(const std_filesystem::path& file_path) const {
    RET_CHECK(!run_yapf_.empty());
    const std::string command = absl::StrCat(run_yapf_, " -i --style=Google '",
                                             file_path.native(), "'");
    const absl::Time yapf_start = absl::Now();
    std::cout << "Running: yapf with command: " << command << std::endl;
    if (system(command.c_str())) {
      return status::InternalErrorBuilder("Error running yapf")
             << "Command: " << command;
    }
    std::cout << "Completed: yapf in: " << (absl::Now() - yapf_start)
              << std::endl;
    return absl::OkStatus();
  }

  void IterateModules(std::function<void(analysis::Module*)> runner) {
    if (write_only_input_) {
      runner(CHECK_NOTNULL(module_));
      return;
    }
    runner(env_->builtin_module());
    for (const auto& it : store_->modules()) {
      runner(it.second);
    }
  }

  const std::string builtin_path_;
  const std::vector<std::string> search_paths_;
  std::unique_ptr<conversion::Converter> converter_;
  const std::string run_yapf_;
  std::unique_ptr<analysis::Environment> env_;
  analysis::ModuleStore* store_ = nullptr;
  analysis::Module* module_ = nullptr;
  const bool write_only_input_;
};

absl::Status RunTool() {
  CHECK(!absl::GetFlag(FLAGS_builtin_path).empty());
  ASSIGN_OR_RETURN(auto lang, ConvertLangFromName(absl::GetFlag(FLAGS_lang)));
  std::vector<std::string> search_paths =
      absl::StrSplit(absl::GetFlag(FLAGS_search_paths), ",", absl::SkipEmpty());
  ConvertTool tool(absl::GetFlag(FLAGS_builtin_path), std::move(search_paths),
                   lang, absl::GetFlag(FLAGS_run_yapf),
                   absl::GetFlag(FLAGS_write_only_input),
                   absl::GetFlag(FLAGS_bindings_on_use));
  RETURN_IF_ERROR(tool.Prepare()) << "Preparing environment";
  if (!absl::GetFlag(FLAGS_input).empty()) {
    RETURN_IF_ERROR(tool.LoadModule(absl::GetFlag(FLAGS_input)))
        << "Loading module: " << absl::GetFlag(FLAGS_input);
  }
  if (absl::GetFlag(FLAGS_output_dir).empty()) {
    RETURN_IF_ERROR(tool.WriteConvesionToStdout());
  } else {
    RETURN_IF_ERROR(tool.WriteOutput(absl::GetFlag(FLAGS_output_dir),
                                     absl::GetFlag(FLAGS_py_path)));
  }
  tool.WriteTimingInfo();
  return absl::OkStatus();
}

}  // namespace nudl

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  // google::InstallFailureSignalHandler();
  absl::InitializeSymbolizer(argv[0]);
  absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());
  absl::ParseCommandLine(argc, argv);
  absl::SetFlag(&FLAGS_status_annotate_joiner, ";\n    ");
  FLAGS_alsologtostderr = true;
  return nudl::LogErrorLines("Running nudl conversion", nudl::RunTool());
}
