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

#include "nudl/analysis/module.h"

#include <fstream>
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "glog/logging.h"
#include "nudl/analysis/expression.h"
#include "nudl/analysis/function.h"
#include "nudl/analysis/types.h"
#include "nudl/grammar/dsl.h"
#include "nudl/grammar/tree_util.h"
#include "nudl/status/status.h"

namespace nudl {
namespace analysis {

namespace {
absl::StatusOr<std_filesystem::path> PathFromString(absl::string_view path) {
  try {
    return std_filesystem::path(path);
  } catch (const std_filesystem::filesystem_error& ex) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid path provided: `" << path << "` :" << ex.what();
  }
}
}  // namespace

ModuleFileReader::ModuleFileReader() {}

ModuleFileReader::~ModuleFileReader() {}

std::string ModuleFileReader::ModuleNameToPath(absl::string_view module_name) {
  return absl::StrJoin(absl::StrSplit(module_name, '.'), "/");
  // std_filesystem::path::preferred_separator ?
}

absl::StatusOr<PathBasedFileReader> PathBasedFileReader::Build(
    std::vector<std::string> search_paths, absl::string_view extension,
    absl::string_view default_file) {
  std::vector<std_filesystem::path> paths;
  for (const auto& path : search_paths) {
    ASSIGN_OR_RETURN(auto crt_path, PathFromString(path));
    paths.emplace_back(std::move(crt_path));
  }
  return {PathBasedFileReader(std::move(paths), extension, default_file)};
}

PathBasedFileReader::PathBasedFileReader(
    std::vector<std_filesystem::path> search_paths, absl::string_view extension,
    absl::string_view default_file)
    : ModuleFileReader(),
      search_paths_(std::move(search_paths)),
      extension_(extension),
      default_file_(default_file) {}

absl::StatusOr<ModuleFileReader::ModuleReadResult>
PathBasedFileReader::ReadFile(const std_filesystem::path& crt_path,
                              ModuleFileReader::ModuleReadResult result) const {
  auto size = std_filesystem::file_size(crt_path);
  if (size > kMaxImportFileSize) {
    return status::InvalidArgumentErrorBuilder()
           << "File to read too big: " << size << " for: " << crt_path.native();
  }
  std::ifstream infile(crt_path, std::ios::in | std::ios::binary);
  if (!infile.is_open()) {
    return status::InternalErrorBuilder()
           << "Error opening existing file: " << crt_path.native();
  }
  result.content.resize(size, '\0');
  infile.read(&result.content[0], size);
  const bool has_errors = infile.fail();
  infile.close();
  if (has_errors) {
    return status::InternalErrorBuilder()
           << "Error reading module file: " << crt_path.native();
  }
  return {std::move(result)};
}

absl::StatusOr<ModuleFileReader::ModuleReadResult>
PathBasedFileReader::ReadModule(absl::string_view module_name) const {
  std::vector<std::string> search_paths;
  search_paths.reserve(search_paths_.size());
  for (const auto& path : search_paths_) {
    const std::string module_path = ModuleNameToPath(module_name);
    const std::string module_file = absl::StrCat(module_path, extension_);
    const std::string module_init_file =
        (std_filesystem::path(module_path) / kDefaultModuleFile).native();
    try {
      if (std_filesystem::is_regular_file(path)) {
        std::string full_path = path.native();
        if (absl::EndsWith(full_path, absl::StrCat("/", module_file))) {
          return ReadFile(
              path, ModuleFileReader::ModuleReadResult{std::string(module_name),
                                                       path, path, false});
        }
        if (absl::EndsWith(full_path, absl::StrCat("/", module_init_file))) {
          return ReadFile(
              path, ModuleFileReader::ModuleReadResult{std::string(module_name),
                                                       path, path, true});
        }
      } else if (std_filesystem::is_directory(path)) {
        auto crt_path = path / module_file;
        if (std_filesystem::is_regular_file(crt_path)) {
          return ReadFile(crt_path,
                          ModuleFileReader::ModuleReadResult{
                              std::string(module_name), path, crt_path, false});
        }
        auto top_path = path / module_init_file;
        if (std_filesystem::is_regular_file(top_path)) {
          return ReadFile(top_path,
                          ModuleFileReader::ModuleReadResult{
                              std::string(module_name), path, top_path, true});
        }
      }
    } catch (const std_filesystem::filesystem_error& ex) {
      return status::InternalErrorBuilder()
             << "Filesystem error while checking to load module: "
             << module_name << " under path: " << path.native()
             << ". Error: " << ex.what();
    }
    search_paths.emplace_back(path.native());
  }
  return status::NotFoundErrorBuilder()
         << "Cannot find any file to import module: " << module_name
         << ". Searched paths: " << absl::StrJoin(search_paths, ", ");
}

absl::Status PathBasedFileReader::AddSearchPath(absl::string_view search_path) {
  ASSIGN_OR_RETURN(auto crt_path, PathFromString(search_path));
  search_paths_.emplace_back(std::move(crt_path));
  return absl::OkStatus();
}

ModuleStore::ModuleStore(std::unique_ptr<ModuleFileReader> reader,
                         Scope* built_in_scope)
    : reader_(std::move(CHECK_NOTNULL(reader))),
      built_in_scope_(built_in_scope),
      top_module_(Module::BuildTopModule(this)) {}

const absl::flat_hash_map<std::string, Module*>& ModuleStore::modules() const {
  return modules_;
}

bool ModuleStore::HasModule(absl::string_view module_name) const {
  return modules_.contains(module_name);
}

absl::optional<Module*> ModuleStore::GetModule(
    absl::string_view module_name) const {
  auto it = modules_.find(module_name);
  if (it != modules_.end()) {
    return it->second;
  }
  return {};
}

void ModuleStore::set_module_code(absl::string_view module_name,
                                  absl::string_view module_code) {
  module_code_.emplace(module_name, module_code);
}

absl::StatusOr<Module*> ModuleStore::ImportModule(
    absl::string_view module_name, std::vector<std::string>* import_chain) {
  std::vector<std::string> local_chain;
  if (!import_chain) {
    import_chain = &local_chain;
  }
  for (const auto& name : *import_chain) {
    if (module_name == name) {
      return status::FailedPreconditionErrorBuilder()
             << "Chain detected in import order, while importing module: "
             << module_name
             << ". Import stack: " << absl::StrJoin(*import_chain, " => ");
    }
  }
  auto existing_module = GetModule(module_name);
  if (existing_module.has_value()) {
    return existing_module.value();
  }
  ModuleFileReader::ModuleReadResult read_result;
  auto it_code = module_code_.find(module_name);
  if (it_code != module_code_.end()) {
    read_result = ModuleFileReader::ModuleReadResult{
        std::string(module_name), std_filesystem::path("preset"),
        std_filesystem::path(module_name), false, std::string(it_code->second)};
  } else {
    ASSIGN_OR_RETURN(read_result, reader_->ReadModule(module_name));
  }
  const std::string filename = read_result.file_name.native();
  const std::string code = read_result.content;
  import_chain->emplace_back(std::string(module_name));
  auto module_result =
      Module::ParseAndImport(std::move(read_result), this, import_chain);
  import_chain->pop_back();
  if (!module_result.ok()) {
    return status::StatusWriter(module_result.status())
           << "Importing module: " << module_name << ParseFileInfo{filename}
           << ParseFileContent{code};
  }
  auto module = std::move(module_result).value();

  modules_.emplace(std::string(module_name), module);
  return module;
}

ModuleFileReader* ModuleStore::reader() const { return reader_.get(); }

Scope* ModuleStore::built_in_scope() const { return built_in_scope_; }

Module* ModuleStore::top_module() const { return top_module_.get(); }

absl::Duration Module::parse_duration() const { return parse_duration_; }

absl::Duration Module::analysis_duration() const { return analysis_duration_; }

namespace {
// TODO(catalin): hava an error reporter object here, that we use.
absl::StatusOr<std::unique_ptr<pb::Module>> ParseToProto(
    const ModuleFileReader::ModuleReadResult& read_result) {
  std::vector<grammar::ErrorInfo> errors;
  auto parse_result = grammar::ParseModule(read_result.content, {}, &errors);
  if (!parse_result.ok()) {
    auto parse_status = parse_result.status();
    status::StatusWriter writer(parse_result.status());
    for (const auto& error : errors) {
      writer << error;
    }
    writer << ParseFileInfo{read_result.file_name.native()}
           << ParseFileContent{read_result.content};
    return writer;
  }
  return parse_result;
}
}  // namespace

absl::StatusOr<Module*> Module::ParseAndImport(
    const ModuleFileReader::ModuleReadResult& read_result, ModuleStore* store,
    std::vector<std::string>* import_chain) {
  absl::Time start_time = absl::Now();
  ASSIGN_OR_RETURN(auto parse_pb, ParseToProto(read_result));
  absl::Time parse_time = absl::Now();
  ASSIGN_OR_RETURN(auto scope_name, ScopeName::Parse(read_result.module_name));
  auto pscope = std::make_shared<ScopeName>(std::move(scope_name));
  auto module = absl::WrapUnique(new Module(pscope, read_result.module_name,
                                            read_result.file_name, store));
  auto pmodule = module.get();
  pmodule->is_init_module_ = read_result.is_init_module;
  RETURN_IF_ERROR(store->top_module()->AddSubScope(std::move(module)))
      << "Registering module: " << read_result.module_name;
  RETURN_IF_ERROR(pmodule->Import(*parse_pb, import_chain));
  absl::Time analysis_time = absl::Now();
  pmodule->parse_duration_ = parse_time - start_time;
  pmodule->analysis_duration_ = analysis_time - parse_time;
  return pmodule;
}

absl::StatusOr<std::unique_ptr<Module>> Module::ParseBuiltin(
    std_filesystem::path file_path, const pb::Module& pb_module) {
  auto module = absl::WrapUnique(new Module(file_path));
  RETURN_IF_ERROR(module->Import(pb_module, nullptr));
  return {std::move(module)};
}

std::unique_ptr<Module> Module::BuildTopModule(ModuleStore* module_store) {
  return absl::WrapUnique(new Module(module_store));
}

Module::Module(std_filesystem::path file_path)
    : Scope(),
      file_path_(std::move(file_path)),
      module_name_(kBuildtinModuleName),
      module_type_(
          std::make_unique<TypeModule>(type_store_, kBuildtinModuleName, this)),
      pragma_handler_(this) {
  module_type_->set_definition_scope(this);
  type_store_->AddRegistrationCallback(
      *scope_name_, absl::bind_front(&Module::RegisterTypeCallback, this));
}

Module::Module(ModuleStore* module_store)
    : Scope(module_store->built_in_scope()),
      module_store_(module_store),
      module_type_(std::make_unique<TypeModule>(type_store_, "__top__", this)),
      pragma_handler_(this) {
  module_type_->set_definition_scope(this);
  type_store_->AddRegistrationCallback(
      *scope_name_, absl::bind_front(&Module::RegisterTypeCallback, this));
}

Module::Module(std::shared_ptr<ScopeName> scope_name, absl::string_view name,
               std_filesystem::path file_path, ModuleStore* module_store)
    : Scope(std::move(scope_name), module_store->top_module(), true),
      file_path_(std::move(file_path)),
      module_name_(name),
      module_store_(module_store),
      module_type_(std::make_unique<TypeModule>(type_store_, name, this)),
      pragma_handler_(this) {
  module_type_->set_definition_scope(this);
  type_store_->AddRegistrationCallback(
      *scope_name_, absl::bind_front(&Module::RegisterTypeCallback, this));
}

Module::~Module() { type_store_->RemoveRegistrationCallback(*scope_name_); }

const std_filesystem::path& Module::file_path() const { return file_path_; }

pb::ObjectKind Module::kind() const { return pb::ObjectKind::OBJ_MODULE; }

const TypeSpec* Module::type_spec() const { return module_type_.get(); }

const std::string& Module::module_name() const { return module_name_; }

PragmaHandler* Module::pragma_handler() { return &pragma_handler_; }

absl::optional<Function*> Module::main_function() const {
  return main_function_;
}

absl::Status Module::Import(const pb::Module& module,
                            std::vector<std::string>* import_chain) {
  absl::Status status;
  auto my_store = type_store_->FindStore(scope_name().name());
  if (my_store.has_value()) {
    TypeDataset::PushRegistrationStore(my_store.value());
  }
  absl::Cleanup pop_store = [&my_store]() {
    if (my_store.has_value()) {
      TypeDataset::PopRegistrationStore();
    }
  };
  for (const auto& element : module.element()) {
    CodeContext context = CodeContext::FromProto(element);
    if (element.has_import_stmt()) {
      if (!import_chain) {
        return absl::InvalidArgumentError(
            "Cannot process import in builtin module");
      }
      MergeErrorStatus(
          ProcessImport(element.import_stmt(), context, import_chain), status);
    } else if (element.has_schema()) {
      MergeErrorStatus(ProcessSchema(element.schema(), context), status);
    } else if (element.has_function_def()) {
      MergeErrorStatus(ProcessFunctionDef(element.function_def(), context),
                       status);
    } else if (element.has_assignment()) {
      MergeErrorStatus(ProcessAssignment(element.assignment(), context),
                       status);
    } else if (element.has_pragma_expr()) {
      MergeErrorStatus(ProcessPragma(element.pragma_expr(), context), status);
    } else if (element.has_type_def()) {
      MergeErrorStatus(ProcessTypeDef(element.type_def(), context), status);
    }
  }
  return status;
}

absl::Status Module::ProcessImport(const pb::ImportStatement& element,
                                   const CodeContext& context,
                                   std::vector<std::string>* import_chain) {
  for (const auto& spec : element.spec()) {
    ASSIGN_OR_RETURN(auto module_name,
                     NameUtil::GetFullModuleName(spec.module()),
                     _ << context.ToErrorInfo("Error in import statement"));
    std::string local_name(module_name);
    if (spec.has_alias()) {
      ASSIGN_OR_RETURN(
          local_name, NameUtil::ValidatedName(spec.alias()),
          _ << context.ToErrorInfo("Bad alias for import statement"));
    }
    ASSIGN_OR_RETURN(
        auto module, module_store_->ImportModule(module_name, import_chain),
        _ << "Importing: " << spec.DebugString() << ": " << module_name
          << context.ToErrorInfo("Error importing module"));
    RETURN_IF_ERROR(AddChildStore(local_name, module))
        << "For module: " << module_name
        << context.ToErrorInfo("Registering imported module");
    if (spec.has_alias()) {
      RETURN_IF_ERROR(type_store()->AddAlias(
          module->scope_name(), scope_name().Submodule(local_name).value()));
    }
    expressions_.emplace_back(std::make_unique<ImportStatementExpression>(
        this, local_name, spec.has_alias(), module));
  }
  return absl::OkStatus();
}

absl::Status Module::RegisterTypeCallback(TypeSpec* type_spec) {
  if (!TypeUtils::IsStructType(*type_spec)) {
    return absl::OkStatus();
  }
  CodeContext context;
  RETURN_IF_ERROR(RegisterStructureConstructors(
      static_cast<TypeStruct*>(type_spec), context))
      << " Registering automatic structure constructors for: "
      << type_spec->full_name() << " in module: " << scope_name().name();
  return absl::OkStatus();
}

absl::Status Module::RegisterStructureConstructors(TypeStruct* type_spec,
                                                   const CodeContext& context) {
  if (registered_struct_types_.contains(type_spec)) {
    return absl::OkStatus();
  }

  registered_struct_types_.insert(type_spec);
  type_spec->set_definition_scope(this);
  std::string name = type_spec->local_name().empty() ? type_spec->name()
                                                     : type_spec->local_name();
  pb::FunctionDefinition object_constructor;
  object_constructor.set_name(absl::StrCat("_init_object_", name));
  object_constructor.set_fun_type(pb::FunctionType::FUN_CONSTRUCTOR);
  RET_CHECK(type_spec->parameters().size() == type_spec->fields().size());
  for (size_t i = 0; i < type_spec->parameters().size(); ++i) {
    const auto param = type_spec->parameters()[i];
    const auto& field = type_spec->fields()[i];
    auto fparam = object_constructor.add_param();
    fparam->set_name(field.name);
    *fparam->mutable_type_spec() =
        field.type_spec->ToTypeSpecProto(scope_name());
    ASSIGN_OR_RETURN(
        *fparam->mutable_default_value(),
        param->DefaultValueExpression(scope_name()),
        _ << "Preparing default value for structure field: " << field.name
          << ", while building default constructor for: " << name);
  }
  object_constructor.mutable_result_type()->mutable_identifier()->add_name(
      name);
  auto snippet = object_constructor.add_snippet();
  snippet->set_name(std::string(kStructObjectConstructor));
  snippet->set_body(name);
  RETURN_IF_ERROR(ProcessFunctionDef(object_constructor, context))
      << "Registering structure type default object constructor";

  pb::FunctionDefinition copy_constructor;
  copy_constructor.set_name(absl::StrCat("_init_copy_", name));
  copy_constructor.set_fun_type(pb::FunctionType::FUN_CONSTRUCTOR);
  auto fparam = copy_constructor.add_param();
  fparam->set_name("obj");
  fparam->mutable_type_spec()->mutable_identifier()->add_name(name);
  copy_constructor.mutable_result_type()->mutable_identifier()->add_name(name);
  snippet = copy_constructor.add_snippet();
  snippet->set_name(std::string(kStructCopyConstructor));
  snippet->set_body(name);
  RETURN_IF_ERROR(ProcessFunctionDef(copy_constructor, context))
      << "Registering structure type copy object constructor";
  return absl::OkStatus();
}

TypeStore* Module::registration_store() const {
  auto my_store = type_store_->FindStore(scope_name().name());
  if (my_store.has_value()) {
    return my_store.value();
  }
  return type_store_;
}

absl::Status Module::ProcessSchema(const pb::SchemaDefinition& element,
                                   const CodeContext& context) {
  ASSIGN_OR_RETURN(auto name, NameUtil::ValidatedName(element.name()),
                   _ << context.ToErrorInfo("Invalid structure name"));
  std::vector<TypeStruct::Field> fields;
  for (const auto& field_spec : element.field()) {
    TypeStruct::Field field;
    ASSIGN_OR_RETURN(field.name, NameUtil::ValidatedName(field_spec.name()),
                     _ << context.ToErrorInfo("Invalid field name"));
    ASSIGN_OR_RETURN(field.type_spec, FindType(field_spec.type_spec()),
                     _ << context.ToErrorInfo("Cannot find field type"));
    fields.emplace_back(std::move(field));
  }
  ASSIGN_OR_RETURN(
      TypeStruct * type_spec,
      TypeStruct::AddTypeStruct(scope_name(), type_store_, registration_store(),
                                name, std::move(fields)),
      _ << context.ToErrorInfo("Creating structure type"));
  expressions_.emplace_back(
      std::make_unique<SchemaDefinitionExpression>(this, type_spec));

  RETURN_IF_ERROR(RegisterStructureConstructors(type_spec, context))
      << context.ToErrorInfo("In init constructor auto-definition");

  return absl::OkStatus();
}

absl::Status Module::ProcessFunctionDef(const pb::FunctionDefinition& element,
                                        const CodeContext& context) {
  ASSIGN_OR_RETURN(auto def_function,
                   Function::BuildInScope(this, element, "", context));
  expressions_.emplace_back(
      std::make_unique<FunctionDefinitionExpression>(this, def_function));
  if (Function::IsFunctionMainKind(*def_function)) {
    if (main_function_.has_value()) {
      return status::InvalidArgumentErrorBuilder()
             << "Cannot define multiple main functions in the "
                " same module. Existing: "
             << main_function_.value()->full_name()
             << " Adding: " << def_function->full_name();
    }
    main_function_ = def_function;
  }
  return absl::OkStatus();
}

absl::Status Module::ProcessAssignment(const pb::Assignment& element,
                                       const CodeContext& context) {
  ASSIGN_OR_RETURN(auto expression, BuildAssignment(element, context));
  expressions_.emplace_back(std::move(expression));
  return absl::OkStatus();
}

absl::Status Module::ProcessPragma(const pb::PragmaExpression& element,
                                   const CodeContext& context) {
  ASSIGN_OR_RETURN(auto expression, pragma_handler_.HandlePragma(this, element),
                   _ << context.ToErrorInfo("In pragma expression"));
  expressions_.emplace_back(std::move(expression));
  return absl::OkStatus();
}

absl::Status Module::ProcessTypeDef(const pb::TypeDefinition& element,
                                    const CodeContext& context) {
  ASSIGN_OR_RETURN(
      auto type_name, NameUtil::ValidatedName(element.name()),
      _ << "Invalid type name" << context.ToErrorInfo("In type definition"));
  ASSIGN_OR_RETURN(auto type_spec,
                   type_store_->FindType(scope_name(), element.type_spec()),
                   _ << "Processing type expression"
                     << context.ToErrorInfo("In type definition"));
  auto new_type = type_spec->Clone();
  new_type->set_definition_scope(this);
  ASSIGN_OR_RETURN(auto declared_type,
                   registration_store()->DeclareType(scope_name(), type_name,
                                                     std::move(new_type)),
                   _ << "Declaring type: " << type_spec->full_name() << " as "
                     << type_name << " in " << full_name()
                     << context.ToErrorInfo("In type definition"));
  auto expression = std::make_unique<TypeDefinitionExpression>(this, type_name,
                                                               declared_type);
  ASSIGN_OR_RETURN(auto negotiated_type, expression->type_spec(),
                   _ << "Negotiating type definition spec" << kBugNotice);
  RET_CHECK(negotiated_type == declared_type);
  expressions_.emplace_back(std::move(expression));
  return absl::OkStatus();
}

absl::StatusOr<VarBase*> Module::ValidateAssignment(const ScopedName& name,
                                                    NamedObject* object) const {
  ASSIGN_OR_RETURN(auto var_base, Scope::ValidateAssignment(name, object));
  // we just checked above that this is a VarBase:
  if (!name.scope_name().empty()) {
    if (object->kind() != pb::ObjectKind::OBJ_PARAMETER) {
      return status::UnimplementedErrorBuilder()
             << "Only parameters can be set for external scopes. "
                "Found: "
             << object->full_name();
    }
  }
  return var_base;
}

void Module::set_module_store(ModuleStore* module_store) {
  CHECK(module_store_ == nullptr)
      << "Use set_module_store just once on a built-in module.";
  module_store_ = CHECK_NOTNULL(module_store);
}

bool Module::is_init_module() const { return is_init_module_; }

std::string Module::DebugString() const {
  std::vector<std::string> body;
  body.reserve(expressions_.size());
  for (const auto& expr : expressions_) {
    body.emplace_back(expr->DebugString());
  }
  return absl::StrCat("// Module: ", name(), "\n", absl::StrJoin(body, "\n"),
                      "\n", Scope::DebugString());
}

pb::ModuleSpec Module::ToProto() const {
  pb::ModuleSpec proto;
  proto.set_name(module_name_);
  for (const auto& expression : expressions_) {
    *proto.add_expression() = expression->ToProto();
  }
  return proto;
}

absl::StatusOr<std::unique_ptr<Environment>> Environment::Build(
    absl::string_view main_builtin_path,
    std::vector<std::string> search_paths) {
  ASSIGN_OR_RETURN(auto file_path, PathFromString(main_builtin_path));
  ASSIGN_OR_RETURN(auto reader,
                   PathBasedFileReader::Build(std::move(search_paths)));
  ASSIGN_OR_RETURN(
      auto read_result,
      reader.ReadFile(file_path, ModuleFileReader::ModuleReadResult{
                                     "", file_path, file_path, false, ""}));
  absl::Time start_time = absl::Now();
  ASSIGN_OR_RETURN(auto module_pb, ParseToProto(read_result));
  absl::Time parse_time = absl::Now();
  ASSIGN_OR_RETURN(auto builtin_module,
                   Module::ParseBuiltin(file_path, *module_pb));
  builtin_module->analysis_duration_ = absl::Now() - parse_time;
  builtin_module->parse_duration_ = parse_time - start_time;
  auto module_store = std::make_unique<ModuleStore>(
      std::make_unique<PathBasedFileReader>(reader), builtin_module.get());
  builtin_module->set_module_store(module_store.get());
  return std::make_unique<Environment>(std::move(builtin_module),
                                       std::move(module_store));
}

Environment::Environment(std::unique_ptr<Module> builtin_module,
                         std::unique_ptr<ModuleStore> module_store)
    : builtin_module_(std::move(builtin_module)),
      module_store_(std::move(module_store)) {}

Module* Environment::builtin_module() const { return builtin_module_.get(); }

ModuleStore* Environment::module_store() const { return module_store_.get(); }

}  // namespace analysis
}  // namespace nudl
