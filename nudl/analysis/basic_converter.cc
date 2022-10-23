#include "nudl/analysis/basic_converter.h"

#include "nudl/status/status.h"

namespace nudl {
namespace analysis {

BasicConvertState::BasicConvertState(Module* module, size_t indent_delta)
    : ConvertState(module), indent_delta_(indent_delta) {}

BasicConvertState::BasicConvertState(BasicConvertState* superstate)
    : ConvertState(superstate->module()),
      superstate_(superstate),
      indent_delta_(superstate->indent_delta_) {}

BasicConvertState::~BasicConvertState() {}

const std::string& BasicConvertState::indent() const { return indent_str_; }

std::stringstream& BasicConvertState::out() { return out_; }

void BasicConvertState::inc_indent() {
  indent_ += indent_delta_;
  indent_str_ += std::string(indent_delta_, ' ');
}

void BasicConvertState::dec_indent() {
  CHECK_GE(indent_, indent_delta_);
  indent_ -= indent_delta_;
  indent_str_ = std::string(indent_, ' ');
}

BasicConvertState* BasicConvertState::superstate() const { return superstate_; }

BasicConvertState* BasicConvertState::top_superstate() const {
  BasicConvertState* s = superstate_;
  while (s) {
    if (!s->superstate()) {
      return s;
    }
    s = s->superstate();
  }
  return nullptr;
}

bool BasicConvertState::RegisterFunction(Function* fun) {
  if (converted_functions_.contains(fun)) {
    return false;
  }
  converted_functions_.emplace(fun);
  return true;
}

std::stringstream& BasicConvertState::write_name(const ScopedName& name) {
  out_ << name.full_name();
  return out_;
}

const Function* BasicConvertState::in_function_call() const {
  if (in_function_call_.empty()) {
    return nullptr;
  }
  return in_function_call_.back();
}

void BasicConvertState::push_in_function_call(const Function* fun) {
  in_function_call_.emplace_back(CHECK_NOTNULL(fun));
  CHECK(in_function_call() == fun);
}

void BasicConvertState::pop_in_function_call() {
  CHECK(!in_function_call_.empty());
  in_function_call_.pop_back();
}

BasicConverter::BasicConverter() : Converter() {}

absl::StatusOr<std::unique_ptr<ConvertState>> BasicConverter::BeginModule(
    Module* module) const {
  return std::make_unique<BasicConvertState>(module);
}

absl::StatusOr<std::string> BasicConverter::FinishModule(
    Module* module, std::unique_ptr<ConvertState> state) const {
  auto bstate = static_cast<BasicConvertState*>(state.get());
  return {bstate->out().str()};
}

absl::Status BasicConverter::ProcessModule(Module* module,
                                           ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  for (const auto& expression : module->expressions()) {
    BasicConvertState expression_state(bstate);
    RETURN_IF_ERROR(ConvertExpression(*expression.get(), &expression_state));
    bstate->out() << expression_state.out().str() << std::endl;
  }
  return absl::OkStatus();
}

std::string BasicConverter::GetTypeString(const TypeSpec* type_spec,
                                          BasicConvertState* state) const {
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

absl::Status BasicConverter::ConvertAssignment(const Assignment& expression,
                                               ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  bstate->write_name(expression.name());
  if (expression.has_type_spec()) {
    bstate->out() << " : "
                  << GetTypeString(expression.var()->original_type(), bstate);
  }
  bstate->out() << " = ";
  RET_CHECK(!expression.children().empty());
  return ConvertExpression(*expression.children().front(), state);
}
absl::Status BasicConverter::ConvertEmptyStruct(const EmptyStruct& expression,
                                                ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  if (expression.stored_type_spec().has_value() &&
      (expression.stored_type_spec().value()->type_id() ==
       pb::TypeId::SET_ID)) {
    bstate->out() << "set()";
  } else {
    bstate->out() << "[]";
  }
  return absl::OkStatus();
}
absl::Status BasicConverter::ConvertLiteral(const Literal& expression,
                                            ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  // TODO(catalin): may want to do more here, but for now is fine:
  bstate->out() << expression.str_value();
  return absl::OkStatus();
}

namespace {
absl::optional<const Function*> GetFunctionBinding(const Expression& expression,
                                                   BasicConvertState* state) {
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

absl::Status BasicConverter::ConvertIdentifier(const Identifier& expression,
                                               ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  auto binding = GetFunctionBinding(expression, bstate);
  if (binding.has_value()) {
    ScopedName local_name(expression.scoped_name().scope_name_ptr(),
                          binding.value()->call_name());
    bstate->write_name(local_name);
  } else {
    bstate->write_name(expression.scoped_name());
  }
  return absl::OkStatus();
}
absl::Status BasicConverter::ConvertFunctionResult(
    const FunctionResultExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
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
absl::Status BasicConverter::ConvertArrayDefinition(
    const ArrayDefinitionExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
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
absl::Status BasicConverter::ConvertMapDefinition(
    const MapDefinitionExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
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
absl::Status BasicConverter::ConvertIfExpression(const IfExpression& expression,
                                                 ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
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
absl::Status BasicConverter::ConvertExpressionBlock(
    const ExpressionBlock& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  bstate->inc_indent();
  for (const auto& expression : expression.children()) {
    bstate->out() << bstate->indent();
    RETURN_IF_ERROR(ConvertExpression(*expression, state));
    bstate->out() << std::endl;
  }
  bstate->dec_indent();
  return absl::OkStatus();
}
absl::Status BasicConverter::ConvertIndexExpression(
    const IndexExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  RET_CHECK(expression.children().size() == 2);
  RETURN_IF_ERROR(ConvertExpression(*expression.children().front(), state));
  bstate->out() << "[";
  RETURN_IF_ERROR(ConvertExpression(*expression.children().back(), state));
  bstate->out() << "]";
  return absl::OkStatus();
}
absl::Status BasicConverter::ConvertTupleIndexExpression(
    const TupleIndexExpression& expression, ConvertState* state) const {
  return ConvertIndexExpression(expression, state);
}
absl::Status BasicConverter::ConvertLambdaExpression(
    const LambdaExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  RETURN_IF_ERROR(ConvertFunction(expression.lambda_function(), bstate));
  bstate->out() << "lambda ";
  Function* fun = static_cast<Function*>(expression.named_object().value());
  RET_CHECK(fun->arguments().size() == fun->default_values().size());
  for (size_t i = 0; i < fun->arguments().size(); ++i) {
    if (i > 0) {
      bstate->out() << ", ";
    }
    const auto& arg = fun->arguments()[i];
    bstate->out() << arg->name() << ": "
                  << GetTypeString(arg->original_type(), bstate);
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

absl::Status BasicConverter::ConvertDotAccessExpression(
    const DotAccessExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
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

absl::Status BasicConverter::ConvertFunctionCallExpression(
    const FunctionCallExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  if (expression.left_expression().has_value() &&
      !expression.is_method_call()) {
    if (expression.function_binding()->fun.has_value()) {
      BasicConvertState::FunctionPusher pusher(
          bstate, expression.function_binding()->fun.value());
      RETURN_IF_ERROR(
          ConvertExpression(*expression.left_expression().value(), state));
    } else {
      RETURN_IF_ERROR(
          ConvertExpression(*expression.left_expression().value(), state));
    }
  } else {
    RET_CHECK(expression.function_binding()->fun.has_value());
    Function* fun = expression.function_binding()->fun.value();
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

absl::Status BasicConverter::ConvertImportStatement(
    const ImportStatementExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  bstate->out() << "import " << expression.module()->module_name();
  if (expression.is_alias()) {
    bstate->out() << " as " << expression.local_name();
  }
  return absl::OkStatus();
}

absl::Status BasicConverter::ConvertFunctionDefinition(
    const FunctionDefinitionExpression& expression, ConvertState* state) const {
  // Here we need to lookup the module to all functions w/ this name
  // and convert them all:
  ASSIGN_OR_RETURN(
      auto fun_object,
      state->module()->GetName(expression.def_function()->function_name()));
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

absl::Status BasicConverter::ConvertSchemaDefinition(
    const SchemaDefinitionExpression& expression, ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
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

absl::Status BasicConverter::ConvertBindings(Function* fun,
                                             ConvertState* state) const {
  for (const auto& binding : fun->bindings()) {
    RETURN_IF_ERROR(ConvertFunction(binding.get(), state));
  }
  return absl::OkStatus();
}

absl::Status BasicConverter::ConvertFunction(Function* fun,
                                             ConvertState* state) const {
  auto bstate = static_cast<BasicConvertState*>(state);
  auto superstate = bstate->top_superstate();
  if (!superstate->RegisterFunction(fun)) {
    return absl::OkStatus();  // Already converted
  }
  const bool is_lambda = fun->kind() == pb::ObjectKind::OBJ_LAMBDA;
  if (!fun->is_native() && fun->expressions().empty()) {  // && !is_lambda) {
    RETURN_IF_ERROR(ConvertBindings(fun, state));
    return absl::OkStatus();  // Untyped and unused function.
  }
  BasicConvertState local_state(superstate);
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
        << GetTypeString(arg->original_type(), &local_state);
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
      out << "$$" << it.first << std::endl
          << it.second << std::endl
          << "%%end" << std::endl;
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

}  // namespace analysis
}  // namespace nudl
