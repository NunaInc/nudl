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
#ifndef NUDL_CONVERSION_CONVERT_TOOL_H__
#define NUDL_CONVERSION_CONVERT_TOOL_H__

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 8)
#include <filesystem>
namespace std_filesystem = std::filesystem;
#else
#include <experimental/filesystem>
namespace std_filesystem = std::experimental::filesystem;
#endif

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nudl/analysis/analysis.h"
#include "nudl/conversion/converter.h"

namespace nudl {

enum class ConvertLang { PSEUDO_CODE, PYTHON };

absl::StatusOr<ConvertLang> ConvertLangFromName(absl::string_view lang_name);

int LogErrorLines(absl::string_view context, const absl::Status& err_status);

class ConvertTool {
 public:
  ConvertTool(absl::string_view builtin_path,
              std::vector<std::string> search_paths, ConvertLang lang,
              absl::string_view run_yapf, bool write_only_input,
              bool bindings_on_use);
  absl::Status Prepare();
  void AddBuiltinModule();
  absl::Status LoadModule(absl::string_view module_name);
  absl::Status LoadModuleFromString(absl::string_view module_name,
                                    absl::string_view code);
  absl::Status WritePythonOutput(
      absl::string_view output_path, absl::string_view py_path,
      bool direct_output,
      const absl::flat_hash_map<std::string, std::string>& output_dirs);
  absl::Status WriteConversionToStdout();
  void WriteTimingInfoToStdout();
  absl::StatusOr<std::string> ConvertToString();

 private:
  static void PythonPreparePath(const std_filesystem::path& file_path,
                                const std_filesystem::path& base_path);
  static absl::Status WriteFile(const std_filesystem::path& file_path,
                                const std::string& content);
  absl::Status RunYapf(const std_filesystem::path& file_path) const;
  void IterateModules(std::function<void(analysis::Module*)> runner);

  const std::string builtin_path_;
  const std::vector<std::string> search_paths_;
  std::unique_ptr<conversion::Converter> converter_;
  const std::string run_yapf_;
  std::unique_ptr<analysis::Environment> env_;
  analysis::ModuleStore* store_ = nullptr;
  absl::flat_hash_set<analysis::Module*> modules_;
  const bool write_only_input_;
};

struct ConvertToolOptions {
  // Path to the builtin nudl module.
  std::string builtin_path;
  // Where to search for modules. Can be directories or just files.
  std::vector<std::string> search_paths;
  // Module to convert - specified as a Nudl module.
  std::string input_module;
  // Second mode: paths to the files to convert.
  std::vector<std::string> input_paths;
  // Possible base directories of the files to convert.
  // E.g. if the input file is `a/b/c/d.ndl`, but an import is
  // specified as `a/b`, we remove this prefix and consider
  // the corresponding input module as c.d"
  std::vector<std::string> imports;
  // Directory with nudle core library
  std::string py_path;
  // If not empty, output to this directory.
  std::string output_dir;
  // If non empty, we run the yapf code formatter on resulting
  // python code. This is the path to the yapf binary.
  std::string run_yapf;
  // If true writes out the debug strings of the modules
  bool debug_modules = false;
  // If true we write to output only the module we got as input.
  bool write_only_input = false;
  // Convert specific function bindings only in the places where
  // they are used.
  bool bindings_on_use = false;
  // If true, we output the files to --output_dir without
  // maintaining directory structure.
  bool direct_output = false;
  // Language to convert to:
  ConvertLang lang = ConvertLang::PYTHON;
};

absl::Status RunConvertTool(const ConvertToolOptions& options);

// This conversion function is to be wrapped in Cython for ad-hoc
// conversion of Nudl snippets.
// If errors occur, we place them in errors, and return an empty
// string.
std::string ConvertPythonSource(
    const std::string& module_name,
    const std::string& code,
    const std::string& builtin_path,
    const std::vector<std::string>& search_paths,
    std::vector<std::string>* errors);


}  // namespace nudl

#endif  // NUDL_CONVERSION_CONVERT_TOOL_H__
