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

#include "nudl/conversion/convert_flags.h"

#include <string>
#include <vector>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, builtin_path, "",
          "File containing the builtin module content.");
ABSL_FLAG(std::vector<std::string>, search_paths, {},
          "Comma separated lists of paths to search for modules");
ABSL_FLAG(std::string, input, "", "Input module to load and convert");
ABSL_FLAG(std::vector<std::string>, input_paths, {},
          "Alternate list of files to load");
ABSL_FLAG(std::vector<std::string>, imports, {},
          "Possible base directories of the files to convert. E.g. "
          "if the input file is `a/b/c/d.ndl`, but an import is "
          "specified as `a/b`, we remove this prefix and consider "
          "the corresponding input module as c.d");
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

namespace nudl {

ConvertToolOptions ConvertOptionsFromFlags() {
  auto lang_result = ConvertLangFromName(absl::GetFlag(FLAGS_lang));
  CHECK(lang_result.ok()) << lang_result.status()
                          << " - invalid value for --lang";
  return ConvertToolOptions{
      absl::GetFlag(FLAGS_builtin_path),
      absl::GetFlag(FLAGS_search_paths),
      absl::GetFlag(FLAGS_input),
      absl::GetFlag(FLAGS_input_paths),
      absl::GetFlag(FLAGS_imports),
      absl::GetFlag(FLAGS_py_path),
      absl::GetFlag(FLAGS_output_dir),
      absl::GetFlag(FLAGS_run_yapf),
      absl::GetFlag(FLAGS_debug_modules),
      absl::GetFlag(FLAGS_write_only_input),
      absl::GetFlag(FLAGS_bindings_on_use),
      lang_result.value(),
  };
}

}  // namespace nudl
