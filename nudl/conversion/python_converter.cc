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

#include "nudl/conversion/python_converter.h"

#include <algorithm>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "nudl/status/status.h"
#include "nudl/testing/stacktrace.h"
#include "re2/re2.h"

namespace nudl {
namespace conversion {

std::string PythonFileName(analysis::Module* module,
                           absl::string_view termination = ".py") {
  if (module->built_in_scope() == module) {
    return absl::StrCat("nudl_builtins", termination);
  } else if (module->is_init_module()) {
    return absl::StrCat(
        analysis::ModuleFileReader::ModuleNameToPath(
            conversion::PythonSafeName(module->module_name(), module)),
        "__init__", termination);
  } else {
    return absl::StrCat(
        analysis::ModuleFileReader::ModuleNameToPath(
            conversion::PythonSafeName(module->module_name(), module)),
        termination);
  }
}

class PythonConvertState : public ConvertState {
 public:
  explicit PythonConvertState(analysis::Module* module,
                              bool should_inline = false,
                              size_t indent_delta = 2);
  explicit PythonConvertState(PythonConvertState* superstate,
                              bool should_inline = false);
  ~PythonConvertState() override;

  // The buffer to which we output the code content.
  std::stringstream& out();
  std::string out_str() const;

  // If this is a sub-state for code generation (that
  // would be appended later to this superstate).
  PythonConvertState* superstate() const;
  // The top of the state tree. e.g. in a function that
  // define another function etc.
  PythonConvertState* top_superstate() const;
  // Current indentation.
  const std::string& indent() const;
  // Advances the indentation.
  void inc_indent(size_t count = 1);
  // Reduces the indentation.
  void dec_indent(size_t count = 1);
  // Returns the recorded import statements:
  const absl::flat_hash_set<std::string>& imports() const;

  // Adds the code and imports from state into this one.
  absl::Status AddState(const PythonConvertState& state);
  // Add just the imports from state into this one.
  void AddImports(const PythonConvertState& state);

  // Records that this function was processed.
  bool RegisterFunction(analysis::Function* fun);
  // Records that this function group was processed.
  bool RegisterFunctionGroup(analysis::FunctionGroup* group);
  // Records that this structure was processed.
  bool RegisterStructType(const analysis::TypeStruct* ts);

  // Used to mark the current function call in order
  // to use the proper names in identifier & dot expressiosn.
  analysis::Function* in_function_call() const;
  void push_in_function_call(analysis::Function* fun);
  void pop_in_function_call();

  // If the content of this state can be inlined.
  bool is_inline() const;
  void clear_inline();

  // If we expect that anything in this state should be inlined.
  bool should_inline() const;

  // Adds an import statement to the list of imports.
  // We use only individual imports in form `import <full_name> [as <name>]`
  void add_import(absl::string_view import_stmt);

  // Checks that the expression is inline:
  absl::Status CheckInline(const analysis::Expression& expression) const;

  // On stack helper for maintaining the function call stack.
  class FunctionPusher {
   public:
    FunctionPusher(PythonConvertState* state, analysis::Function* fun)
        : state_(CHECK_NOTNULL(state)) {
      state_->push_in_function_call(CHECK_NOTNULL(fun));
    }
    ~FunctionPusher() { state_->pop_in_function_call(); }

   private:
    PythonConvertState* const state_;
  };
  absl::optional<std::string> main_module_content() const;
  void set_main_module_content(std::string content);

