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
#include "absl/flags/flag.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "nudl/status/status.h"

ABSL_FLAG(bool, pyconvert_print_name_changes, false,
          "If we should output the name changes done for python conversion");

namespace nudl {
namespace conversion {

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

 protected:
  PythonConvertState* const superstate_ = nullptr;
  const bool should_inline_ = false;
  const size_t indent_delta_;
  std::stringstream out_;
  size_t indent_ = 0;
  std::string indent_str_;
  absl::flat_hash_set<analysis::Function*> converted_functions_;
  std::vector<analysis::Function*> in_function_call_;
  absl::flat_hash_set<std::string> imports_;
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
  if (converted_functions_.contains(fun)) {
    return false;
  }
  converted_functions_.emplace(fun);
  return true;
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

PythonConverter::PythonConverter() : Converter() {}

absl::StatusOr<std::unique_ptr<ConvertState>> PythonConverter::BeginModule(
    analysis::Module* module) const {
  return std::make_unique<PythonConvertState>(module);
}

absl::StatusOr<std::string> PythonConverter::FinishModule(
    analysis::Module* module, std::unique_ptr<ConvertState> state) const {
  auto bstate = static_cast<PythonConvertState*>(state.get());
  return {bstate->out_str()};
}

absl::Status PythonConverter::ConvertInlineExpression(
    const analysis::Expression& expression, PythonConvertState* state) const {
  PythonConvertState expression_state(state, true);
  RETURN_IF_ERROR(ConvertExpression(expression, &expression_state));
  RETURN_IF_ERROR(expression_state.CheckInline(expression));
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
    local_state.add_import("from _nudl_builtin import *");
  }
  for (const auto& expression : module->expressions()) {
    PythonConvertState expression_state(&local_state);
    RETURN_IF_ERROR(ConvertExpression(*expression.get(), &expression_state));
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
          {pb::TypeId::DATETIME_ID, {"'datetime.datetime'", "datetime"}},
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
          {pb::TypeId::DATASET_ID, {"nudl.Dataset", "nudl"}},
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

std::string GetStructTypeName(const analysis::TypeSpec* type_spec,
                              PythonConvertState* state) {
  std::string module_name;
  std::string prefix;
  if (!type_spec->scope_name().empty() &&
      type_spec->scope_name().name() != state->module()->name()) {
    if (!type_spec->scope_name().module_names().empty()) {
      state->add_import(absl::StrCat(
          "import ", PythonSafeName(type_spec->scope_name().module_name(),
                                    type_spec->definition_scope())));
    }
    prefix = PythonSafeName(scope_name(type_spec->scope_name(), true),
                            type_spec->definition_scope());
  }
  return absl::StrCat(
      prefix, PythonSafeName(type_spec->name(),
                             const_cast<analysis::TypeSpec*>(type_spec)));
}

absl::Status AddTypeName(const analysis::TypeSpec* type_spec,
                         PythonConvertState* state) {
  if (type_spec->type_id() == pb::TypeId::STRUCT_ID) {
    state->out() << GetStructTypeName(type_spec, state);
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
  if (type_spec->parameters().empty()) {
    return absl::OkStatus();
  }
  state->out() << "[";
  if (type_spec->type_id() == pb::TypeId::FUNCTION_ID) {
    state->out() << "[";
    for (size_t i = 0; i + 1 < type_spec->parameters().size(); ++i) {
      if (i) {
        state->out() << ", ";
      }
      RETURN_IF_ERROR(AddTypeName(type_spec->parameters()[i], state));
    }
    state->out() << "]";
    state->out() << ", ";
    RETURN_IF_ERROR(AddTypeName(type_spec->parameters().back(), state));
  } else if (type_spec->type_id() == pb::TypeId::NULLABLE_ID) {
    RETURN_IF_ERROR(AddTypeName(type_spec->parameters().back(), state));
  } else {
    for (size_t i = 0; i < type_spec->parameters().size(); ++i) {
      if (i) {
        state->out() << ", ";
      }
      RETURN_IF_ERROR(AddTypeName(type_spec->parameters()[i], state));
    }
  }
  state->out() << "]";
  return absl::OkStatus();
}
bool HasType(const analysis::Expression& expression, pb::TypeId type_id) {
  return (expression.stored_type_spec().has_value() &&
          (expression.stored_type_spec().value()->type_id() == type_id));
}
}  // namespace

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
    RETURN_IF_ERROR(AddTypeName(expression.var()->converted_type(), bstate));
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
    analysis::ScopedName local_name(expression.scoped_name().scope_name_ptr(),
                                    binding.value()->call_name());
    bstate->out() << PythonSafeName(scoped_name(local_name), binding.value());
  } else {
    bstate->out() << PythonSafeName(scoped_name(expression.scoped_name()),
                                    expression.named_object());
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
  if (is_set) {
    bstate->out() << "{" << std::endl;
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
  bstate->out() << (is_set ? "}" : "]");
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
  return ConvertIndexExpression(expression, state);
}

absl::Status PythonConverter::ConvertLambdaExpression(
    const analysis::LambdaExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  ASSIGN_OR_RETURN(bool converted,
                   ConvertFunction(expression.lambda_function(), bstate));
  analysis::Function* fun =
      static_cast<analysis::Function*>(expression.named_object().value());
  if (!converted) {
    return status::InternalErrorBuilder()
           << "Cannot convert unbound / missing expression function: "
           << fun->full_name();
  }
  bstate->out() << "lambda ";
  RET_CHECK(fun->arguments().size() == fun->default_values().size());
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
  bstate->out() << ": " << PythonSafeName(fun->call_name(), fun) << "(";
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
    bstate->out() << PythonSafeName(binding.value()->call_name(),
                                    binding.value());
  } else {
    bstate->out() << PythonSafeName(expression.name().name(),
                                    expression.named_object());
  }
  return absl::OkStatus();
}

struct NativeConvert {
  NativeConvert(analysis::Function* fun, PythonConvertState* state)
      : fun(fun), state(state) {}

