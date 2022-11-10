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

#include "nudl/conversion/pseudo_converter.h"

#include "nudl/status/status.h"

namespace nudl {
namespace conversion {

class PseudoConvertState : public ConvertState {
 public:
  explicit PseudoConvertState(analysis::Module* module,
                              size_t indent_delta = 2);
  explicit PseudoConvertState(PseudoConvertState* superstate);
  ~PseudoConvertState() override;

  // The buffer to which we output the code content.
  std::stringstream& out();

  // If this is a sub-state for code generation (that
  // would be appended later to this superstate).
  PseudoConvertState* superstate() const;
  // The top of the state tree. e.g. in a function that
  // define another function etc.
  PseudoConvertState* top_superstate() const;
  // Current indentation.
  const std::string& indent() const;
  // Advances the indentation.
  void inc_indent();
  // Reduces the indentation.
  void dec_indent();

  // Utility to writer a scoped name to out
  std::stringstream& write_name(const analysis::ScopedName& name);
  // Records that this function was processed.
  bool RegisterFunction(analysis::Function* fun);

  // Used to mark the current function call in order
  // to use the proper names in identifier & dot expressiosn.
  const analysis::Function* in_function_call() const;
  void push_in_function_call(const analysis::Function* fun);
  void pop_in_function_call();

  // On stack helper for maintaining the function call stacl.
  class FunctionPusher {
   public:
    FunctionPusher(PseudoConvertState* state, const analysis::Function* fun)
        : state_(CHECK_NOTNULL(state)) {
      state_->push_in_function_call(CHECK_NOTNULL(fun));
    }
    ~FunctionPusher() { state_->pop_in_function_call(); }

   private:
    PseudoConvertState* const state_;
  };

