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
#include "nudl/conversion/convert_tool.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <utility>

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "glog/logging.h"
#include "nudl/conversion/pseudo_converter.h"
#include "nudl/conversion/python_converter.h"
#include "nudl/status/status.h"

namespace nudl {

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

namespace {

// Iterates the python file paths under py_path.
// The processor is called with the relative path from py_path.
void IteratePythonFiles(
    absl::string_view py_path,
    std::function<void(const std_filesystem::path&, absl::string_view)>
        processor) {
  if (py_path.empty()) {
    return;
  }
  std_filesystem::path src_path(py_path);
  for (const auto& filename :
       std_filesystem::recursive_directory_iterator(src_path)) {
    auto crt_path = filename.path();
    if (std_filesystem::is_regular_file(crt_path)) {
      const std::string crt_name = crt_path.native();
      if (absl::EndsWith(crt_name, ".py")) {
        processor(crt_path,
                  absl::StripPrefix(absl::StripPrefix(crt_name, py_path), "/"));
      }
    }
  }
}
}  // namespace

ConvertTool::ConvertTool(absl::string_view builtin_path,
                         std::vector<std::string> search_paths,
                         ConvertLang lang, absl::string_view run_yapf,
                         bool write_only_input, bool bindings_on_use)
    : builtin_path_(builtin_path),
      search_paths_(std::move(search_paths)),
      converter_(CHECK_NOTNULL(BuildConverter(lang, bindings_on_use))),
      run_yapf_(run_yapf),
      write_only_input_(write_only_input) {}

absl::Status ConvertTool::Prepare() {
  ASSIGN_OR_RETURN(
      env_, nudl::analysis::Environment::Build(builtin_path_, search_paths_),
      _ << "Building environment");
  store_ = env_->module_store();
  return absl::OkStatus();
}

void ConvertTool::AddBuiltinModule() {
  CHECK(store_ != nullptr) << "Tool not properly prepared.";
  modules_.insert(env_->builtin_module());
}

absl::Status ConvertTool::LoadModule(absl::string_view module_name) {
  RET_CHECK(store_ != nullptr) << "Tool not properly prepared.";
  ASSIGN_OR_RETURN(auto module, store_->ImportModule(module_name));
  LOG(INFO) << "Module: " << module->module_name() << " loaded OK" << std::endl;
  modules_.insert(module);
  return absl::OkStatus();
}

absl::Status ConvertTool::WritePythonOutput(
    absl::string_view output_path, absl::string_view py_path,
    bool direct_output,
    const absl::flat_hash_map<std::string, std::string>& output_dirs) {
  if (output_path.empty()) {
    std::cout << "Skipping file output.";
    return absl::OkStatus();
  }
  std_filesystem::path dest_path(output_path);
  std_filesystem::create_directories(dest_path);
  absl::Status error;
  IterateModules([this, direct_output, &error, &dest_path,
                  &output_dirs](analysis::Module* module) {
    auto convert_result = converter_->ConvertModule(module);
    if (!convert_result.ok()) {
      status::UpdateOrAnnotate(error, convert_result.status());
      return;
    }
    for (const auto& file_spec : convert_result.value().files) {
      std_filesystem::path file_path = dest_path / file_spec.file_name;
      if (output_dirs.contains(module->name())) {
        auto it = output_dirs.find(module->name());
        file_path = dest_path / it->second / file_path.filename();
        PythonPreparePath(file_path, dest_path);
      } else if (direct_output) {
        file_path = dest_path / file_path.filename();
      } else {
        PythonPreparePath(file_path, dest_path);
      }
      status::UpdateOrAnnotate(error, WriteFile(file_path, file_spec.content));
      std::cout << "Written: " << file_path.native() << " with "
                << module->module_name() << std::endl;
      if (!run_yapf_.empty()) {
        status::UpdateOrAnnotate(error, RunYapf(file_path));
      }
    }
  });
  IteratePythonFiles(py_path, [&dest_path](const std_filesystem::path& crt_path,
                                           absl::string_view out_path) {
    auto crt_dest = dest_path / std::string(out_path);
    std_filesystem::create_directories(crt_dest.parent_path());
    std_filesystem::copy_file(crt_path, crt_dest,
                              std_filesystem::copy_options::overwrite_existing);
  });
  return error;
}

absl::Status ConvertTool::WriteConversionToStdout() {
  absl::Status error;
  IterateModules([this, &error](analysis::Module* module) {
    auto convert_result = converter_->ConvertModule(module);
    if (!convert_result.ok()) {
      status::UpdateOrAnnotate(error, convert_result.status());
      return;
    }
    for (const auto& file_spec : convert_result.value().files) {
      std::cout << "Module: " << module->module_name() << std::endl
                << "File: " << file_spec.file_name << std::endl
                << ">>>>>>>>>" << std::endl
                << file_spec.content << "<<<<<<<<<" << std::endl;
    }
  });
  return error;
}

void ConvertTool::WriteTimingInfoToStdout() {
  std::cout << "Timing information:" << std::endl;
  IterateModules([](analysis::Module* module) {
    std::cout << "  `" << module->name()
              << "`:  parse: " << module->parse_duration()
              << ", analysis: " << module->analysis_duration() << std::endl;
  });
}

void ConvertTool::PythonPreparePath(const std_filesystem::path& file_path,
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

absl::Status ConvertTool::WriteFile(const std_filesystem::path& file_path,
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

absl::Status ConvertTool::RunYapf(const std_filesystem::path& file_path) const {
  RET_CHECK(!run_yapf_.empty());
  const std::string command =
      absl::StrCat(run_yapf_, " -i --style=Google '", file_path.native(), "'");
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

void ConvertTool::IterateModules(
    std::function<void(analysis::Module*)> runner) {
  if (write_only_input_) {
    for (auto module : modules_) {
      runner(CHECK_NOTNULL(module));
    }
    return;
  }
  runner(env_->builtin_module());
  for (const auto& it : store_->modules()) {
    runner(it.second);
  }
}

absl::Status RunConvertTool(const ConvertToolOptions& options) {
  RET_CHECK(!options.builtin_path.empty()) << "Please specify builtin_path";
  std::vector<std::string> search_paths = options.imports;
  std::vector<std::string> base_dirs = options.imports;
  std::copy(options.search_paths.begin(), options.search_paths.end(),
            std::back_inserter(search_paths));
  ConvertTool tool(options.builtin_path, std::move(search_paths), options.lang,
                   options.run_yapf, options.write_only_input,
                   options.bindings_on_use);
  RETURN_IF_ERROR(tool.Prepare()) << "Preparing environment";
  if (!options.input_module.empty()) {
    RETURN_IF_ERROR(tool.LoadModule(options.input_module))
        << "Loading module: " << options.input_module;
  }
  absl::flat_hash_map<std::string, std::string> output_dirs;
  for (const auto& input_file : options.input_paths) {
    if (input_file == options.builtin_path) {
      tool.AddBuiltinModule();
      continue;
    }
    std::string ifile = input_file;
    for (const auto& bd : base_dirs) {
      if (bd.empty() || bd == ".") {
        continue;
      }
      auto pos = ifile.find(bd);
      if (pos != std::string::npos) {
        ifile = absl::StripPrefix(input_file.substr(pos), bd);
        break;
      }
    }
    auto module_name = absl::StrJoin(
        absl::StrSplit(absl::StripSuffix(absl::StripPrefix(ifile, "/"), ".ndl"),
                       '/'),
        ".");
    std::vector<std::string> components = absl::StrSplit(module_name, ".");
    if (components.size() > 1) {
      components.pop_back();
      output_dirs.emplace(module_name, absl::StrJoin(components, "/"));
    }
    std::cout << "Loading: `" << module_name << "` from: `" << input_file << "`"
              << std::endl;
    RETURN_IF_ERROR(tool.LoadModule(module_name))
        << "Loading module: " << module_name;
  }
  if (options.output_dir.empty()) {
    RETURN_IF_ERROR(tool.WriteConversionToStdout());
  } else if (options.lang == ConvertLang::PYTHON) {
    RETURN_IF_ERROR(tool.WritePythonOutput(options.output_dir, options.py_path,
                                           options.direct_output, output_dirs));
  }
  tool.WriteTimingInfoToStdout();
  return absl::OkStatus();
}

}  // namespace nudl