  absl::StatusOr<std::string> Replace() {
    RETURN_IF_ERROR(Prepare());
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

 private:
  absl::Status Prepare() {
    RET_CHECK(fun->is_native());
    if (PrepareStructConstructor()) {
      return absl::OkStatus();
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
  bool PrepareStructConstructor() {
    bool is_copy = false;
    auto it = fun->native_impl().find(analysis::kStructObjectConstructor);
    if (it == fun->native_impl().end()) {
      it = fun->native_impl().find(analysis::kStructCopyConstructor);
      if (it == fun->native_impl().end()) {
        return false;
      }
      is_copy = true;
    }
    const std::string args_str =
        absl::StrJoin(arguments_ordered, ", ",
                      [this, is_copy](std::string* out, const std::string& s) {
                        if (is_copy) {
                          absl::StrAppend(out, arg_escaped(s));
                        } else {
                          absl::StrAppend(out, s, "=", arg_escaped(s));
                        }
                      });
    if (is_copy) {
      state->add_import("import copy");
      code = absl::StrCat("copy.deepcopy(", args_str, ")");
    } else {
      code = absl::StrCat(it->second, "(", args_str, ")");
    }
    return true;
  }

  analysis::Function* const fun;
  PythonConvertState* const state;
  std::string code;
  absl::flat_hash_map<std::string, std::string> arguments;
  absl::flat_hash_set<std::string> skipped;
  std::vector<std::string> arguments_ordered;
};

absl::Status PythonConverter::ConvertNativeFunctionCallExpression(
    const analysis::FunctionCallExpression& expression, analysis::Function* fun,
    PythonConvertState* state) const {
  NativeConvert convert(fun, state);
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
    convert.add_arg(arg_name, expression_state.out_str());
  }
  ASSIGN_OR_RETURN(std::string replaced, convert.Replace());
  state->out() << "(" << replaced << ")";
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertFunctionCallExpression(
    const analysis::FunctionCallExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  if (expression.function_binding()->fun.has_value() &&
      expression.function_binding()->fun.value()->is_native()) {
    RETURN_IF_ERROR(ConvertNativeFunctionCallExpression(
        expression, expression.function_binding()->fun.value(), bstate));
    return absl::OkStatus();
  }
  if (expression.left_expression().has_value() &&
      !expression.is_method_call()) {
    if (expression.function_binding()->fun.has_value()) {
      PythonConvertState::FunctionPusher pusher(
          bstate, expression.function_binding()->fun.value());
      RETURN_IF_ERROR(ConvertInlineExpression(
          *expression.left_expression().value(), bstate));
    } else {
      RETURN_IF_ERROR(ConvertInlineExpression(
          *expression.left_expression().value(), bstate));
    }
  } else {
    RET_CHECK(expression.function_binding()->fun.has_value());
    analysis::Function* fun = expression.function_binding()->fun.value();
    if (bstate->module() != fun->module_scope()) {
      bstate->out() << PythonSafeName(scoped_name(fun->qualified_call_name()),
                                      fun);
    } else {
      bstate->out() << PythonSafeName(fun->call_name(), fun);
    }
  }
  // TODO(catalin): this in practice is a bit more complicated, but
  //  we do something simple.
  RET_CHECK(expression.function_binding()->call_expressions.size() ==
            expression.function_binding()->names.size());
  bstate->out() << "(\n";
  bstate->inc_indent(2);
  bool has_arguments = false;
  for (size_t i = 0; i < expression.function_binding()->names.size(); ++i) {
    if (!expression.function_binding()->call_expressions[i].has_value()) {
      continue;
    }
    if (has_arguments) {
      bstate->out() << "," << std::endl;
    }
    has_arguments = true;
    if (expression.function_binding()->fun.has_value()) {
      bstate->out() << bstate->indent()
                    << expression.function_binding()->names[i] << "=";
    }
    // TODO(catalin): note - this may convert the default expressions
    // as well, which may not be valid in this scope - will want to massage
    // this a bit - Note - actually this may be ok, because we would prefix
    // all names w/ proper module names - need to proper test.
    RETURN_IF_ERROR(ConvertInlineExpression(
        *expression.function_binding()->call_expressions[i].value(), bstate));
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
      RETURN_IF_ERROR(ConvertFunction(fun, state).status());
    }
  } else if (analysis::Function::IsFunctionKind(*fun_object)) {
    RETURN_IF_ERROR(
        ConvertFunction(static_cast<analysis::Function*>(fun_object), state)
            .status());
  }
  return ConvertFunction(expression.def_function(), state).status();
}

absl::Status PythonConverter::ConvertSchemaDefinition(
    const analysis::SchemaDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->clear_inline();
  bstate->add_import("import dataclasses");
  auto ts = CHECK_NOTNULL(expression.def_schema());
  bstate->out() << std::endl
                << "@dataclasses.dataclass" << std::endl
                << "class "
                << PythonSafeName(ts->name(),
                                  const_cast<analysis::TypeStruct*>(ts))
                << ":" << std::endl;
  bstate->inc_indent();
  for (const auto& field : ts->fields()) {
    ASSIGN_OR_RETURN(auto field_obj,
                     ts->type_member_store()->GetName(field.name, true),
                     _ << "Finding field object: " << field.name);
    bstate->out() << bstate->indent() << PythonSafeName(field.name, field_obj)
                  << ": ";
    RETURN_IF_ERROR(AddTypeName(field.type_spec, bstate))
        << "In type of field: " << field.name << " in " << ts->name();
    // TODO(catalin) - Need the default values + constructor & stuff when
    //    they come
    bstate->out() << std::endl;
  }
  bstate->dec_indent();
  bstate->out() << std::endl;
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertTypeDefinition(
    const analysis::TypeDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->clear_inline();
  bstate->out() << PythonSafeName(expression.type_name(),
                                  expression.named_object())
                << " = ";
  RETURN_IF_ERROR(AddTypeName(expression.defined_type_spec(), bstate))
      << "In typedef of " << expression.type_name();
  bstate->out() << std::endl;
  return absl::OkStatus();
}

absl::StatusOr<bool> PythonConverter::ConvertBindings(
    analysis::Function* fun, ConvertState* state) const {
  bool has_conversions = false;
  for (const auto& binding : fun->bindings()) {
    ASSIGN_OR_RETURN(const bool has_converted,
                     ConvertFunction(binding.get(), state));
    if (has_converted) {
      has_conversions = true;
    }
  }
  return has_conversions;
}

absl::StatusOr<bool> PythonConverter::ConvertFunction(
    analysis::Function* fun, ConvertState* state) const {
  const bool is_lambda = fun->kind() == pb::ObjectKind::OBJ_LAMBDA;
  if (!fun->is_native() && fun->expressions().empty()) {  // && !is_lambda) {
    return ConvertBindings(fun, state);  // Untyped and unused function.
  }
  auto bstate = static_cast<PythonConvertState*>(state);
  auto superstate = bstate->top_superstate();
  if (!superstate->RegisterFunction(fun)) {
    return true;  // Already converted
  }
  if (fun->is_skip_conversion()) {
    return true;  // No need to convert this one
  }
  const bool is_pure_native = fun->is_native() && !fun->is_struct_constructor();
  PythonConvertState local_state(superstate);
  auto& out = local_state.out();
  out << std::endl
      << "def " << PythonSafeName(fun->call_name(), fun) << "(" << std::endl;
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
      RETURN_IF_ERROR(AddTypeName(arg->converted_type(), &local_state))
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
    RETURN_IF_ERROR(AddTypeName(fun->result_type(), &local_state))
        << "In typedef of result type of " << fun->call_name();
  }
  out << ":" << std::endl;
  local_state.dec_indent(2);
  if (fun->is_native()) {
    NativeConvert convert(fun, &local_state);
    for (const auto& arg : fun->arguments()) {
      convert.add_arg(arg->name(), PythonSafeName(arg->name(), arg.get()));
    }
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
  RETURN_IF_ERROR(ConvertBindings(fun, state).status());
  return true;
}

namespace {
std::string PythonSafeNameUnit(absl::string_view name,
                               absl::optional<analysis::NamedObject*> object) {
  const bool is_python_special = IsPythonSpecialName(name);
  const bool is_python_keyword = IsPythonKeyword(name);
  const bool is_nudl_name = absl::EndsWith(name, kPythonRenameEnding);
  bool is_python_builtin = IsPythonBuiltin(name);
  if (object.has_value() &&
      object.value()->kind() == pb::ObjectKind::OBJ_FIELD) {
    is_python_builtin = false;
  }
  if (!is_python_special && !is_python_special && !is_nudl_name &&
      !is_python_builtin) {
    return std::string(name);
  }
  const std::string new_name(absl::StrCat(name, kPythonRenameEnding));
  if (absl::GetFlag(FLAGS_pyconvert_print_name_changes)) {
    std::cerr << "Renaming: `" << name << "` as `" << new_name << "`"
              << (is_python_special ? " Is Python special function name;" : "")
              << (is_python_keyword ? " Is Python keyword;" : "")
              << (is_nudl_name ? " Is a nudl python-safe name;" : "")
              << (is_python_builtin ? " Is Python builtin / standard name;"
                                    : "")
              << std::endl;
  }
  return new_name;
}
}  // namespace

// TODO(catalin): May want to tweak this one if creates too much damage:
std::string PythonSafeName(absl::string_view name,
                           absl::optional<analysis::NamedObject*> object) {
  if (object.has_value() &&
      analysis::Function::IsFunctionKind(*object.value()) &&
      (static_cast<analysis::Function*>(object.value())
           ->is_skip_conversion())) {
    return std::string(name);
  }
  std::vector<std::string> comp = absl::StrSplit(name, ".");
  std::reverse(comp.begin(), comp.end());
  std::vector<std::string> result;
  result.reserve(comp.size());
  auto crt_object = object;
  for (const auto& s : comp) {
    result.emplace_back(PythonSafeNameUnit(s, crt_object));
    do {
      if (crt_object.has_value()) {
        crt_object = crt_object.value()->parent_store();
      } else {
        crt_object = {};
      }
    } while (crt_object.has_value() &&
             analysis::FunctionGroup::IsFunctionGroup(*crt_object.value()));
  }
  std::reverse(result.begin(), result.end());
  return absl::StrJoin(result, ".");
}

bool IsPythonKeyword(absl::string_view name) {
  static const auto kPythonKeywords = new absl::flat_hash_set<std::string>({
      // Keywords:
      "await",    "else",  "import", "pass",    "None",   "break",  "except",
      "in",       "raise", "class",  "finally", "is",     "return", "and",
      "continue", "for",   "lambda", "try",     "as",     "def",    "from",
      "nonlocal", "while", "assert", "del",     "global", "not",    "with",
      "async",    "elif",  "if",     "or",      "yield",  "False",  "True",
  });
  return kPythonKeywords->contains(name);
}

bool IsPythonBuiltin(absl::string_view name) {
  static const auto kPythonNames = new absl::flat_hash_set<std::string>({
      // Builtins
      "__name__",
      "__doc__",
      "__package__",
      "__loader__",
      "__spec__",
      "__build_class__",
      "__import__",
      "abs",
      "all",
      "any",
      "ascii",
      "bin",
      "breakpoint",
      "callable",
      "chr",
      "compile",
      "delattr",
      "dir",
      "divmod",
      "eval",
      "exec",
      "format",
      "getattr",
      "globals",
      "hasattr",
      "hash",
      "hex",
      "id",
      "input",
      "isinstance",
      "issubclass",
      "iter",
      "len",
      "locals",
      "max",
      "min",
      "next",
      "oct",
      "ord",
      "pow",
      "print",
      "repr",
      "round",
      "setattr",
      "sorted",
      "sum",
      "vars",
      "None",
      "Ellipsis",
      "NotImplemented",
      "bool",
      "memoryview",
      "bytearray",
      "bytes",
      "classmethod",
      "complex",
      "dict",
      "enumerate",
      "filter",
      "float",
      "frozenset",
      "property",
      "int",
      "list",
      "map",
      "object",
      "range",
      "reversed",
      "set",
      "slice",
      "staticmethod",
      "str",
      "super",
      "tuple",
      "type",
      "zip",
      "__debug__",
      "BaseException",
      "Exception",
      "TypeError",
      "StopAsyncIteration",
      "StopIteration",
      "GeneratorExit",
      "SystemExit",
      "KeyboardInterrupt",
      "ImportError",
      "ModuleNotFoundError",
      "OSError",
      "EnvironmentError",
      "IOError",
      "EOFError",
      "RuntimeError",
      "RecursionError",
      "NotImplementedError",
      "NameError",
      "UnboundLocalError",
      "AttributeError",
      "SyntaxError",
      "IndentationError",
      "TabError",
      "LookupError",
      "IndexError",
      "KeyError",
      "ValueError",
      "UnicodeError",
      "UnicodeEncodeError",
      "UnicodeDecodeError",
      "UnicodeTranslateError",
      "AssertionError",
      "ArithmeticError",
      "FloatingPointError",
      "OverflowError",
      "ZeroDivisionError",
      "SystemError",
      "ReferenceError",
      "MemoryError",
      "BufferError",
      "Warning",
      "UserWarning",
      "DeprecationWarning",
      "PendingDeprecationWarning",
      "SyntaxWarning",
      "RuntimeWarning",
      "FutureWarning",
      "ImportWarning",
      "UnicodeWarning",
      "BytesWarning",
      "ResourceWarning",
      "ConnectionError",
      "BlockingIOError",
      "BrokenPipeError",
      "ChildProcessError",
      "ConnectionAbortedError",
      "ConnectionRefusedError",
      "ConnectionResetError",
      "FileExistsError",
      "FileNotFoundError",
      "IsADirectoryError",
      "NotADirectoryError",
      "InterruptedError",
      "PermissionError",
      "ProcessLookupError",
      "TimeoutError",
      "open",
      "quit",
      "exit",
      "copyright",
      "credits",
      "license",
      "help",
      "_",
      // General module names:
      "__future__",
      "__main__",
      "_thread",
      "abc",
      "aifc",
      "argparse",
      "array",
      "ast",
      "asynchat",
      "asyncio",
      "asyncore",
      "atexit",
      "audioop",
      "base64",
      "bdb",
      "binascii",
      "bisect",
      "builtins",
      "bz2",
      "calendar",
      "cgi",
      "cgitb",
      "chunk",
      "cmath",
      "cmd",
      "code",
      "codecs",
      "codeop",
      "collections",
      "colorsys",
      "compileall",
      "concurrent",
      "contextlib",
      "contextvars",
      "copy",
      "copyreg",
      "cProfile",
      "csv",
      "ctypes",
      "curses",
      "dataclasses",
      "datetime",
      "dbm",
      "decimal",
      "difflib",
      "dis",
      "distutils",
      "doctest",
      "email",
      "encodings",
      "ensurepip",
      "enum",
      "errno",
      "faulthandler",
      "fcntl",
      "filecmp",
      "fileinput",
      "fnmatch",
      "fractions",
      "ftplib",
      "functools",
      "gc",
      "getopt",
      "getpass",
      "gettext",
      "glob",
      "graphlib",
      "grp",
      "gzip",
      "hashlib",
      "heapq",
      "hmac",
      "html",
      "http",
      "idlelib",
      "imaplib",
      "imghdr",
      "imp",
      "importlib",
      "inspect",
      "io",
      "ipaddress",
      "itertools",
      "json",
      "keyword",
      "lib2to3",
      "linecache",
      "locale",
      "logging",
      "lzma",
      "mailbox",
      "mailcap",
      "marshal",
      "math",
      "mimetypes",
      "mmap",
      "modulefinder",
      "msilib",
      "msvcrt",
      "multiprocessing",
      "netrc",
      "nis",
      "nntplib",
      "numbers",
      "operator",
      "optparse",
      "os",
      "ossaudiodev",
      "pathlib",
      "pdb",
      "pickle",
      "pickletools",
      "pipes",
      "pkgutil",
      "platform",
      "plistlib",
      "poplib",
      "posix",
      "pprint",
      "profile",
      "pstats",
      "pty",
      "pwd",
      "py_compile",
      "pyclbr",
      "pydoc",
      "queue",
      "quopri",
      "random",
      "re",
      "readline",
      "reprlib",
      "resource",
      "rlcompleter",
      "runpy",
      "sched",
      "secrets",
      "select",
      "selectors",
      "shelve",
      "shlex",
      "shutil",
      "signal",
      "site",
      "smtpd",
      "smtplib",
      "sndhdr",
      "socket",
      "socketserver",
      "spwd",
      "sqlite3",
      "ssl",
      "stat",
      "statistics",
      "string",
      "stringprep",
      "struct",
      "subprocess",
      "sunau",
      "symtable",
      "sys",
      "sysconfig",
      "syslog",
      "tabnanny",
      "tarfile",
      "telnetlib",
      "tempfile",
      "termios",
      "test",
      "textwrap",
      "threading",
      "time",
      "timeit",
      "tkinter",
      "token",
      "tokenize",
      "tomllib",
      "trace",
      "traceback",
      "tracemalloc",
      "tty",
      "turtle",
      "turtledemo",
      "types",
      "typing",
      "unicodedata",
      "unittest",
      "urllib",
      "uu",
      "uuid",
      "venv",
      "warnings",
      "wave",
      "weakref",
      "webbrowser",
      "winreg",
      "winsound",
      "wsgiref",
      "xdrlib",
      "xml",
      "xmlrpc",
      "zipapp",
      "zipfile",
      "zipimport",
      "zlib",
      "zoneinfo",
  });
  return kPythonNames->contains(name);
}

bool IsPythonSpecialName(absl::string_view name) {
  // Cut out all the __init__ and related python name.
  return absl::StartsWith(name, "__") && absl::EndsWith(name, "__");
}

}  // namespace conversion
}  // namespace nudl
