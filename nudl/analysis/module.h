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
#ifndef NUDL_ANALYSIS_MODULE_H__
#define NUDL_ANALYSIS_MODULE_H__

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
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "nudl/analysis/errors.h"
#include "nudl/analysis/pragma.h"
#include "nudl/analysis/scope.h"
#include "nudl/proto/analysis.pb.h"
#include "nudl/proto/dsl.pb.h"

namespace nudl {
namespace analysis {

// Interface for reading a module from disk.
class ModuleFileReader {
 public:
  ModuleFileReader();
  virtual ~ModuleFileReader();

  // Result from reading a module content from the dik
  struct ModuleReadResult {
    std::string module_name;
    std_filesystem::path path_used;
    std_filesystem::path file_name;
    bool is_init_module = false;
    std::string content;
  };

  // Virtual function to do the actual reading.
  virtual absl::StatusOr<ModuleReadResult> ReadModule(
      absl::string_view module_name) const = 0;

  // Adds a search path to this reader.
  virtual absl::Status AddSearchPath(absl::string_view search_path) = 0;

  // Converts a module path to a disk path. I.e a.b.c => a.b.c
  static std::string ModuleNameToPath(absl::string_view module_name);
};

inline constexpr absl::string_view kDefaultFileExtension = ".ndl";
inline constexpr absl::string_view kDefaultModuleFile = "__init__.ndl";
inline constexpr absl::string_view kBuildtinModuleName = "__builtin__";

// Reads modules from disk, by searching their corresponding
// paths in the provided search path order.
class PathBasedFileReader : public ModuleFileReader {
 public:
  static absl::StatusOr<PathBasedFileReader> Build(
      std::vector<std::string> search_paths,
      absl::string_view extension = kDefaultFileExtension,
      absl::string_view default_file = kDefaultModuleFile);

  PathBasedFileReader(std::vector<std_filesystem::path> search_paths,
                      absl::string_view extension = kDefaultFileExtension,
                      absl::string_view default_file = kDefaultModuleFile);

  absl::StatusOr<ModuleFileReader::ModuleReadResult> ReadModule(
      absl::string_view module_name) const override;
  absl::Status AddSearchPath(absl::string_view search_path) override;

  static constexpr size_t kMaxImportFileSize = 100ul << 20;

  absl::StatusOr<ModuleFileReader::ModuleReadResult> ReadFile(
      const std_filesystem::path& crt_path,
      ModuleFileReader::ModuleReadResult result) const;

 protected:
  std::vector<std_filesystem::path> search_paths_;
  const std::string extension_;
  const std::string default_file_;
};

class Module;

class ModuleStore {
 public:
  ModuleStore(std::unique_ptr<ModuleFileReader> reader, Scope* built_in_scope);

  // If the provided module is read and parsed in this store.
  bool HasModule(absl::string_view module_name) const;
  // Returns the read and parsed module from this store.
  // Returns nullptr if not found.
  absl::optional<Module*> GetModule(absl::string_view module_name) const;

  // Imports the provided module. If not in this store, uses the
  // provided reader to load it from the disk (presumably).
  absl::StatusOr<Module*> ImportModule(
      absl::string_view module_name,
      std::vector<std::string>* import_chain = nullptr);

  // Accessors:
  ModuleFileReader* reader() const;
  Scope* built_in_scope() const;
  Module* top_module() const;

  const absl::flat_hash_map<std::string, Module*>& modules() const;

  // This can be used to set some default code for specific modules.
  // Generally for testing.
  void set_module_code(absl::string_view module_name,
                       absl::string_view module_code);

 private:
  std::unique_ptr<ModuleFileReader> reader_;
  Scope* const built_in_scope_;
  std::unique_ptr<Module> top_module_;
  absl::flat_hash_map<std::string, Module*> modules_;
  absl::flat_hash_map<std::string, std::string> module_code_;
};

class TypeStruct;

class Module : public Scope {
 public:
  static absl::StatusOr<Module*> ParseAndImport(
      const ModuleFileReader::ModuleReadResult& read_result,
      ModuleStore* module_store, std::vector<std::string>* import_chain);

  static absl::StatusOr<std::unique_ptr<Module>> ParseBuiltin(
      std_filesystem::path file_path, const pb::Module& module);

  static std::unique_ptr<Module> BuildTopModule(ModuleStore* module_store);

  const std_filesystem::path& file_path() const;
  pb::ObjectKind kind() const override;
  const TypeSpec* type_spec() const override;
  const std::string& module_name() const;
  PragmaHandler* pragma_handler();
  // ANTLR4 parse time:
  absl::Duration parse_duration() const;
  // Type and binding analysis type:
  absl::Duration analysis_duration() const;
  // If a main function is defined in this module, it is this one:
  absl::optional<Function*> main_function() const;

  // Imports the definitions in this module from proto.
  absl::Status Import(const pb::Module& module,
                      std::vector<std::string>* import_chain);

  // This is to set the module store just in the builtin module.
  void set_module_store(ModuleStore* module_store);

  // This designates if the module is a directory/__init__.ndl module.
  bool is_init_module() const;

  std::string DebugString() const override;
  pb::ModuleSpec ToProto() const;

  ~Module() override;

 protected:
  explicit Module(std_filesystem::path file_path);
  explicit Module(ModuleStore* module_store);
  Module(std::shared_ptr<ScopeName> scope_name, absl::string_view name,
         std_filesystem::path file_path, ModuleStore* store);

  absl::StatusOr<VarBase*> ValidateAssignment(
      const ScopedName& name, NamedObject* object) const override;

  absl::Status ProcessImport(const pb::ImportStatement& element,
                             const CodeContext& context,
                             std::vector<std::string>* import_chain);
  absl::Status ProcessSchema(const pb::SchemaDefinition& element,
                             const CodeContext& context);
  absl::Status ProcessFunctionDef(const pb::FunctionDefinition& element,
                                  const CodeContext& context);
  absl::Status ProcessAssignment(const pb::Assignment& element,
                                 const CodeContext& context);
  absl::Status ProcessPragma(const pb::PragmaExpression& element,
                             const CodeContext& context);
  absl::Status ProcessTypeDef(const pb::TypeDefinition& element,
                              const CodeContext& context);
  absl::Status RegisterTypeCallback(TypeSpec* type_spec);
  absl::Status RegisterStructureConstructors(TypeStruct* type_spec,
                                             const CodeContext& context);
  TypeStore* registration_store() const;

  std_filesystem::path file_path_;
  const std::string module_name_;
  ModuleStore* module_store_ = nullptr;
  std::unique_ptr<TypeSpec> module_type_;
  absl::optional<Function*> main_function_;
  bool is_init_module_ = false;
  PragmaHandler pragma_handler_;
  absl::flat_hash_set<const TypeSpec*> registered_struct_types_;
  absl::Duration parse_duration_;
  absl::Duration analysis_duration_;
  friend class Environment;
};

class Environment {
 public:
  static absl::StatusOr<std::unique_ptr<Environment>> Build(
      absl::string_view main_builtin_path,
      std::vector<std::string> search_paths);

  Environment(std::unique_ptr<Module> builtin_module,
              std::unique_ptr<ModuleStore> module_store);

  Module* builtin_module() const;
  ModuleStore* module_store() const;

 private:
  std::unique_ptr<Module> builtin_module_;
  std::unique_ptr<ModuleStore> module_store_;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_MODULE_H__