 protected:
  PythonConvertState* const superstate_ = nullptr;
  const bool should_inline_ = false;
  const size_t indent_delta_;
  std::stringstream out_;
  size_t indent_ = 0;
  std::string indent_str_;
  absl::flat_hash_set<analysis::Function*> converted_functions_;
  absl::flat_hash_set<analysis::FunctionGroup*> converted_groups_;
  absl::flat_hash_set<std::string> converted_structs_;
  std::vector<analysis::Function*> in_function_call_;
  absl::flat_hash_set<std::string> imports_;
  absl::optional<std::string> main_module_content_;
  bool is_inline_ = true;
};

PythonConvertState::PythonConvertState(analysis::Module* module,
                                       bool should_inline, size_t indent_delta)
    : ConvertState(module),
      should_inline_(should_inline),
      indent_delta_(indent_delta) {}

PythonConvertState::PythonConvertState(PythonConvertState* superstate,
                                       bool should_inline)
    : ConvertState(superstate->module()),
      superstate_(superstate),
      should_inline_(should_inline),
      indent_delta_(superstate->indent_delta_) {}

PythonConvertState::~PythonConvertState() {}

const std::string& PythonConvertState::indent() const { return indent_str_; }

std::stringstream& PythonConvertState::out() { return out_; }

std::string PythonConvertState::out_str() const { return out_.str(); }

void PythonConvertState::inc_indent(size_t count) {
  indent_ += indent_delta_ * count;
  indent_str_ += std::string(indent_delta_ * count, ' ');
}

void PythonConvertState::dec_indent(size_t count) {
  CHECK_GE(indent_, indent_delta_ * count);
  indent_ -= indent_delta_ * count;
  indent_str_ = std::string(indent_, ' ');
}

bool PythonConvertState::is_inline() const { return is_inline_; }

void PythonConvertState::clear_inline() { is_inline_ = false; }

bool PythonConvertState::should_inline() const { return should_inline_; }

const absl::flat_hash_set<std::string>& PythonConvertState::imports() const {
  return imports_;
}

void PythonConvertState::add_import(absl::string_view import_stmt) {
  // TODO(catalin): add a small RE check on this - just in case..
  imports_.insert(std::string(import_stmt));
}

PythonConvertState* PythonConvertState::superstate() const {
  return superstate_;
}

PythonConvertState* PythonConvertState::top_superstate() const {
  PythonConvertState* s = superstate_;
  while (s) {
    if (!s->superstate()) {
      return s;
    }
    s = s->superstate();
  }
  return nullptr;
}

bool PythonConvertState::RegisterFunction(analysis::Function* fun) {
  return converted_functions_.emplace(fun).second;
}

bool PythonConvertState::RegisterFunctionGroup(analysis::FunctionGroup* group) {
  return converted_groups_.emplace(group).second;
}

bool PythonConvertState::RegisterStructType(const analysis::TypeStruct* ts) {
  return converted_structs_.emplace(ts->name()).second;
}

absl::Status PythonConvertState::CheckInline(
    const analysis::Expression& expression) const {
  if (!is_inline()) {
    return status::InvalidArgumentErrorBuilder()
           << "Expression produces non inline output:\n"
           << out_.str() << "\nFor: " << expression.DebugString();
  }
  return absl::OkStatus();
}

absl::optional<std::string> PythonConvertState::main_module_content() const {
  return main_module_content_;
}

void PythonConvertState::set_main_module_content(std::string content) {
  main_module_content_ = std::move(content);
}

absl::Status PythonConvertState::AddState(const PythonConvertState& state) {
  AddImports(state);
  if (!state.is_inline() && should_inline()) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot add code produced in a non-inline state "
              "to a state that requires inline code. Faulty code: \n"
           << state.out_str();
  }
  out() << state.out_str();
  return absl::OkStatus();
}

// Add just the imports from state into this one.
void PythonConvertState::AddImports(const PythonConvertState& state) {
  imports_.insert(state.imports().begin(), state.imports().end());
}

namespace {
std::string scope_name(const analysis::ScopeName& name,
                       bool with_final_dot = false) {
  if (name.empty()) {
    return "";
  }
  std::vector<std::string> names;
  names.reserve(name.size() + 1);
  std::copy(name.module_names().begin(), name.module_names().end(),
            std::back_inserter(names));
  std::copy(name.function_names().begin(), name.function_names().end(),
            std::back_inserter(names));
  return absl::StrCat(absl::StrJoin(names, "."), (with_final_dot ? "." : ""));
}

std::string scoped_name(const analysis::ScopedName& name) {
  if (name.name().empty()) {
    return scope_name(name.scope_name());
  }
  return absl::StrCat(scope_name(name.scope_name(), true), name.name());
}
}  // namespace

analysis::Function* PythonConvertState::in_function_call() const {
  if (in_function_call_.empty()) {
    if (superstate_) {
      return superstate_->in_function_call();
    }
    return nullptr;
  }
  return in_function_call_.back();
}

void PythonConvertState::push_in_function_call(analysis::Function* fun) {
  in_function_call_.emplace_back(CHECK_NOTNULL(fun));
  CHECK(in_function_call() == fun);
}

void PythonConvertState::pop_in_function_call() {
  CHECK(!in_function_call_.empty());
  in_function_call_.pop_back();
}

PythonConverter::PythonConverter(bool bindings_on_use)
    : Converter(), bindings_on_use_(bindings_on_use) {}

absl::StatusOr<std::unique_ptr<ConvertState>> PythonConverter::BeginModule(
    analysis::Module* module) const {
  return std::make_unique<PythonConvertState>(module);
}

absl::StatusOr<ConversionResult> PythonConverter::FinishModule(
    analysis::Module* module, std::unique_ptr<ConvertState> state) const {
  auto bstate = static_cast<PythonConvertState*>(state.get());
  ConversionResult result;
  if (module->main_function().has_value()) {
    RET_CHECK(bstate->main_module_content().has_value());
    result.files.emplace_back(ConversionResult::ConvertedFile{
        PythonFileName(state->module(), "_main.py"),
        bstate->main_module_content().value()});
  }
  result.files.emplace_back(ConversionResult::ConvertedFile{
      PythonFileName(state->module()), bstate->out_str()});
  return {std::move(result)};
}

absl::Status PythonConverter::ConvertInlineExpression(
    const analysis::Expression& expression, PythonConvertState* state,
    std::optional<std::string*> str) const {
  PythonConvertState expression_state(state, true);
  RETURN_IF_ERROR(ConvertExpression(expression, &expression_state));
  RETURN_IF_ERROR(expression_state.CheckInline(expression));
  if (str.has_value()) {
    *str.value() = expression_state.out_str();
  }
  RETURN_IF_ERROR(state->AddState(expression_state));
  return absl::OkStatus();
}

absl::Status PythonConverter::ProcessModule(analysis::Module* module,
                                            ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  PythonConvertState local_state(module);
  local_state.add_import("import nudl");
  // TODO(catalin) : to see here how we do exactly for name overloading..
  if (module->built_in_scope() != module) {
    local_state.add_import("from nudl_builtins import *");
  }
  for (const auto& expression : module->expressions()) {
    PythonConvertState expression_state(&local_state);
    RETURN_IF_ERROR(ConvertExpression(*expression.get(), &expression_state));
    RETURN_IF_ERROR(local_state.AddState(expression_state));
  }
  if (module->main_function().has_value()) {
    PythonConvertState expression_state(&local_state);
    ASSIGN_OR_RETURN(std::string main_module,
                     ConvertMainFunction(module->main_function().value(),
                                         &expression_state));
    bstate->set_main_module_content(std::move(main_module));
    RETURN_IF_ERROR(local_state.AddState(expression_state));
  }
  std::vector<std::string> imports(local_state.imports().begin(),
                                   local_state.imports().end());
  std::sort(imports.begin(), imports.end());
  bstate->out() << "''' ------- NuDL autogenerated module:" << std::endl
                << "  Module Name: " << module->module_name() << std::endl
                << "  Module File: " << module->file_path().native()
                << std::endl
                << "  Parse Duration: " << module->parse_duration() << std::endl
                << "  Analysis Duration: " << module->analysis_duration()
                << std::endl
                << "-----'''" << std::endl
                << std::endl
                << absl::StrJoin(imports, "\n") << std::endl
                << std::endl
                << local_state.out_str() << std::endl;

  return absl::OkStatus();
}
namespace {
absl::optional<std::pair<std::string, std::string>> PythonTypeName(
    int type_id) {
  static const auto* const kTypeNames =
      new absl::flat_hash_map<int, std::pair<std::string, std::string>>({
          {pb::TypeId::ANY_ID, {"typing.Any", "typing"}},
          {pb::TypeId::NULL_ID, {"None", ""}},
          {pb::TypeId::NUMERIC_ID, {"nudl.Numeric", "nudl"}},
          // TODO(catalin): want to use numpy for these ?
          {pb::TypeId::INT_ID, {"int", ""}},
          {pb::TypeId::INT8_ID, {"int", ""}},
          {pb::TypeId::INT16_ID, {"int", ""}},
          {pb::TypeId::INT32_ID, {"int", ""}},
          {pb::TypeId::UINT_ID, {"int", ""}},
          {pb::TypeId::UINT8_ID, {"int", ""}},
          {pb::TypeId::UINT16_ID, {"int", ""}},
          {pb::TypeId::UINT32_ID, {"int", ""}},
          {pb::TypeId::STRING_ID, {"str", ""}},
          {pb::TypeId::BYTES_ID, {"bytes", ""}},
          {pb::TypeId::BOOL_ID, {"bool", ""}},
          {pb::TypeId::FLOAT32_ID, {"float", ""}},
          {pb::TypeId::FLOAT64_ID, {"float", ""}},
          {pb::TypeId::DATE_ID, {"datetime.date", "datetime"}},
          {pb::TypeId::DATETIME_ID, {"datetime.datetime", "datetime"}},
          {pb::TypeId::TIMEINTERVAL_ID, {"datetime.timedelta", "datetime"}},
          {pb::TypeId::TIMESTAMP_ID, {"float", ""}},
          {pb::TypeId::DECIMAL_ID, {"decimal.Decimal", "decimal"}},
          {pb::TypeId::ITERABLE_ID,
           {"collections.abc.Iterable", "collections.abc"}},
          {pb::TypeId::ARRAY_ID, {"typing.List", "typing"}},
          {pb::TypeId::TUPLE_ID, {"typing.Tuple", "typing"}},
          {pb::TypeId::SET_ID, {"typing.Set", "typing"}},
          {pb::TypeId::MAP_ID, {"typing.Map", "typing"}},
          // {pb::TypeId::STRUCT_ID, std::string(kTypeNameStruct)},
          {pb::TypeId::FUNCTION_ID,
           {"collections.abc.Callable", "collections.abc"}},
          {pb::TypeId::UNION_ID, {"typing.Union", "typing"}},
          {pb::TypeId::NULLABLE_ID, {"typing.Optional", "typing"}},
          {pb::TypeId::DATASET_ID,
           {"nudl.dataset.DatasetStep", "nudl.dataset"}},
          {pb::TypeId::TYPE_ID, {"type", ""}},
          {pb::TypeId::MODULE_ID, {"types.ModuleType", "types"}},
          {pb::TypeId::INTEGRAL_ID, {"int", ""}},
          {pb::TypeId::CONTAINER_ID,
           {"collections.abc.Collection", "collections.abc"}},
          {pb::TypeId::GENERATOR_ID,
           {"collections.abc.Iterable", "collections.abc"}},
      });
  auto it = kTypeNames->find(type_id);
  if (it == kTypeNames->end()) {
    return {};
  }
  return it->second;
}
bool HasType(const analysis::Expression& expression, pb::TypeId type_id) {
  return (expression.stored_type_spec().has_value() &&
          (expression.stored_type_spec().value()->type_id() == type_id));
}

bool IsExternalType(const analysis::TypeSpec* type_spec,
                    PythonConvertState* state) {
  return (!type_spec->scope_name().empty() &&
          type_spec->scope_name().name() != state->module()->name());
}
}  // namespace

std::string PythonConverter::GetStructTypeName(
    const analysis::TypeSpec* type_spec, bool force_name,
    PythonConvertState* state) const {
  std::string module_name;
  std::string prefix;
  if (IsExternalType(type_spec, state)) {
    // TODO(catalin): for now, to prevent circular dependencies, as functions
    // defined in external modules may be binded (in those modules)
    // with types defined in places from where they are called.
    // One more reason to place the bindings in the module they are used !!
    if (!bindings_on_use_ ||
        !force_name
        // for now filter out typedef struct stuff.
        || !type_spec->local_name().empty()) {
      return "typing.Any";
    }
    if (!type_spec->scope_name().module_names().empty()) {
      state->add_import(absl::StrCat(
          "import ", PythonSafeName(type_spec->scope_name().module_name(),
                                    type_spec->definition_scope())));
    }
    prefix = PythonSafeName(scope_name(type_spec->scope_name(), true),
                            type_spec->definition_scope());
  }
  const std::string name = type_spec->local_name().empty()
                               ? type_spec->name()
                               : type_spec->local_name();
  return absl::StrCat(
      prefix, PythonSafeName(name, const_cast<analysis::TypeSpec*>(type_spec)));
}

absl::Status PythonConverter::AddTypeName(const analysis::TypeSpec* type_spec,
                                          bool force_struct_name,
                                          PythonConvertState* state) const {
  if (type_spec->type_id() == pb::TypeId::STRUCT_ID) {
    if (!IsExternalType(type_spec, state)) {
      RETURN_IF_ERROR(ConvertStructType(
          static_cast<const analysis::TypeStruct*>(type_spec), state));
    }
    state->out() << GetStructTypeName(type_spec, true, state);
    return absl::OkStatus();
  }
  auto pytype_spec = PythonTypeName(type_spec->type_id());
  if (!pytype_spec.has_value()) {
    return status::UnimplementedErrorBuilder()
           << "Don't know how to convert: " << type_spec->full_name();
  }
  if (!pytype_spec.value().second.empty()) {
    state->add_import(absl::StrCat("import ", pytype_spec.value().second));
  }
  state->out() << pytype_spec.value().first;
  if (type_spec->parameters().empty() ||
      type_spec->type_id() == pb::TypeId::DATASET_ID
      // We end up with very, very big tuples just skip the args there.
      || type_spec->type_id() == pb::TypeId::TUPLE_ID) {
    return absl::OkStatus();
  }
  state->out() << "[";
  if (type_spec->type_id() == pb::TypeId::FUNCTION_ID) {
    state->out() << "[";
    for (size_t i = 0; i + 1 < type_spec->parameters().size(); ++i) {
      if (i) {
        state->out() << ", ";
      }
      RETURN_IF_ERROR(
          AddTypeName(type_spec->parameters()[i], force_struct_name, state));
    }
    state->out() << "]";
    state->out() << ", ";
    RETURN_IF_ERROR(
        AddTypeName(type_spec->parameters().back(), force_struct_name, state));
  } else if (type_spec->type_id() == pb::TypeId::NULLABLE_ID) {
    RETURN_IF_ERROR(
        AddTypeName(type_spec->parameters().back(), force_struct_name, state));
  } else if (type_spec->type_id() == pb::TypeId::TUPLE_ID &&
             static_cast<const analysis::TypeTuple*>(type_spec)->is_named()) {
    auto tuple_type = static_cast<const analysis::TypeTuple*>(type_spec);
    for (size_t i = 0; i < type_spec->parameters().size(); ++i) {
      if (i) {
        state->out() << ", ";
      }
      const bool is_named = !tuple_type->names()[i].empty();
      if (is_named) {
        state->out() << "typing.Tuple[str, ";
      }
      RETURN_IF_ERROR(
          AddTypeName(type_spec->parameters()[i], force_struct_name, state));
      if (is_named) {
        state->out() << "]";
      }
    }
  } else {
    for (size_t i = 0; i < type_spec->parameters().size(); ++i) {
      if (i) {
        state->out() << ", ";
      }
      RETURN_IF_ERROR(
          AddTypeName(type_spec->parameters()[i], force_struct_name, state));
    }
  }
  state->out() << "]";
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertAssignment(
    const analysis::Assignment& expression, ConvertState* state) const {
  // TODO(catalin): See the param setting - needs `global` setting or
  // setter function automatically defined.
  RET_CHECK(!expression.children().empty());
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->out() << PythonSafeName(scoped_name(expression.name()),
                                  expression.named_object());
  if (expression.has_type_spec()) {
    bstate->out() << " : ";
    RETURN_IF_ERROR(
        AddTypeName(expression.var()->converted_type(), false, bstate));
  }
  bstate->out() << " = ";
  RETURN_IF_ERROR(
      ConvertInlineExpression(*expression.children().front(), bstate))
      << "In assignment";
  bstate->out() << std::endl;
  bstate->clear_inline();
  return absl::OkStatus();
}
absl::Status PythonConverter::ConvertEmptyStruct(
    const analysis::EmptyStruct& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  if (HasType(expression, pb::TypeId::SET_ID)) {
    bstate->out() << "set()";
  } else if (HasType(expression, pb::TypeId::MAP_ID)) {
    bstate->out() << "{}";
  } else {
    bstate->out() << "[]";
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertLiteral(
    const analysis::Literal& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  switch (expression.build_type_spec()->type_id()) {
    case pb::TypeId::NULL_ID:
      bstate->out() << "None";
      break;
    case pb::TypeId::INT_ID:
      bstate->out() << absl::StrCat(std::any_cast<int64_t>(expression.value()));
      break;
    case pb::TypeId::UINT_ID:
      bstate->out() << absl::StrCat(
          std::any_cast<uint64_t>(expression.value()));
      break;
    case pb::TypeId::STRING_ID:
      // TODO(catalin): figure out if this makes sense w/ unicode / UTF8 and
      // such.
      bstate->out() << absl::StrCat(
          "\"",
          absl::Utf8SafeCEscape(std::any_cast<std::string>(expression.value())),
          "\"");
      break;
    case pb::TypeId::BYTES_ID:
      bstate->out() << absl::StrCat(
          "b\"",
          absl::CHexEscape(std::any_cast<std::string>(expression.value())),
          "\"");
      break;
    case pb::TypeId::BOOL_ID:
      if (std::any_cast<bool>(expression.value())) {
        bstate->out() << "True";
      } else {
        bstate->out() << "False";
      }
      break;
    case pb::TypeId::FLOAT32_ID:
      bstate->out() << absl::StrCat(std::any_cast<float>(expression.value()));
      break;
    case pb::TypeId::FLOAT64_ID:
      bstate->out() << absl::StrCat(std::any_cast<double>(expression.value()));
      break;
    case pb::TypeId::TIMEINTERVAL_ID:
      bstate->out() << "datetime.timedelta(seconds="
                    << absl::ToInt64Seconds(
                           std::any_cast<absl::Duration>(expression.value()))
                    << ")";
      bstate->add_import("import datetime");
      break;
    default:
      return status::InvalidArgumentErrorBuilder()
             << "Invalid type id for literal: "
             << expression.build_type_spec()->full_name()
             << analysis::kBugNotice;
  }
  return absl::OkStatus();
}

namespace {
absl::optional<analysis::Function*> GetFunctionBinding(
    const analysis::Expression& expression, PythonConvertState* state) {
  analysis::Function* check_fun = state->in_function_call();
  if (!check_fun) {
    return {};
  }
  auto named_obj = expression.named_object();
  if (!named_obj.has_value()) {
    return {};
  }
  if (analysis::Function::IsFunctionKind(*named_obj.value())) {
    auto fun = static_cast<const analysis::Function*>(named_obj.value());
    if (fun->IsBinding(check_fun)) {
      return check_fun;
    }
  } else if (analysis::FunctionGroup::IsFunctionGroup(*named_obj.value())) {
    auto group = static_cast<const analysis::FunctionGroup*>(named_obj.value());
    if (group->FindBinding(check_fun)) {
      return check_fun;
    }
  }
  return {};
}
}  // namespace

absl::Status PythonConverter::ConvertIdentifier(
    const analysis::Identifier& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  auto binding = GetFunctionBinding(expression, bstate);
  if (binding.has_value()) {
    if (bindings_on_use_) {
      ASSIGN_OR_RETURN(auto name,
                       LocalFunctionName(binding.value(), true, state));
      bstate->out() << name;
    } else if (binding.value()->is_abstract()) {
      // TODO(catalin): function groups
      auto group = binding.value()->function_group();
      analysis::ScopedName local_name(expression.scoped_name().scope_name_ptr(),
                                      group->call_name());
      bstate->out() << PythonSafeName(scoped_name(local_name), group);
    } else {
      analysis::ScopedName local_name(expression.scoped_name().scope_name_ptr(),
                                      binding.value()->call_name());
      bstate->out() << PythonSafeName(scoped_name(local_name), binding.value());
    }
  } else {
    auto object = expression.named_object();
    if (!object.has_value() ||
        !analysis::Function::IsFunctionKind(*object.value())) {
      std::string object_prefix = scoped_name(expression.scoped_name());
      // This takes care of the case in which an external function that
      // uses an external top level variable or related object is used in
      // the locally bound function.
      if (bindings_on_use_ && object.has_value() &&
          analysis::VarBase::IsVarKind(*object.value())) {
        auto obj =
            static_cast<analysis::VarBase*>(object.value())->GetRootVar();
        if (obj->parent_store().has_value()) {
          auto parent_store = obj->parent_store().value();
          if (parent_store->kind() == pb::ObjectKind::OBJ_MODULE &&
              parent_store != state->module()) {
            auto parent_module = static_cast<analysis::Module*>(parent_store);
            std::string external_prefix =
                absl::StrCat(parent_module->scope_name().name(), ".");
            if (!absl::StartsWith(object_prefix, external_prefix)) {
              object_prefix = absl::StrCat(external_prefix, object_prefix);
            }
          }
        }
      }
      bstate->out() << PythonSafeName(object_prefix, object);
    } else {
      analysis::ScopedName fun_scoped_name(
          expression.scoped_name().scope_name_ptr(),
          static_cast<const analysis::Function*>(object.value())->call_name());
      bstate->out() << PythonSafeName(scoped_name(fun_scoped_name),
                                      expression.named_object());
    }
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertFunctionResult(
    const analysis::FunctionResultExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  switch (expression.result_kind()) {
    case pb::FunctionResultKind::RESULT_NONE:
      return absl::InvalidArgumentError(
          "Should not end up with a NONE result kind in a function result"
          "expression");
    case pb::FunctionResultKind::RESULT_RETURN: {
      bstate->out() << "return ";
      RET_CHECK(!expression.children().empty());
      RETURN_IF_ERROR(
          ConvertInlineExpression(*expression.children().front(), bstate))
          << "In `return`";
      break;
    }
    case pb::FunctionResultKind::RESULT_YIELD: {
      bstate->out() << "yield ";
      RET_CHECK(!expression.children().empty());
      RETURN_IF_ERROR(
          ConvertInlineExpression(*expression.children().front(), bstate))
          << "In `yield`";
      break;
    }
    case pb::FunctionResultKind::RESULT_PASS:
      bstate->out() << "return";
      break;
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertArrayDefinition(
    const analysis::ArrayDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  const bool is_set = HasType(expression, pb::TypeId::SET_ID);
  const bool is_tuple = !is_set && HasType(expression, pb::TypeId::TUPLE_ID);
  if (is_set) {
    bstate->out() << "{" << std::endl;
  } else if (is_tuple) {
    bstate->out() << "(" << std::endl;
  } else {
    bstate->out() << "[" << std::endl;
  }
  bstate->inc_indent(2);
  for (size_t i = 0; i < expression.children().size(); ++i) {
    const auto& expr = expression.children()[i];
    if (i) {
      bstate->out() << "," << std::endl;
    }
    bstate->out() << bstate->indent();
    RETURN_IF_ERROR(ConvertInlineExpression(*expr, bstate))
        << "In array def: " << i;
  }
  bstate->out() << (is_set ? "}" : (is_tuple ? ",)" : "]"));
  bstate->dec_indent(2);
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertMapDefinition(
    const analysis::MapDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->out() << "{" << std::endl;
  bool is_first = true;
  bool last_key = false;
  bstate->inc_indent(2);
  for (const auto& expr : expression.children()) {
    if (last_key) {
      bstate->out() << ": ";
      last_key = false;
    } else {
      if (is_first) {
        is_first = false;
      } else {
        bstate->out() << ", " << std::endl << bstate->indent();
      }
      last_key = true;
    }
    RETURN_IF_ERROR(ConvertInlineExpression(*expr, bstate))
        << "In map def " << (last_key ? "key" : "value");
  }
  bstate->out() << "}";
  bstate->dec_indent(2);
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertTupleDefinition(
    const analysis::TupleDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->out() << "(";
  expression.CheckSizes();
  bstate->inc_indent(2);
  for (size_t i = 0; i < expression.names().size(); ++i) {
    // Names are checked to be valid identifiers - no need to escape
    bstate->out() << bstate->indent() << "(\"" << expression.names()[i]
                  << "\", ";
    RETURN_IF_ERROR(ConvertInlineExpression(*expression.children()[i], bstate))
        << "In tuple element definition";
    bstate->out() << "),\n";
  }
  bstate->dec_indent();
  bstate->out() << ")";
  bstate->dec_indent();
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertIfExpression(
    const analysis::IfExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->clear_inline();
  RET_CHECK(
      expression.condition().size() == expression.expression().size() ||
      (expression.condition().size() + 1 == expression.expression().size()));
  for (size_t i = 0; i < expression.condition().size(); ++i) {
    if (i == 0) {
      bstate->out() << "if ";
    } else {
      bstate->out() << bstate->indent();
      bstate->out() << "elif ";
    }
    RETURN_IF_ERROR(ConvertInlineExpression(*expression.condition()[i], bstate))
        << "In `if` condition " << i;
    bstate->out() << ":" << std::endl;
    RETURN_IF_ERROR(ConvertExpression(*expression.expression()[i], bstate));
  }
  if (expression.expression().size() > expression.condition().size()) {
    bstate->out() << bstate->indent() << "else:" << std::endl;
    RETURN_IF_ERROR(ConvertExpression(*expression.expression().back(), state));
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertExpressionBlock(
    const analysis::ExpressionBlock& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  if (expression.children().size() > 1) {
    bstate->clear_inline();
  }
  bstate->inc_indent();
  for (const auto& expr : expression.children()) {
    if (!bstate->should_inline()) {
      bstate->out() << bstate->indent();
    }
    if (expr->is_default_return() && !bstate->should_inline()) {
      bstate->clear_inline();
      if (expr->expr_kind() == pb::ExpressionKind::EXPR_ASSIGNMENT) {
        // Special case - assignment is not inline, so process assignment, then
        // return the assigned identifier:
        RETURN_IF_ERROR(ConvertExpression(*expr, state));
        auto assignment = static_cast<const analysis::Assignment*>(expr.get());
        bstate->out() << bstate->indent() << "return "
                      << PythonSafeName(scoped_name(assignment->name()),
                                        assignment->named_object());
      } else {
        bstate->out() << "return ";
        RETURN_IF_ERROR(ConvertInlineExpression(*expr, bstate))
            << "For the implicit return expression in function";
      }
    } else {
      RETURN_IF_ERROR(ConvertExpression(*expr, state));
    }
    if (!bstate->should_inline()) {
      bstate->out() << std::endl;
    }
  }
  bstate->dec_indent();
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertIndexExpression(
    const analysis::IndexExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  RET_CHECK(expression.children().size() == 2);
  RETURN_IF_ERROR(ConvertExpression(*expression.children().front(), state));
  bstate->out() << "[";
  RETURN_IF_ERROR(
      ConvertInlineExpression(*expression.children().back(), bstate))
      << "In index expression";
  bstate->out() << "]";
  return absl::OkStatus();
}
absl::Status PythonConverter::ConvertTupleIndexExpression(
    const analysis::TupleIndexExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  RETURN_IF_ERROR(ConvertIndexExpression(expression, state));
  auto type_spec = expression.children().front()->stored_type_spec();
  if (type_spec.has_value() &&
      analysis::TypeUtils::IsNamedTupleType(*type_spec.value())) {
    bstate->out() << "[1]";
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertLambdaExpression(
    const analysis::LambdaExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  analysis::Function* const lambda_fun = expression.lambda_function();
  ASSIGN_OR_RETURN(
      bool converted,
      ConvertFunction(
          !bindings_on_use_ && lambda_fun->binding_parent().has_value()
              ? lambda_fun->binding_parent().value()
              : lambda_fun,
          false, bstate));
  if (expression.lambda_group()) {
    RETURN_IF_ERROR(ConvertFunctionGroup(expression.lambda_group(), bstate));
  }
  RET_CHECK(expression.named_object().has_value());
  analysis::NamedObject* const obj = expression.named_object().value();
  RET_CHECK(analysis::Function::IsFunctionKind(*obj));
  analysis::Function* fun = static_cast<analysis::Function*>(obj);
  if (bindings_on_use_ && !converted) {
    ASSIGN_OR_RETURN(converted, ConvertFunction(fun, true, bstate));
    LOG(INFO) << " ---- Converted lambda: " << fun->full_name();
  }
  if (!converted) {
    return status::InternalErrorBuilder()
           << "Cannot convert unbound / missing expression function: "
           << expression.lambda_function()->full_name();
  }
  const std::string group_name =
      PythonSafeName(fun->function_group()->call_name(), fun->function_group());
  if (fun->is_abstract()) {
    RET_CHECK(!bindings_on_use_) << "For: " << fun->full_name();
    // This means the function is used in a x = .. or similar situation
    bstate->out() << group_name;
    return absl::OkStatus();
  }
  if (!fun->first_default_value_index().has_value()) {
    if (bindings_on_use_) {
      ASSIGN_OR_RETURN(auto name, LocalFunctionName(fun, true, state));
      bstate->out() << name;
    } else {
      bstate->out() << group_name << "." << fun->type_signature();
    }
    return absl::OkStatus();
  }
  // Else we create a local lambda to capture any local parameters.
  // TODO(catalin): maybe we need to just use the above if no local
  //  parameters are provided as default values.
  bstate->out() << "lambda ";
  RET_CHECK(fun->arguments().size() == fun->default_values().size())
      << "For: " << fun->full_name();
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      bstate->out() << ", ";
    }
    const auto& arg = fun->arguments()[i];
    const auto& value = fun->default_values()[i];
    bstate->out() << PythonSafeName(arg->name(), arg.get());
    if (value.has_value()) {
      bstate->out() << " = ";
      RETURN_IF_ERROR(ConvertInlineExpression(*value.value(), bstate))
          << "For default expression in lambda: " << i;
    }
  }
  // TODO(catalin):  --- test the waters here - whats the binding here ???
  // is type_signature good enough for all cases ?
  // bstate->out() << ": " << PythonSafeName(fun->call_name(), fun) << "(";
  bstate->out() << ": ";
  if (bindings_on_use_) {
    ASSIGN_OR_RETURN(auto name, LocalFunctionName(fun, true, state));
    bstate->out() << name;
  } else {
    bstate->out() << group_name << "." << fun->type_signature();
  }
  bstate->out() << "(";
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      bstate->out() << ", ";
    }
    bstate->out() << PythonSafeName(fun->arguments()[i]->name(), fun);
  }
  bstate->out() << ")";
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertDotAccessExpression(
    const analysis::DotAccessExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  RET_CHECK(expression.children().size() == 1);
  RETURN_IF_ERROR(
      ConvertInlineExpression(*expression.children().front(), bstate));
  bstate->out() << ".";
  auto binding = GetFunctionBinding(expression, bstate);
  if (binding.has_value()) {
    if (!bindings_on_use_) {
      ASSIGN_OR_RETURN(auto name,
                       LocalFunctionName(binding.value(), true, state));
      bstate->out() << name;
    } else if (binding.value()->is_abstract()) {
      // TODO(catalin): function groups
      auto group = binding.value()->function_group();
      bstate->out() << PythonSafeName(group->call_name(), group);
    } else {
      bstate->out() << PythonSafeName(binding.value()->call_name(),
                                      binding.value());
    }
  } else {
    bstate->out() << PythonSafeName(expression.name().name(),
                                    expression.named_object());
  }
  return absl::OkStatus();
}

struct NativeConvert {
  NativeConvert(analysis::Function* fun, PythonConvertState* state)
      : fun(fun), state(state) {}

  absl::flat_hash_set<std::string> FindMacros() const {
    re2::StringPiece input(code);
    absl::flat_hash_set<std::string> macros;
    std::string token;
    while (RE2::Consume(&input, R"(.*\${{(\w+)}})", &token)) {
      macros.insert(token);
    }
    return macros;
  }
  void PrepareMacros(
      const absl::flat_hash_map<std::string, std::unique_ptr<ConvertState>>&
          expansions) {
    for (const auto& it : expansions) {
      auto bstate = static_cast<PythonConvertState*>(it.second.get());
      state->AddImports(*bstate);
      arguments.emplace(absl::StrCat("${{", it.first, "}}"), bstate->out_str());
    }
  }

  absl::StatusOr<std::string> Replace() {
    PrepareStructConstructor();
    std::string replaced =
        absl::StrReplaceAll(absl::StripAsciiWhitespace(code), arguments);
    for (const auto& s : skipped) {
      if (absl::StrContains(replaced, s)) {
        return status::InvalidArgumentErrorBuilder()
               << "Argument: " << s
               << " for which we got no value in the call "
                  "of native inline function "
               << fun->call_name() << " remains in result: `" << replaced
               << "`";
      }
    }
    return replaced;
  }
  std::string arg_escaped(absl::string_view arg_name) const {
    return absl::StrCat("${", arg_name, "}");
  }
  void add_skipped(absl::string_view arg_name) {
    skipped.emplace(arg_escaped(arg_name));
  }
  void add_arg(absl::string_view arg_name, std::string value) {
    arguments_ordered.emplace_back(arg_name);
    arguments.emplace(arg_escaped(arg_name), std::move(value));
  }

  absl::Status Prepare() {
    RET_CHECK(fun->is_native());
    if (SetStructConstructor()) {
      return absl::OkStatus();  // will be prepared later
    }
    auto py_inline_it = fun->native_impl().find("pyinline");
    if (py_inline_it == fun->native_impl().end()) {
      return status::InvalidArgumentErrorBuilder()
             << "No native implementation under `pyinline` for function: "
             << fun->name();
    }
    auto py_import_it = fun->native_impl().find("pyimport");
    if (py_import_it != fun->native_impl().end()) {
      state->add_import(absl::StripAsciiWhitespace(py_import_it->second));
    }
    code = py_inline_it->second;
    return absl::OkStatus();
  }

 private:
  enum StructConstructor { None, Copy, Build };
  bool SetStructConstructor() {
    auto it = fun->native_impl().find(analysis::kStructObjectConstructor);
    if (it == fun->native_impl().end()) {
      it = fun->native_impl().find(analysis::kStructCopyConstructor);
      if (it == fun->native_impl().end()) {
        return false;
      }
      struct_construct = StructConstructor::Copy;
    } else {
      struct_construct = StructConstructor::Build;
      constructor_name = it->second;
    }
    return true;
  }
  void PrepareStructConstructor() {
    switch (struct_construct) {
      case StructConstructor::None:
        return;
      case StructConstructor::Copy: {
        const std::string args_str =
            absl::StrJoin(arguments_ordered, ", ",
                          [this](std::string* out, const std::string& s) {
                            absl::StrAppend(out, arg_escaped(s));
                          });
        state->add_import("import copy");
        code = absl::StrCat("copy.deepcopy(", args_str, ")");
        break;
      }
      case StructConstructor::Build: {
        const std::string args_str =
            absl::StrJoin(arguments_ordered, ", ",
                          [this](std::string* out, const std::string& s) {
                            absl::StrAppend(out, s, "=", arg_escaped(s));
                          });
        std::string name;
        if (state->module() != fun->module_scope()) {
          name = PythonSafeName(
              absl::StrCat(scope_name(fun->module_scope()->scope_name(), true),
                           constructor_name),
              fun);
        } else {
          name = PythonSafeName(constructor_name, fun);
        }
        code = absl::StrCat(name, "(", args_str, ")");
        break;
      }
    }
  }

  analysis::Function* const fun;
  PythonConvertState* const state;
  StructConstructor struct_construct = StructConstructor::None;
  std::string constructor_name;
  std::string code;
  absl::flat_hash_map<std::string, std::string> arguments;
  absl::flat_hash_set<std::string> skipped;
  std::vector<std::string> arguments_ordered;
};

namespace {
void ConvertArgumentSubBinding(
    absl::string_view function_name, const analysis::Expression* call_binding,
    analysis::TypeBindingArg type_spec,
    std::optional<analysis::FunctionBinding*> sub_binding,
    PythonConvertState* state) {
  auto obj = call_binding->named_object();
  if (!obj.has_value() ||
      !analysis::FunctionGroup::IsFunctionGroup(*obj.value())) {
    return;
  }
  std::string signature;
  if (sub_binding.has_value()) {
    signature = analysis::TypeSpec::TypeBindingSignature(
        sub_binding.value()->type_arguments);
  } else if (!std::holds_alternative<const analysis::TypeSpec*>(type_spec)) {
    return;
  } else {
    // One may think we can use type_spec for signature - however that
    // is wrong - is the type signature of the function we call not of
    // the object we expect - so bummer.
    return;
  }
  state->out() << "." << signature;
}
}  // namespace

absl::Status PythonConverter::ConvertNativeFunctionCallExpression(
    const analysis::FunctionCallExpression& expression, analysis::Function* fun,
    PythonConvertState* state) const {
  NativeConvert convert(fun, state);
  RETURN_IF_ERROR(convert.Prepare());
  ASSIGN_OR_RETURN(
      auto macros,
      ProcessMacros(convert.FindMacros(), expression.scope(),
                    expression.function_binding(), fun, state),
      _ << "Processing macros in function call of: " << fun->full_name());
  convert.PrepareMacros(macros);
  RET_CHECK(expression.function_binding()->call_expressions.size() ==
            expression.function_binding()->names.size());
  for (size_t i = 0; i < expression.function_binding()->names.size(); ++i) {
    absl::string_view arg_name = expression.function_binding()->names[i];
    const auto& expr = expression.function_binding()->call_expressions[i];
    if (!expr.has_value()) {
      convert.add_skipped(arg_name);
      continue;
    }
    PythonConvertState expression_state(state, true);
    RETURN_IF_ERROR(ConvertExpression(*expr.value(), &expression_state));
    RETURN_IF_ERROR(expression_state.CheckInline(expression))
        << "For argument " << i << " : " << arg_name
        << " of inline native function " << fun->call_name();
    state->AddImports(expression_state);
    CHECK_LT(i, expression.function_binding()->call_sub_bindings.size());
    ConvertArgumentSubBinding(
        fun->call_name(), expr.value(),
        expression.function_binding()->type_arguments[i],
        expression.function_binding()->call_sub_bindings[i], &expression_state);
    convert.add_arg(arg_name, expression_state.out_str());
  }
  ASSIGN_OR_RETURN(std::string replaced, convert.Replace());
  state->out() << "(" << replaced << ")";
  return absl::OkStatus();
}

namespace {

template <class F>
std::string FunSafeCallName(F* f, analysis::Module* module) {
  if (module != f->module_scope()) {
    return PythonSafeName(scoped_name(f->qualified_call_name()), f);
  }
  return PythonSafeName(f->call_name(), f);
}

std::pair<std::string, bool> GetCallBindingFunctionName(
    analysis::Function* fun, analysis::Module* module) {
  if (fun->is_abstract()) {
    return std::make_pair(FunSafeCallName(fun->function_group(), module), true);
  }
  return std::make_pair(FunSafeCallName(fun, module), false);
}
}  // namespace

absl::Status PythonConverter::ConvertFunctionCallExpression(
    const analysis::FunctionCallExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  auto binding = expression.function_binding();  // shortcut
  if (bindings_on_use_) {
    if (binding->fun.has_value()) {
      RETURN_IF_ERROR(
          ConvertFunction(binding->fun.value(), true, bstate).status());
    } else if (analysis::TypeUtils::IsFunctionType(*binding->type_spec)) {
      auto fun_type =
          static_cast<const analysis::TypeFunction*>(binding->type_spec);
      for (auto fun : fun_type->function_instances()) {
        RETURN_IF_ERROR(ConvertFunction(fun, true, bstate).status());
      }
    }
  }
  if (binding->fun.has_value() && binding->fun.value()->is_native()) {
    RETURN_IF_ERROR(ConvertNativeFunctionCallExpression(
        expression, binding->fun.value(), bstate));
    return absl::OkStatus();
  }
  bool add_signature = false;
  std::string function_name;
  if (expression.left_expression().has_value() &&
      !expression.is_method_call()) {
    if (binding->fun.has_value()) {
      PythonConvertState::FunctionPusher pusher(bstate, binding->fun.value());
      RETURN_IF_ERROR(ConvertInlineExpression(
          *expression.left_expression().value(), bstate, &function_name));
    } else {
      RETURN_IF_ERROR(ConvertInlineExpression(
          *expression.left_expression().value(), bstate, &function_name));
    }
  } else {
    RET_CHECK(binding->fun.has_value());
    if (bindings_on_use_) {
      ASSIGN_OR_RETURN(function_name,
                       LocalFunctionName(binding->fun.value(), true, state));
    } else {
      // TODO(catalin): function groups
      std::tie(function_name, add_signature) =
          GetCallBindingFunctionName(binding->fun.value(), bstate->module());
    }
    bstate->out() << function_name;
  }
  // TODO(catalin): function group
  if (add_signature) {
    const std::string signature(
        analysis::TypeSpec::TypeBindingSignature(binding->type_arguments));
    bstate->out() << "." << signature;
  }
  const bool is_constructor_call =
      (binding->fun.has_value() &&
       binding->fun.value()->kind() == pb::ObjectKind::OBJ_CONSTRUCTOR);
  // TODO(catalin): this in practice is a bit more complicated, but
  //  we do something simple.
  RET_CHECK(binding->call_expressions.size() == binding->names.size());
  RET_CHECK(binding->call_sub_bindings.size() == binding->names.size());
  bstate->out() << "(\n";
  bstate->inc_indent(2);
  bool has_arguments = false;
  for (size_t i = 0; i < binding->names.size(); ++i) {
    auto expr = binding->call_expressions[i];
    if (!expr.has_value() ||
        (is_constructor_call && binding->is_default_value[i])) {
      continue;
    }
    if (has_arguments) {
      bstate->out() << "," << std::endl;
    }
    has_arguments = true;
    if (binding->fun.has_value()) {
      bstate->out() << bstate->indent() << binding->names[i] << "=";
    }
    // TODO(catalin): note - this may convert the default expressions
    // as well, which may not be valid in this scope - will want to massage
    // this a bit - Note - actually this may be ok, because we would prefix
    // all names w/ proper module names - need to proper test.
    RETURN_IF_ERROR(ConvertInlineExpression(*expr.value(), bstate));
    ConvertArgumentSubBinding(function_name, expr.value(),
                              binding->type_arguments[i],
                              binding->call_sub_bindings[i], bstate);
  }
  bstate->out() << ")";
  bstate->dec_indent(2);
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertImportStatement(
    const analysis::ImportStatementExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->add_import(absl::StrCat(
      "import ",
      PythonSafeName(scope_name(expression.module()->scope_name()),
                     expression.module()),
      (expression.is_alias()
           ? absl::StrCat(" as ", PythonSafeName(expression.local_name(),
                                                 expression.module()))
           : "")));
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertFunctionDefinition(
    const analysis::FunctionDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->clear_inline();
  // Here we need to lookup the module to all functions w/ this name
  // and convert them all.
  //
  // TODO(catalin): To minimize the regeneration of base code, we actually
  //  need to generate the bindings in the modules that use the bindings
  //  on call function conversion !!!
  //
  ASSIGN_OR_RETURN(auto fun_object,
                   state->module()->GetName(
                       expression.def_function()->function_name(), true));
  if (analysis::FunctionGroup::IsFunctionGroup(*fun_object)) {
    auto function_group = static_cast<analysis::FunctionGroup*>(fun_object);
    for (analysis::Function* fun : function_group->functions()) {
      RETURN_IF_ERROR(ConvertFunction(fun, false, state).status());
    }
    RETURN_IF_ERROR(ConvertFunctionGroup(function_group, bstate));
  } else if (analysis::Function::IsFunctionKind(*fun_object)) {
    RETURN_IF_ERROR(
        ConvertFunction(static_cast<analysis::Function*>(fun_object), false,
                        state)
            .status());
  }
  return ConvertFunction(expression.def_function(), false, state).status();
}

std::string PythonConverter::DefaultFieldFactory(
    const analysis::TypeSpec* type_spec, PythonConvertState* state) const {
  static const auto* const kFactory =
      new absl::flat_hash_map<size_t, std::string>({
          {pb::TypeId::ANY_ID, "nudl.default_none"},
          {pb::TypeId::NULL_ID, "nudl.default_none"},
          {pb::TypeId::NUMERIC_ID, "int"},
          {pb::TypeId::INT_ID, "int"},
          {pb::TypeId::INT8_ID, "int"},
          {pb::TypeId::INT16_ID, "int"},
          {pb::TypeId::INT32_ID, "int"},
          {pb::TypeId::UINT_ID, "int"},
          {pb::TypeId::UINT8_ID, "int"},
          {pb::TypeId::UINT16_ID, "int"},
          {pb::TypeId::UINT32_ID, "int"},
          {pb::TypeId::STRING_ID, "str"},
          {pb::TypeId::BYTES_ID, "bytes"},
          {pb::TypeId::BOOL_ID, "bool"},
          {pb::TypeId::FLOAT32_ID, "float"},
          {pb::TypeId::FLOAT64_ID, "float"},
          {pb::TypeId::DATE_ID, "nudl.default_date"},
          {pb::TypeId::DATETIME_ID, "nudl.default_datetime"},
          {pb::TypeId::TIMEINTERVAL_ID, "nudl.default_timeinterval"},
          {pb::TypeId::TIMESTAMP_ID, "nudl.default_timestamp"},
          {pb::TypeId::DECIMAL_ID, "nudl.default_decimal"},
          {pb::TypeId::ITERABLE_ID, "list"},
          {pb::TypeId::ARRAY_ID, "list"},
          {pb::TypeId::TUPLE_ID, "tuple"},
          {pb::TypeId::SET_ID, "set"},
          {pb::TypeId::MAP_ID, "dict"},
          {pb::TypeId::FUNCTION_ID, "nudl.default_function"},
          {pb::TypeId::NULLABLE_ID, "nudl.default_none"},
          {pb::TypeId::DATASET_ID, "nudl.default_none"},
          {pb::TypeId::TYPE_ID, "nudl.default_none"},
          {pb::TypeId::MODULE_ID, "nudl.default_none"},
          {pb::TypeId::INTEGRAL_ID, "int"},
          {pb::TypeId::CONTAINER_ID, "list"},
          {pb::TypeId::GENERATOR_ID, "list"},
      });
  const size_t type_id = type_spec->type_id();
  if (type_id == pb::TypeId::STRUCT_ID) {
    std::string name = GetStructTypeName(type_spec, true, state);
    if (name == "typing.Any") {
      return "nudl.default_none";
    }
    return name;
  } else if (type_id == pb::TypeId::UNION_ID) {
    if (type_spec->parameters().empty()) {
      return "nudl.default_none";
    }
    return DefaultFieldFactory(type_spec->parameters().front(), state);
  }
  auto it = kFactory->find(type_id);
  if (it != kFactory->end()) {
    return it->second;
  }
  return "nudl.default_none";
}

absl::Status PythonConverter::ConvertStructType(
    const analysis::TypeStruct* ts, PythonConvertState* state) const {
  auto superstate = state->top_superstate();
  if (!superstate->RegisterStructType(ts)) {
    return absl::OkStatus();
  }
  PythonConvertState local_state(superstate);
  auto& out = local_state.out();
  local_state.add_import("import dataclasses");
  const std::string name =
      ts->local_name().empty() ? ts->name() : ts->local_name();
  out << std::endl
      << "@dataclasses.dataclass" << std::endl
      << "class " << PythonSafeName(name, const_cast<analysis::TypeStruct*>(ts))
      << ":" << std::endl;
  local_state.inc_indent();
  for (const auto& field : ts->fields()) {
    ASSIGN_OR_RETURN(auto field_obj,
                     ts->type_member_store()->GetName(field.name, true),
                     _ << "Finding field object: " << field.name);
    out << local_state.indent() << PythonSafeName(field.name, field_obj)
        << ": ";
    RETURN_IF_ERROR(AddTypeName(field.type_spec, true, &local_state))
        << "In type of field: " << field.name << " in " << name;
    out << " = dataclasses.field(default_factory="
        << DefaultFieldFactory(field.type_spec, &local_state) << ")"
        << std::endl;  //  # type: ignore ?
  }
  local_state.dec_indent();
  out << std::endl;
  superstate->out() << local_state.out_str();
  superstate->AddImports(local_state);
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertSchemaDefinition(
    const analysis::SchemaDefinitionExpression& expression,
    ConvertState* state) const {
  return ConvertStructType(CHECK_NOTNULL(expression.def_schema()),
                           static_cast<PythonConvertState*>(state));
}

absl::Status PythonConverter::ConvertTypeDefinition(
    const analysis::TypeDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  if (analysis::TypeUtils::IsStructType(*expression.defined_type_spec())) {
    return ConvertStructType(static_cast<const analysis::TypeStruct*>(
                                 expression.defined_type_spec()),
                             bstate);
  }
  bstate->clear_inline();
  bstate->out() << PythonSafeName(expression.type_name(),
                                  expression.named_object())
                << " = ";
  RETURN_IF_ERROR(AddTypeName(expression.defined_type_spec(), true, bstate))
      << "In typedef of " << expression.type_name();
  bstate->out() << std::endl;
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertFunctionGroup(
    analysis::FunctionGroup* group, PythonConvertState* state) const {
  auto superstate = state->top_superstate();
  if (!superstate->RegisterFunctionGroup(group)) {
    return absl::OkStatus();
  }
  if (bindings_on_use_) {
    return absl::OkStatus();
  }
  absl::flat_hash_map<std::string, analysis::Function*> bindings_map;
  bool skip_all_conversion = true;
  for (const auto fun : group->functions()) {
    if (!fun->is_skip_conversion()) {
      skip_all_conversion = false;
    }
    for (const auto& it : fun->bindings_by_name()) {
      analysis::Function* crt_fun = CHECK_NOTNULL(it.second.second);
      if ((crt_fun->is_native() || !crt_fun->expressions().empty()) &&
          !crt_fun->is_skip_conversion()) {
        bindings_map.emplace(it.first, crt_fun);
      }
    }
  }
  if (skip_all_conversion) {
    return absl::OkStatus();
  }
  PythonConvertState local_state(superstate);
  local_state.out() << std::endl
                    << "class " << PythonSafeName(group->call_name(), group)
                    << ":" << std::endl;
  local_state.inc_indent();
  for (const auto& it : bindings_map) {
    auto fun = it.second;
    local_state.out() << local_state.indent() << it.first << ": ";
    RETURN_IF_ERROR(AddTypeName(fun->type_spec(), false, &local_state))
        << "Adding function group type for function: " << fun->full_name();
    local_state.out() << " = " << PythonSafeName(fun->call_name(), fun)
                      << std::endl;
  }
  local_state.out() << local_state.indent()
                    << "def __new__(cls, *args):" << std::endl;
  local_state.inc_indent();
  if (group->functions().size() == 1 && !bindings_map.empty()) {
    // This allows the call of a function group - for variables
    // defined as a function, then called.
    local_state.out() << local_state.indent() << "return cls."
                      << bindings_map.begin()->first << "(*args)";
  } else {
    // TODO(catalin): Here basically we hope for best - need to be better.
    local_state.out() << local_state.indent() << "pass";
  }
  local_state.dec_indent();
  local_state.out() << std::endl;
  local_state.dec_indent();
  local_state.out() << std::endl;
  superstate->AddImports(local_state);
  superstate->out() << local_state.out_str();
  return absl::OkStatus();
}

absl::StatusOr<bool> PythonConverter::ConvertBindings(
    analysis::Function* fun, PythonConvertState* state) const {
  bool has_converted = false;
  for (const auto& it : fun->bindings_by_name()) {
    analysis::Function* crt_fun = CHECK_NOTNULL(it.second.second);
    bool crt_converted = false;
    if ((crt_fun->is_native() || !crt_fun->expressions().empty()) &&
        !crt_fun->is_skip_conversion()) {
      // bool crt_converted = !it.second.first.has_value();
      // if (!crt_converted) {
      ASSIGN_OR_RETURN(crt_converted, ConvertFunction(crt_fun, false, state));
    }
    if (crt_converted) {
      has_converted = true;
    }
  }
  return has_converted;
}

absl::StatusOr<bool> PythonConverter::ConvertFunction(
    analysis::Function* fun, bool is_on_use, ConvertState* state) const {
  const bool is_lambda = fun->kind() == pb::ObjectKind::OBJ_LAMBDA;
  auto bstate = static_cast<PythonConvertState*>(state);
  if (!fun->is_native() && fun->expressions().empty()) {
    if (bindings_on_use_) {
      RET_CHECK(!is_on_use) << stacktrace::ToString();
      return true;
    }
    return ConvertBindings(fun, bstate);  // Untyped and unused function.
  }
  if (bindings_on_use_ && fun->binding_parent().has_value() && !is_on_use) {
    return true;
  }
  auto superstate = bstate->top_superstate();
  if (!superstate->RegisterFunction(fun)) {
    return true;  // Already converted
  }
  if (fun->is_skip_conversion()) {
    return true;  // No need to convert this one
  }
  const bool is_pure_native =
      (fun->is_native() && !fun->is_struct_constructor());
  PythonConvertState local_state(superstate);
  auto& out = local_state.out();
  out << std::endl << "def ";
  if (bindings_on_use_) {
    ASSIGN_OR_RETURN(auto name, LocalFunctionName(fun, false, state));
    out << name;
  } else {
    out << PythonSafeName(fun->call_name(), fun);
  }
  out << "(" << std::endl;
  local_state.inc_indent(2);
  RET_CHECK(fun->arguments().size() == fun->default_values().size());
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      out << "," << std::endl;
    }
    const auto& arg = fun->arguments()[i];
    out << local_state.indent() << PythonSafeName(arg->name(), arg.get());
    if (!is_pure_native) {
      out << ": ";
      RETURN_IF_ERROR(AddTypeName(arg->converted_type(), false, &local_state))
          << "In typedef of argument: " << arg->name() << " of "
          << fun->call_name();
    }
    if (!is_lambda && fun->default_values()[i].has_value()) {
      out << " = ";
      RETURN_IF_ERROR(ConvertInlineExpression(*fun->default_values()[i].value(),
                                              &local_state));
    }
  }
  out << ")";
  if (!is_pure_native) {
    out << " -> ";
    RETURN_IF_ERROR(AddTypeName(fun->result_type(), false, &local_state))
        << "In typedef of result type of " << fun->call_name();
  }
  out << ":" << std::endl;
  local_state.dec_indent(2);
  if (fun->is_native()) {
    NativeConvert convert(fun, &local_state);
    for (const auto& arg : fun->arguments()) {
      convert.add_arg(arg->name(), PythonSafeName(arg->name(), arg.get()));
    }
    RETURN_IF_ERROR(convert.Prepare());
    ASSIGN_OR_RETURN(auto macros,
                     ProcessMacros(convert.FindMacros(), bstate->module(),
                                   nullptr, fun, state),
                     _ << "Processing macros in function definition of: "
                       << fun->full_name());
    convert.PrepareMacros(macros);
    ASSIGN_OR_RETURN(std::string replaced, convert.Replace());
    local_state.inc_indent();
    out << local_state.indent() << "return " << replaced;
    local_state.dec_indent();
  } else if (fun->expressions().empty()) {
    // TODO(catalin): this is a bit tricky - we cannot devise a body if
    //   types not defined.
    return status::InvalidArgumentErrorBuilder(
               "Cannot build function with unbound types: ")
           << fun->full_name();
  } else {
    RET_CHECK(fun->expressions().size() == 1) << "For: " << fun->full_name();
    RETURN_IF_ERROR(
        ConvertExpression(*fun->expressions().front(), &local_state));
  }
  out << std::endl;
  superstate->out() << local_state.out_str();
  superstate->AddImports(local_state);
  RETURN_IF_ERROR(ConvertBindings(fun, bstate).status());
  return true;
}

absl::StatusOr<std::string> PythonConverter::ConvertMainFunction(
    analysis::Function* fun, PythonConvertState* state) const {
  std::string s;
  absl::StrAppend(&s, "import absl.app\n");
  const std::string module_name(
      PythonSafeName(state->module()->module_name(), state->module()));
  absl::StrAppend(&s, "import ", module_name, "\n\n");
  ASSIGN_OR_RETURN(auto fname, LocalFunctionName(fun, true, state));
  absl::StrAppend(&s,
                  "if __name__ == \"__main__\":\n"
                  "  absl.app.run(lambda _: ",
                  module_name, ".", fname, "())\n");
  return s;
}

absl::StatusOr<std::string> PythonConverter::LocalFunctionName(
    analysis::Function* fun, bool is_on_use, ConvertState* state) const {
  RET_CHECK(bindings_on_use_);
  if (fun->is_abstract()) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot call abstract function: " << fun->name();
  }
  if (is_on_use) {
    RETURN_IF_ERROR(ConvertFunction(fun, true, state).status());
  }
  if (state->module() == fun->module_scope()) {
    return PythonSafeName(fun->call_name(), fun);
  }
  if (state->module() == fun->built_in_scope()) {
    return PythonSafeName(absl::StrCat("__builtin__", fun->call_name()), fun);
  }
  return PythonSafeName(
      absl::StrCat(
          absl::StrJoin(absl::StrSplit(fun->module_scope()->name(), "."), "__"),
          "__", fun->call_name()),
      fun);
}

/*
absl::StatusOr<std::string> PythonConverter::LocalOjbectName(
    analysis::NamedObject* object, const ScopedName& scoped_name) {
  if (bindings_on_use_) {
  if (scoped_name.
}
*/

// TODO(catalin): this functionality can sit at the converter level.
absl::StatusOr<absl::flat_hash_map<std::string, std::unique_ptr<ConvertState>>>
PythonConverter::ProcessMacros(const absl::flat_hash_set<std::string>& macros,
                               analysis::Scope* scope,
                               analysis::FunctionBinding* binding,
                               analysis::Function* fun,
                               ConvertState* state) const {
  auto result_type =
      (binding ? binding->type_spec->ResultType() : fun->result_type());
  if (!result_type) {
    return status::InvalidArgumentErrorBuilder()
           << "Function: " << fun->function_name()
           << " has not a result type defined.";
  }
  absl::flat_hash_map<std::string, std::unique_ptr<ConvertState>> result;
  auto bstate = static_cast<PythonConvertState*>(state);
  for (const auto& macro : macros) {
    auto sub_state = std::make_unique<PythonConvertState>(bstate, true);
    if (macro == "result_type") {
      RETURN_IF_ERROR(AddTypeName(result_type, true, sub_state.get()))
          << "Converting type name per macro: " << macro;
    } else if (macro == "result_seed" || macro == "dataset_seed") {
      auto res_type = result_type;
      if (macro == "dataset_seed") {
        RET_CHECK(analysis::TypeUtils::IsDatasetType(*result_type) &&
                  result_type->ResultType())
            << "Invalid type for macro " << macro
            << " - found: " << result_type->full_name();
        res_type = result_type->ResultType();
      }
      ASSIGN_OR_RETURN(auto default_value,
                       bstate->module()->BuildDefaultValueExpression(res_type),
                       _ << "Processing conversion macro: " << macro
                         << " for result type: " << res_type->full_name());
      RETURN_IF_ERROR(ConvertExpression(*default_value, sub_state.get()));
      RETURN_IF_ERROR(sub_state->CheckInline(*default_value))
          << "Converting default expression per macro: " << macro;
    } else {
      return status::UnimplementedErrorBuilder()
             << "Unknown macro: " << macro
             << " in function: " << fun->function_name();
    }
    result.emplace(macro, std::move(sub_state));
  }
  return {std::move(result)};
}

}  // namespace conversion
}  // namespace nudl
