#include "nudl/analysis/python_converter.h"

#include <algorithm>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "nudl/status/status.h"

namespace nudl {
namespace analysis {

class PythonConvertState : public ConvertState {
 public:
  explicit PythonConvertState(Module* module, bool should_inline = false,
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
  bool RegisterFunction(Function* fun);

  // Used to mark the current function call in order
  // to use the proper names in identifier & dot expressiosn.
  const Function* in_function_call() const;
  void push_in_function_call(const Function* fun);
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
  absl::Status CheckInline(const Expression& expression) const;

  // On stack helper for maintaining the function call stack.
  class FunctionPusher {
   public:
    FunctionPusher(PythonConvertState* state, const Function* fun)
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
  absl::flat_hash_set<Function*> converted_functions_;
  std::vector<const Function*> in_function_call_;
  absl::flat_hash_set<std::string> imports_;
  bool is_inline_ = true;
};

PythonConvertState::PythonConvertState(Module* module, bool should_inline,
                                       size_t indent_delta)
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
  imports_.emplace(import_stmt);
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

bool PythonConvertState::RegisterFunction(Function* fun) {
  if (converted_functions_.contains(fun)) {
    return false;
  }
  converted_functions_.emplace(fun);
  return true;
}

absl::Status PythonConvertState::CheckInline(
    const Expression& expression) const {
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
std::string scope_name(const ScopeName& name, bool with_final_dot = false) {
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

std::string scoped_name(const ScopedName& name) {
  if (name.name().empty()) {
    return scope_name(name.scope_name());
  }
  return absl::StrCat(scope_name(name.scope_name(), true), name.name());
}
}  // namespace

const Function* PythonConvertState::in_function_call() const {
  if (in_function_call_.empty()) {
    return nullptr;
  }
  return in_function_call_.back();
}

void PythonConvertState::push_in_function_call(const Function* fun) {
  in_function_call_.emplace_back(CHECK_NOTNULL(fun));
  CHECK(in_function_call() == fun);
}

void PythonConvertState::pop_in_function_call() {
  CHECK(!in_function_call_.empty());
  in_function_call_.pop_back();
}

PythonConverter::PythonConverter() : Converter() {}

absl::StatusOr<std::unique_ptr<ConvertState>> PythonConverter::BeginModule(
    Module* module) const {
  return std::make_unique<PythonConvertState>(module);
}

absl::StatusOr<std::string> PythonConverter::FinishModule(
    Module* module, std::unique_ptr<ConvertState> state) const {
  auto bstate = static_cast<PythonConvertState*>(state.get());
  return {bstate->out().str()};
}

absl::Status PythonConverter::ConvertInlineExpression(
    const Expression& expression, PythonConvertState* state) const {
  PythonConvertState expression_state(state, true);
  RETURN_IF_ERROR(ConvertExpression(expression, &expression_state));
  RETURN_IF_ERROR(expression_state.CheckInline(expression));
  RETURN_IF_ERROR(state->AddState(expression_state));
  return absl::OkStatus();
}

absl::Status PythonConverter::ProcessModule(Module* module,
                                            ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  PythonConvertState local_state(module);
  local_state.add_import("import nudl");
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
          {pb::TypeId::DATASET_ID, {"nudl.Dataset", "nudl"}},
          {pb::TypeId::TYPE_ID, {"type", ""}},
          {pb::TypeId::MODULE_ID, {"types.ModuleType", "types"}},
          {pb::TypeId::INTEGRAL_ID, {"int", ""}},
          {pb::TypeId::CONTAINER_ID,
           {"collections.abc.Container", "collections.abc"}},
          {pb::TypeId::GENERATOR_ID,
           {"collections.abc.Generator", "collections.abc"}},
      });
  auto it = kTypeNames->find(type_id);
  if (it == kTypeNames->end()) {
    return {};
  }
  return it->second;
}

std::string GetStructTypeName(const TypeSpec* type_spec,
                              PythonConvertState* state) {
  std::string module_name;
  std::string prefix;
  if (!type_spec->scope_name().empty() &&
      type_spec->scope_name().name() != state->module()->name()) {
    if (!type_spec->scope_name().module_names().empty()) {
      state->add_import(
          absl::StrCat("import ", type_spec->scope_name().module_name()));
    }
    prefix = scope_name(type_spec->scope_name(), true);
  }
  return absl::StrCat(prefix, type_spec->name());
}

absl::Status AddTypeName(const TypeSpec* type_spec, PythonConvertState* state) {
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
    state->out() << ", ";
    RETURN_IF_ERROR(AddTypeName(type_spec->parameters().back(), state));
  } else if (type_spec->type_id() == pb::TypeId::NULLABLE_ID) {
    RETURN_IF_ERROR(AddTypeName(type_spec->parameters().back(), state));
  } else if (type_spec->type_id() == pb::TypeId::GENERATOR_ID) {
    RETURN_IF_ERROR(AddTypeName(type_spec->parameters().back(), state));
    state->out() << ", None, Non";
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
bool HasType(const Expression& expression, pb::TypeId type_id) {
  return (expression.stored_type_spec().has_value() &&
          (expression.stored_type_spec().value()->type_id() == type_id));
}
}  // namespace
absl::Status PythonConverter::ConvertAssignment(const Assignment& expression,
                                                ConvertState* state) const {
  // TODO(catalin): See the param setting - needs `global` setting or
  // setter function automatically defined.
  RET_CHECK(!expression.children().empty());
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->out() << scoped_name(expression.name());
  if (expression.has_type_spec()) {
    bstate->out() << " : ";
    RETURN_IF_ERROR(AddTypeName(expression.var()->original_type(), bstate));
  }
  bstate->out() << " = ";
  RETURN_IF_ERROR(
      ConvertInlineExpression(*expression.children().front(), bstate))
      << "In assignment";
  bstate->out() << std::endl;
  bstate->clear_inline();
  return absl::OkStatus();
}
absl::Status PythonConverter::ConvertEmptyStruct(const EmptyStruct& expression,
                                                 ConvertState* state) const {
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

absl::Status PythonConverter::ConvertLiteral(const Literal& expression,
                                             ConvertState* state) const {
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
             << expression.build_type_spec()->full_name() << kBugNotice;
  }
  return absl::OkStatus();
}

namespace {
absl::optional<const Function*> GetFunctionBinding(const Expression& expression,
                                                   PythonConvertState* state) {
  const Function* check_fun = state->in_function_call();
  if (!check_fun) {
    return {};
  }
  auto named_obj = expression.named_object();
  if (!named_obj.has_value()) {
    return {};
  }
  if (Function::IsFunctionKind(*named_obj.value())) {
    auto fun = static_cast<const Function*>(named_obj.value());
    if (!fun->IsBinding(check_fun)) {
      return check_fun;
    }
  } else if (FunctionGroup::IsFunctionGroup(*named_obj.value())) {
    auto group = static_cast<const FunctionGroup*>(named_obj.value());
    if (group->FindBinding(check_fun)) {
      return check_fun;
    }
  }
  return {};
}
}  // namespace

absl::Status PythonConverter::ConvertIdentifier(const Identifier& expression,
                                                ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  auto binding = GetFunctionBinding(expression, bstate);
  if (binding.has_value()) {
    ScopedName local_name(expression.scoped_name().scope_name_ptr(),
                          binding.value()->call_name());
    bstate->out() << scoped_name(local_name);
  } else {
    bstate->out() << scoped_name(expression.scoped_name());
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertFunctionResult(
    const FunctionResultExpression& expression, ConvertState* state) const {
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
      bstate->out() << "pass";
      break;
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertArrayDefinition(
    const ArrayDefinitionExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  const bool is_set = HasType(expression, pb::TypeId::SET_ID);
  if (is_set) {
    bstate->out() << "{" << std::endl;
  } else {
    bstate->out() << "[" << std::endl;
  }
  bstate->inc_indent(2);
  for (int i = 0; i < expression.children().size(); ++i) {
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
    const MapDefinitionExpression& expression, ConvertState* state) const {
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
    const IfExpression& expression, ConvertState* state) const {
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
    const ExpressionBlock& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->clear_inline();
  bstate->inc_indent();
  for (const auto& expr : expression.children()) {
    bstate->out() << bstate->indent();
    if (expr->is_default_return()) {
      if (expr->expr_kind() == pb::ExpressionKind::EXPR_ASSIGNMENT) {
        // Special case - assignment is not inline, so process assignment, then
        // return the assigned identifier:
        RETURN_IF_ERROR(ConvertExpression(*expr, state));
        bstate->out()
            << "return "
            << scoped_name(static_cast<const Assignment*>(expr.get())->name());
      } else {
        bstate->out() << "return ";
        RETURN_IF_ERROR(ConvertInlineExpression(*expr, bstate))
            << "For the implicit return expression in function";
      }
    } else {
      RETURN_IF_ERROR(ConvertExpression(*expr, state));
    }
    bstate->out() << std::endl;
  }
  bstate->dec_indent();
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertIndexExpression(
    const IndexExpression& expression, ConvertState* state) const {
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
    const TupleIndexExpression& expression, ConvertState* state) const {
  return ConvertIndexExpression(expression, state);
}

absl::Status PythonConverter::ConvertLambdaExpression(
    const LambdaExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  RETURN_IF_ERROR(ConvertFunction(expression.lambda_function(), bstate));
  bstate->out() << "lambda ";
  Function* fun = static_cast<Function*>(expression.named_object().value());
  RET_CHECK(fun->arguments().size() == fun->default_values().size());
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      bstate->out() << ", ";
    }
    const auto& arg = fun->arguments()[i];
    bstate->out() << arg->name();
    if (fun->default_values()[i].has_value()) {
      bstate->out() << " = ";
      RETURN_IF_ERROR(
          ConvertInlineExpression(*fun->default_values()[i].value(), bstate))
          << "For default expression in lambda: " << i;
    }
  }
  bstate->out() << ": " << fun->call_name() << "(";
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      bstate->out() << ", ";
    }
    bstate->out() << fun->arguments()[i]->name();
  }
  bstate->out() << ")";
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertDotAccessExpression(
    const DotAccessExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  RET_CHECK(expression.children().size() == 1);
  RETURN_IF_ERROR(
      ConvertInlineExpression(*expression.children().front(), bstate));
  bstate->out() << ".";
  auto binding = GetFunctionBinding(expression, bstate);
  if (binding.has_value()) {
    bstate->out() << binding.value()->call_name();
  } else {
    bstate->out() << expression.name().name();
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertNativeFunctionCallExpression(
    const FunctionCallExpression& expression, Function* fun,
    PythonConvertState* state) const {
  RET_CHECK(fun->is_native());
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
  absl::flat_hash_map<std::string, std::string> arguments;
  absl::flat_hash_set<std::string> skipped;
  RET_CHECK(expression.function_binding()->call_expressions.size() ==
            expression.function_binding()->names.size());
  for (size_t i = 0; i < expression.function_binding()->names.size(); ++i) {
    std::string arg_name =
        absl::StrCat("${", expression.function_binding()->names[i], "}");
    if (!expression.function_binding()->call_expressions[i].has_value()) {
      skipped.emplace(std::move(arg_name));
      continue;
    }
    PythonConvertState expression_state(state, true);
    RETURN_IF_ERROR(ConvertExpression(
        *expression.function_binding()->call_expressions[i].value(),
        &expression_state));
    RETURN_IF_ERROR(expression_state.CheckInline(expression))
        << "For argument " << i << " : " << arg_name
        << " of inline native function " << fun->call_name();
    state->AddImports(expression_state);
    arguments.emplace(std::move(arg_name), expression_state.out().str());
  }
  std::string replaced = absl::StrReplaceAll(
      absl::StripAsciiWhitespace(py_inline_it->second), arguments);
  for (const auto& s : skipped) {
    if (absl::StrContains(replaced, s)) {
      return status::InvalidArgumentErrorBuilder()
             << "Argument: " << s
             << " for which we got no value in the call "
                "of native inline function "
             << fun->call_name()
             << " remains "
                "in result: `"
             << replaced << "`";
    }
  }
  state->out() << "(" << replaced << ")";
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertFunctionCallExpression(
    const FunctionCallExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  if (expression.function_binding()->fun.has_value() &&
      expression.function_binding()->fun.value()->is_native()) {
    return ConvertNativeFunctionCallExpression(
        expression, expression.function_binding()->fun.value(), bstate);
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
    Function* fun = expression.function_binding()->fun.value();
    if (bstate->module() != fun->module_scope()) {
      bstate->out() << scoped_name(fun->qualified_call_name());
    } else {
      bstate->out() << fun->call_name();
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
    bstate->out() << bstate->indent() << expression.function_binding()->names[i]
                  << "=";
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
    const ImportStatementExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->add_import(absl::StrCat(
      "import ", scope_name(expression.module()->scope_name()),
      (expression.is_alias() ? absl::StrCat(" as ", expression.local_name())
                             : "")));
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertFunctionDefinition(
    const FunctionDefinitionExpression& expression, ConvertState* state) const {
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
  if (FunctionGroup::IsFunctionGroup(*fun_object)) {
    auto function_group = static_cast<FunctionGroup*>(fun_object);
    for (Function* fun : function_group->functions()) {
      RETURN_IF_ERROR(ConvertFunction(fun, state));
    }
  } else if (Function::IsFunctionKind(*fun_object)) {
    RETURN_IF_ERROR(ConvertFunction(static_cast<Function*>(fun_object), state));
  }
  return ConvertFunction(expression.def_function(), state);
}

absl::Status PythonConverter::ConvertSchemaDefinition(
    const SchemaDefinitionExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->clear_inline();
  bstate->add_import("import dataclasses");
  auto ts = CHECK_NOTNULL(expression.def_schema());
  bstate->out() << std::endl
                << "@dataclasses.dataclass" << std::endl
                << "class " << ts->name() << ":" << std::endl;
  bstate->inc_indent();
  for (const auto& field : ts->fields()) {
    bstate->out() << bstate->indent() << field.name << ": ";
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
    const TypeDefinitionExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  bstate->clear_inline();
  bstate->out() << expression.type_name() << " = ";
  RETURN_IF_ERROR(AddTypeName(expression.defined_type_spec(), bstate))
      << "In typedef of " << expression.type_name();
  bstate->out() << std::endl;
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertBindings(Function* fun,
                                              ConvertState* state) const {
  for (const auto& binding : fun->bindings()) {
    RETURN_IF_ERROR(ConvertFunction(binding.get(), state));
  }
  return absl::OkStatus();
}

absl::Status PythonConverter::ConvertFunction(Function* fun,
                                              ConvertState* state) const {
  auto bstate = static_cast<PythonConvertState*>(state);
  auto superstate = bstate->top_superstate();
  if (!superstate->RegisterFunction(fun)) {
    return absl::OkStatus();  // Already converted
  }
  if (fun->is_native()) {
    return absl::OkStatus();  // these get converted directly
  }
  const bool is_lambda = fun->kind() == pb::ObjectKind::OBJ_LAMBDA;
  if (!fun->is_native() && fun->expressions().empty()) {  // && !is_lambda) {
    RETURN_IF_ERROR(ConvertBindings(fun, state));
    return absl::OkStatus();  // Untyped and unused function.
  }
  PythonConvertState local_state(superstate);
  auto& out = local_state.out();
  out << std::endl << "def " << fun->call_name() << "(" << std::endl;
  local_state.inc_indent(2);
  RET_CHECK(fun->arguments().size() == fun->default_values().size());
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      out << "," << std::endl;
    }
    const auto& arg = fun->arguments()[i];
    out << local_state.indent() << arg->name() << ": ";
    RETURN_IF_ERROR(AddTypeName(arg->original_type(), &local_state))
        << "In typedef of argument: " << arg->name() << " of "
        << fun->call_name();
    if (!is_lambda && fun->default_values()[i].has_value()) {
      out << " = ";
      RETURN_IF_ERROR(ConvertInlineExpression(*fun->default_values()[i].value(),
                                              &local_state));
    }
  }
  out << ") -> ";
  RETURN_IF_ERROR(AddTypeName(fun->result_type(), &local_state))
      << "In typedef of result type of " << fun->call_name();
  out << ":" << std::endl;
  local_state.dec_indent(2);
  if (fun->expressions().empty()) {
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
  superstate->out() << local_state.out().str();
  RETURN_IF_ERROR(ConvertBindings(fun, state));
  return absl::OkStatus();
}

}  // namespace analysis
}  // namespace nudl