 protected:
  PseudoConvertState* const superstate_ = nullptr;
  const size_t indent_delta_;
  std::stringstream out_;
  size_t indent_ = 0;
  std::string indent_str_;
  absl::flat_hash_set<analysis::Function*> converted_functions_;
  std::vector<const analysis::Function*> in_function_call_;
};

PseudoConvertState::PseudoConvertState(analysis::Module* module,
                                       size_t indent_delta)
    : ConvertState(module), indent_delta_(indent_delta) {}

PseudoConvertState::PseudoConvertState(PseudoConvertState* superstate)
    : ConvertState(superstate->module()),
      superstate_(superstate),
      indent_delta_(superstate->indent_delta_) {}

PseudoConvertState::~PseudoConvertState() {}

const std::string& PseudoConvertState::indent() const { return indent_str_; }

std::stringstream& PseudoConvertState::out() { return out_; }

void PseudoConvertState::inc_indent() {
  indent_ += indent_delta_;
  indent_str_ += std::string(indent_delta_, ' ');
}

void PseudoConvertState::dec_indent() {
  CHECK_GE(indent_, indent_delta_);
  indent_ -= indent_delta_;
  indent_str_ = std::string(indent_, ' ');
}

PseudoConvertState* PseudoConvertState::superstate() const {
  return superstate_;
}

PseudoConvertState* PseudoConvertState::top_superstate() const {
  PseudoConvertState* s = superstate_;
  while (s) {
    if (!s->superstate()) {
      return s;
    }
    s = s->superstate();
  }
  return nullptr;
}

bool PseudoConvertState::RegisterFunction(analysis::Function* fun) {
  if (converted_functions_.contains(fun)) {
    return false;
  }
  converted_functions_.emplace(fun);
  return true;
}

std::stringstream& PseudoConvertState::write_name(
    const analysis::ScopedName& name) {
  out_ << name.full_name();
  return out_;
}

const analysis::Function* PseudoConvertState::in_function_call() const {
  if (in_function_call_.empty()) {
    if (superstate_) {
      return superstate_->in_function_call();
    }
    return nullptr;
  }
  return in_function_call_.back();
}

void PseudoConvertState::push_in_function_call(const analysis::Function* fun) {
  in_function_call_.emplace_back(CHECK_NOTNULL(fun));
  CHECK(in_function_call() == fun);
}

void PseudoConvertState::pop_in_function_call() {
  CHECK(!in_function_call_.empty());
  in_function_call_.pop_back();
}

PseudoConverter::PseudoConverter() : Converter() {}

absl::StatusOr<std::unique_ptr<ConvertState>> PseudoConverter::BeginModule(
    analysis::Module* module) const {
  return std::make_unique<PseudoConvertState>(module);
}

absl::StatusOr<std::string> PseudoConverter::FinishModule(
    analysis::Module* module, std::unique_ptr<ConvertState> state) const {
  auto bstate = static_cast<PseudoConvertState*>(state.get());
  return {bstate->out().str()};
}

absl::Status PseudoConverter::ProcessModule(analysis::Module* module,
                                            ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  for (const auto& expression : module->expressions()) {
    PseudoConvertState expression_state(bstate);
    RETURN_IF_ERROR(ConvertExpression(*expression.get(), &expression_state));
    bstate->out() << expression_state.out().str() << std::endl;
  }
  return absl::OkStatus();
}

std::string PseudoConverter::GetTypeString(const analysis::TypeSpec* type_spec,
                                           PseudoConvertState* state) const {
  std::string prefix;
  if (!type_spec->scope_name().empty() &&
      type_spec->scope_name().name() != state->module()->name()) {
    // TODO(catalin): treat for alias imports
    // i.e. type_spec->scope_name().name() refers to original name
    prefix = absl::StrCat(type_spec->scope_name().name(), ".");
  }
  if (type_spec->type_id() == pb::TypeId::STRUCT_ID) {
    return absl::StrCat(prefix, type_spec->name());
  }
  // May want something deeper, but for now:
  return absl::StrCat(prefix, type_spec->full_name());
}

absl::Status PseudoConverter::ConvertAssignment(
    const analysis::Assignment& expression, ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  bstate->write_name(expression.name());
  if (expression.has_type_spec()) {
    bstate->out() << " : "
                  << GetTypeString(expression.var()->converted_type(), bstate);
  }
  bstate->out() << " = ";
  RET_CHECK(!expression.children().empty());
  return ConvertExpression(*expression.children().front(), state);
}
absl::Status PseudoConverter::ConvertEmptyStruct(
    const analysis::EmptyStruct& expression, ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  if (expression.stored_type_spec().has_value() &&
      (expression.stored_type_spec().value()->type_id() ==
       pb::TypeId::SET_ID)) {
    bstate->out() << "set()";
  } else {
    bstate->out() << "[]";
  }
  return absl::OkStatus();
}
absl::Status PseudoConverter::ConvertLiteral(
    const analysis::Literal& expression, ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  // TODO(catalin): may want to do more here, but for now is fine:
  bstate->out() << expression.DebugString();
  return absl::OkStatus();
}

namespace {
absl::optional<const analysis::Function*> GetFunctionBinding(
    const analysis::Expression& expression, PseudoConvertState* state) {
  const analysis::Function* check_fun = state->in_function_call();
  if (!check_fun) {
    return {};
  }
  auto named_obj = expression.named_object();
  if (!named_obj.has_value()) {
    return {};
  }
  if (analysis::Function::IsFunctionKind(*named_obj.value())) {
    auto fun = static_cast<const analysis::Function*>(named_obj.value());
    if (!fun->IsBinding(check_fun)) {
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

absl::Status PseudoConverter::ConvertIdentifier(
    const analysis::Identifier& expression, ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  auto binding = GetFunctionBinding(expression, bstate);
  if (binding.has_value()) {
    analysis::ScopedName local_name(expression.scoped_name().scope_name_ptr(),
                                    binding.value()->call_name());
    bstate->write_name(local_name);
  } else {
    bstate->write_name(expression.scoped_name());
  }
  return absl::OkStatus();
}
absl::Status PseudoConverter::ConvertFunctionResult(
    const analysis::FunctionResultExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  switch (expression.result_kind()) {
    case pb::FunctionResultKind::RESULT_NONE:
      return absl::InvalidArgumentError(
          "Should not end up with a NONE result kind in a function result"
          "expression");
    case pb::FunctionResultKind::RESULT_RETURN:
      bstate->out() << "return ";
      RET_CHECK(!expression.children().empty());
      return ConvertExpression(*expression.children().front(), state);
    case pb::FunctionResultKind::RESULT_YIELD:
      bstate->out() << "yield ";
      RET_CHECK(!expression.children().empty());
      return ConvertExpression(*expression.children().front(), state);
    case pb::FunctionResultKind::RESULT_PASS:
      bstate->out() << "pass";
      return absl::OkStatus();
  }
  return absl::OkStatus();
}
absl::Status PseudoConverter::ConvertArrayDefinition(
    const analysis::ArrayDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  auto type_spec = expression.stored_type_spec();
  const bool is_set = type_spec.has_value() &&
                      type_spec.value()->type_id() == pb::TypeId::SET_ID;
  bstate->out() << (is_set ? "{" : "[") << std::endl;
  bool is_first = true;
  bstate->inc_indent();
  bstate->inc_indent();
  for (const auto& expr : expression.children()) {
    if (is_first) {
      is_first = false;
    } else {
      bstate->out() << "," << std::endl;
    }
    bstate->out() << bstate->indent();
    RETURN_IF_ERROR(ConvertExpression(*expr, state));
  }
  bstate->out() << (is_set ? "}" : "]");
  bstate->dec_indent();
  bstate->dec_indent();
  return absl::OkStatus();
}
absl::Status PseudoConverter::ConvertMapDefinition(
    const analysis::MapDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  bstate->out() << "{";
  bool is_first = true;
  bool last_key = false;
  for (const auto& expr : expression.children()) {
    if (last_key) {
      bstate->out() << ": ";
      last_key = false;
    } else {
      if (is_first) {
        is_first = false;
      } else {
        bstate->out() << ", ";
      }
      last_key = true;
    }
    RETURN_IF_ERROR(ConvertExpression(*expr, state));
  }
  bstate->out() << "}";
  return absl::OkStatus();
}

absl::Status PseudoConverter::ConvertTupleDefinition(
    const analysis::TupleDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  bstate->out() << "{";
  expression.CheckSizes();
  bstate->inc_indent();
  bstate->inc_indent();
  for (size_t i = 0; i < expression.names().size(); ++i) {
    if (i) {
      bstate->out() << "," << std::endl;
    }
    bstate->out() << bstate->indent() << expression.names()[i];
    if (expression.types()[i].has_value()) {
      bstate->out() << ": " << expression.types()[i].value()->full_name();
    }
    bstate->out() << " = ";
    RETURN_IF_ERROR(ConvertExpression(*expression.children()[i], state));
  }
  bstate->out() << "}";
  bstate->dec_indent();
  bstate->dec_indent();
  return absl::OkStatus();
}

absl::Status PseudoConverter::ConvertIfExpression(
    const analysis::IfExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
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
    RETURN_IF_ERROR(ConvertExpression(*expression.condition()[i], bstate));
    bstate->out() << std::endl;
    RETURN_IF_ERROR(ConvertExpression(*expression.expression()[i], bstate));
  }
  if (expression.expression().size() > expression.condition().size()) {
    bstate->out() << bstate->indent() << "else:" << std::endl;
    RETURN_IF_ERROR(ConvertExpression(*expression.expression().back(), state));
  }
  return absl::OkStatus();
}
absl::Status PseudoConverter::ConvertExpressionBlock(
    const analysis::ExpressionBlock& expression, ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  bstate->inc_indent();
  for (const auto& expression : expression.children()) {
    bstate->out() << bstate->indent();
    RETURN_IF_ERROR(ConvertExpression(*expression, state));
    bstate->out() << std::endl;
  }
  bstate->dec_indent();
  return absl::OkStatus();
}
absl::Status PseudoConverter::ConvertIndexExpression(
    const analysis::IndexExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  RET_CHECK(expression.children().size() == 2);
  RETURN_IF_ERROR(ConvertExpression(*expression.children().front(), state));
  bstate->out() << "[";
  RETURN_IF_ERROR(ConvertExpression(*expression.children().back(), state));
  bstate->out() << "]";
  return absl::OkStatus();
}
absl::Status PseudoConverter::ConvertTupleIndexExpression(
    const analysis::TupleIndexExpression& expression,
    ConvertState* state) const {
  return ConvertIndexExpression(expression, state);
}
absl::Status PseudoConverter::ConvertLambdaExpression(
    const analysis::LambdaExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  RETURN_IF_ERROR(ConvertFunction(expression.lambda_function(), bstate));
  bstate->out() << "lambda ";
  RET_CHECK(expression.named_object().has_value());
  auto obj = expression.named_object().value();
  analysis::Function* fun = nullptr;
  if (analysis::FunctionGroup::IsFunctionGroup(*obj)) {
    fun = expression.lambda_function();
  } else {
    RET_CHECK(analysis::Function::IsFunctionKind(*obj));
    fun = static_cast<analysis::Function*>(obj);
  }
  RET_CHECK(fun->arguments().size() == fun->default_values().size());
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      bstate->out() << ", ";
    }
    const auto& arg = fun->arguments()[i];
    bstate->out() << arg->name() << ": "
                  << GetTypeString(arg->converted_type(), bstate);
    if (fun->default_values()[i].has_value()) {
      bstate->out() << " = ";
      RETURN_IF_ERROR(
          ConvertExpression(*fun->default_values()[i].value(), bstate));
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

absl::Status PseudoConverter::ConvertDotAccessExpression(
    const analysis::DotAccessExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  RET_CHECK(expression.children().size() == 1);
  RETURN_IF_ERROR(ConvertExpression(*expression.children().front(), state));
  bstate->out() << ".";
  auto binding = GetFunctionBinding(expression, bstate);
  if (binding.has_value()) {
    bstate->out() << binding.value()->call_name();
  } else {
    bstate->out() << expression.name().name();
  }
  return absl::OkStatus();
}

absl::Status PseudoConverter::ConvertFunctionCallExpression(
    const analysis::FunctionCallExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  if (expression.left_expression().has_value() &&
      !expression.is_method_call()) {
    if (expression.function_binding()->fun.has_value()) {
      PseudoConvertState::FunctionPusher pusher(
          bstate, expression.function_binding()->fun.value());
      RETURN_IF_ERROR(
          ConvertExpression(*expression.left_expression().value(), state));
    } else {
      RETURN_IF_ERROR(
          ConvertExpression(*expression.left_expression().value(), state));
    }
  } else {
    RET_CHECK(expression.function_binding()->fun.has_value());
    analysis::Function* fun = expression.function_binding()->fun.value();
    if (bstate->module() != fun->module_scope()) {
      bstate->out() << fun->qualified_call_name().full_name();
    } else {
      bstate->out() << fun->call_name();
    }
  }
  // TODO(catalin): this in practice is a bit more complicated, but
  //  we do something simple.
  RET_CHECK(expression.function_binding()->call_expressions.size() ==
            expression.function_binding()->names.size());
  bstate->out() << "(\n";
  bstate->inc_indent();
  bstate->inc_indent();
  for (size_t i = 0; i < expression.function_binding()->names.size(); ++i) {
    if (!expression.function_binding()->call_expressions[i].has_value()) {
      continue;
    }
    if (i > 0) {
      bstate->out() << "," << std::endl;
    }
    bstate->out() << bstate->indent() << expression.function_binding()->names[i]
                  << "=";
    // TODO(catalin): note - this may convert the default expressions
    // as well, which may not be valid in this scope - will want to massage
    // this a bit
    RETURN_IF_ERROR(ConvertExpression(
        *expression.function_binding()->call_expressions[i].value(), state));
  }
  bstate->out() << ")";
  bstate->dec_indent();
  bstate->dec_indent();
  return absl::OkStatus();
}

absl::Status PseudoConverter::ConvertImportStatement(
    const analysis::ImportStatementExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  bstate->out() << "import " << expression.module()->module_name();
  if (expression.is_alias()) {
    bstate->out() << " as " << expression.local_name();
  }
  return absl::OkStatus();
}

absl::Status PseudoConverter::ConvertFunctionDefinition(
    const analysis::FunctionDefinitionExpression& expression,
    ConvertState* state) const {
  // Here we need to lookup the module to all functions w/ this name
  // and convert them all:
  ASSIGN_OR_RETURN(auto fun_object,
                   state->module()->GetName(
                       expression.def_function()->function_name(), true));
  if (analysis::FunctionGroup::IsFunctionGroup(*fun_object)) {
    auto function_group = static_cast<analysis::FunctionGroup*>(fun_object);
    for (analysis::Function* fun : function_group->functions()) {
      RETURN_IF_ERROR(ConvertFunction(fun, state));
    }
  } else if (analysis::Function::IsFunctionKind(*fun_object)) {
    RETURN_IF_ERROR(
        ConvertFunction(static_cast<analysis::Function*>(fun_object), state));
  }
  return ConvertFunction(expression.def_function(), state);
}

absl::Status PseudoConverter::ConvertSchemaDefinition(
    const analysis::SchemaDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  auto ts = CHECK_NOTNULL(expression.def_schema());
  bstate->out() << "schema " << ts->name() << " = {" << std::endl;
  bstate->inc_indent();
  for (const auto& field : ts->fields()) {
    bstate->out() << bstate->indent() << field.name << ": "
                  << GetTypeString(field.type_spec, bstate) << ";" << std::endl;
  }
  bstate->out() << "}" << std::endl;
  return absl::OkStatus();
}

absl::Status PseudoConverter::ConvertTypeDefinition(
    const analysis::TypeDefinitionExpression& expression,
    ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  bstate->out() << "typedef " << expression.type_name() << " = "
                << GetTypeString(expression.defined_type_spec(), bstate)
                << std::endl;
  return absl::OkStatus();
}

absl::Status PseudoConverter::ConvertBindings(analysis::Function* fun,
                                              ConvertState* state) const {
  for (const auto& binding : fun->bindings()) {
    RETURN_IF_ERROR(ConvertFunction(binding.get(), state));
  }
  return absl::OkStatus();
}

absl::Status PseudoConverter::ConvertFunction(analysis::Function* fun,
                                              ConvertState* state) const {
  auto bstate = static_cast<PseudoConvertState*>(state);
  auto superstate = bstate->top_superstate();
  if (!superstate->RegisterFunction(fun)) {
    return absl::OkStatus();  // Already converted
  }
  const bool is_lambda = fun->kind() == pb::ObjectKind::OBJ_LAMBDA;
  if (!fun->is_native() && fun->expressions().empty()) {  // && !is_lambda) {
    RETURN_IF_ERROR(ConvertBindings(fun, state));
    return absl::OkStatus();  // Untyped and unused function.
  }
  PseudoConvertState local_state(superstate);
  auto& out = local_state.out();
  out << "def " << fun->call_name() << "(" << std::endl;
  local_state.inc_indent();
  local_state.inc_indent();
  RET_CHECK(fun->arguments().size() == fun->default_values().size());
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      out << "," << std::endl;
    }
    const auto& arg = fun->arguments()[i];
    out << local_state.indent() << arg->name() << ": "
        << GetTypeString(arg->converted_type(), &local_state);
    if (!is_lambda && fun->default_values()[i].has_value()) {
      out << " = ";
      RETURN_IF_ERROR(
          ConvertExpression(*fun->default_values()[i].value(), &local_state));
    }
  }
  out << ") : " << GetTypeString(fun->result_type(), bstate) << " {"
      << std::endl;
  local_state.dec_indent();
  local_state.dec_indent();
  if (fun->is_native()) {
    for (const auto& it : fun->native_impl()) {
      out << "[[" << it.first << "]]" << std::endl
          << it.second << std::endl
          << "[[end]]" << std::endl;
    }
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
  out << "}" << std::endl;
  superstate->out() << local_state.out().str();
  RETURN_IF_ERROR(ConvertBindings(fun, state));
  return absl::OkStatus();
}

}  // namespace conversion
}  // namespace nudl
