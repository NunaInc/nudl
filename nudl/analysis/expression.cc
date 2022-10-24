#include "nudl/analysis/expression.h"

#include <typeindex>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "glog/logging.h"
#include "nudl/analysis/function.h"
#include "nudl/analysis/module.h"
#include "nudl/analysis/scope.h"
#include "nudl/analysis/types.h"
#include "nudl/analysis/vars.h"
#include "nudl/status/status.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);

namespace nudl {
namespace analysis {

Expression::Expression(Scope* scope) : scope_(CHECK_NOTNULL(scope)) {}

Expression::~Expression() {
  while (!children_.empty()) {
    children_.pop_back();
  }
}

std::any Expression::StaticValue() const { return std::any(); }

absl::optional<const TypeSpec*> Expression::stored_type_spec() const {
  return type_spec_;
}

absl::StatusOr<const TypeSpec*> Expression::type_spec(
    absl::optional<const TypeSpec*> type_hint) {
  if (!type_spec_.has_value() ||
      (type_hint.has_value() &&
       (!type_hint_.has_value() ||
        !type_hint_.value()->IsEqual(*type_hint.value())))) {
    ASSIGN_OR_RETURN(type_spec_, NegotiateType(type_hint));
    type_hint_ = type_hint;
  }
  return CHECK_NOTNULL(type_spec_.value());
}

const std::vector<std::unique_ptr<Expression>>& Expression::children() const {
  return children_;
}

absl::optional<NamedObject*> Expression::named_object() const {
  return named_object_;
}

void Expression::set_named_object(NamedObject* object) {
  named_object_ = CHECK_NOTNULL(object);
}

bool Expression::ContainsFunctionExit() const { return false; }

pb::ExpressionSpec Expression::ToProto() const {
  pb::ExpressionSpec expression;
  expression.set_kind(expr_kind());
  for (const auto& child : children()) {
    *expression.add_child() = child->ToProto();
  }
  if (absl::GetFlag(FLAGS_nudl_short_analysis_proto)) {
    return expression;
  }
  if (type_spec_.has_value()) {
    *expression.mutable_type_spec() = type_spec_.value()->ToProto();
  }
  auto object = named_object();
  if (object.has_value()) {
    *expression.mutable_named_object() = object.value()->ToProtoRef();
  }
  return expression;
}

NopExpression::NopExpression(Scope* scope,
                             absl::optional<std::unique_ptr<Expression>> child)
    : Expression(scope) {
  if (child.has_value()) {
    children_.emplace_back(std::move(CHECK_NOTNULL(child)).value());
  }
}

pb::ExpressionKind NopExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_NOP;
}

std::string NopExpression::DebugString() const {
  return absl::StrCat(
      "NOP{", (children_.empty() ? "" : children_.front()->DebugString()), "}");
}

absl::StatusOr<const TypeSpec*> NopExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return TypeUnknown::Instance();
}

Assignment::Assignment(Scope* scope, ScopedName name, VarBase* var,
                       std::unique_ptr<Expression> value, bool has_type_spec,
                       bool is_initial_assignment)
    : Expression(scope),
      name_(std::move(name)),
      var_(CHECK_NOTNULL(var)),
      has_type_spec_(has_type_spec),
      is_initial_assignment_(is_initial_assignment) {
  type_spec_ = CHECK_NOTNULL(var->type_spec());
  children_.emplace_back(std::move(value));
  // We expect that the value is already assigned to var.
  CHECK(!var->assignments().empty())
      << " For: " << var->full_name() << " in: " << scope->full_name()
      << kBugNotice;
  CHECK_EQ(var->assignments().back(), children_.front().get())
      << " For: " << var->full_name() << " in: " << scope->full_name()
      << kBugNotice;
}

const ScopedName& Assignment::name() const { return name_; }

VarBase* Assignment::var() const { return var_; }

bool Assignment::has_type_spec() const { return has_type_spec_; }

bool Assignment::is_initial_assignment() const {
  return is_initial_assignment_;
}

pb::ExpressionKind Assignment::expr_kind() const {
  return pb::ExpressionKind::EXPR_ASSIGNMENT;
}

absl::StatusOr<const TypeSpec*> Assignment::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  // This should already have this set, per constructor, and we cannot change:
  RET_CHECK(type_spec_.has_value());
  return type_spec_.value();
}

absl::optional<NamedObject*> Assignment::named_object() const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  return var_;
}

std::string Assignment::DebugString() const {
  return absl::StrCat(name_.full_name(), ": ",
                      var_->original_type()->full_name(), " = ",
                      children_.front()->DebugString());
}

EmptyStruct::EmptyStruct(Scope* scope) : Expression(scope) {}

pb::ExpressionKind EmptyStruct::expr_kind() const {
  return pb::ExpressionKind::EXPR_EMPTY_STRUCT;
}

std::string EmptyStruct::DebugString() const { return "[]"; }

absl::StatusOr<const TypeSpec*> EmptyStruct::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  if (!type_hint.has_value()) {
    return status::InvalidArgumentErrorBuilder()
           << "Empty iterable [] expression needs to have a type "
              "specification associated";
    ASSIGN_OR_RETURN(auto type_spec, scope_->FindTypeByName("Iterable<Any>"),
                     _ << "Finding standard type" << kBugNotice);
    return type_spec;
  }
  RET_CHECK(type_hint.value() != nullptr) << kBugNotice;
  ASSIGN_OR_RETURN(auto iterable_type, scope_->FindTypeByName("Iterable"),
                   _ << "Finding standard type" << kBugNotice);
  if (!iterable_type->IsAncestorOf(*type_hint.value())) {
    return status::InvalidArgumentErrorBuilder()
           << "Empty iterable [] cannot be coerced into a "
           << type_hint.value()->full_name();
  }
  return type_hint.value();
}

absl::Status Literal::CheckType(const TypeSpec* type_spec,
                                const std::any& value) {
  static auto kTypeSpec =
      new absl::flat_hash_map<size_t, const std::type_index>({
          {pb::TypeId::NULL_ID, std::type_index(typeid(void*))},
          {pb::TypeId::INT_ID, std::type_index(typeid(int64_t))},
          {pb::TypeId::INT8_ID, std::type_index(typeid(int64_t))},
          {pb::TypeId::INT16_ID, std::type_index(typeid(int64_t))},
          {pb::TypeId::INT32_ID, std::type_index(typeid(int64_t))},
          {pb::TypeId::UINT_ID, std::type_index(typeid(uint64_t))},
          {pb::TypeId::UINT8_ID, std::type_index(typeid(uint64_t))},
          {pb::TypeId::UINT16_ID, std::type_index(typeid(uint64_t))},
          {pb::TypeId::UINT32_ID, std::type_index(typeid(uint64_t))},
          {pb::TypeId::STRING_ID, std::type_index(typeid(std::string))},
          {pb::TypeId::BYTES_ID, std::type_index(typeid(std::string))},
          {pb::TypeId::BOOL_ID, std::type_index(typeid(bool))},
          {pb::TypeId::FLOAT32_ID, std::type_index(typeid(float))},
          {pb::TypeId::FLOAT64_ID, std::type_index(typeid(double))},
          {pb::TypeId::TIMEINTERVAL_ID,
           std::type_index(typeid(absl::Duration))},
      });
  auto it = kTypeSpec->find(type_spec->type_id());
  if (it == kTypeSpec->end()) {
    return status::UnimplementedErrorBuilder()
           << "Cannot have a type literal: " << type_spec->full_name();
  }
  if (it->second != std::type_index(value.type())) {
    return status::InvalidArgumentErrorBuilder()
           << "A value of C++ type: " << value.type().name()
           << " was created for: " << type_spec->full_name()
           << " - expecting: " << it->second.name();
  }
  return absl::OkStatus();
}

Literal::Literal(Scope* scope, const TypeSpec* type_spec, std::any value,
                 std::string str_value)
    : Expression(scope),
      build_type_spec_(CHECK_NOTNULL(type_spec)),
      value_(std::move(value)),
      str_value_(str_value) {
  CHECK_OK(CheckType(type_spec, value_));
  type_spec_ = type_spec;
}

const TypeSpec* Literal::build_type_spec() const { return build_type_spec_; }

const std::any& Literal::value() const { return value_; }

const std::string& Literal::str_value() const { return str_value_; }

std::any Literal::StaticValue() const { return value(); }

pb::ExpressionKind Literal::expr_kind() const {
  return pb::ExpressionKind::EXPR_LITERAL;
}

std::string Literal::DebugString() const { return str_value_; }

pb::ExpressionSpec Literal::ToProto() const {
  auto proto = Expression::ToProto();
  switch (build_type_spec_->type_id()) {
    case pb::TypeId::NULL_ID:
      proto.mutable_literal()->set_null_value(pb::NullType::NULL_VALUE);
      break;
    case pb::TypeId::INT_ID:
      proto.mutable_literal()->set_int_value(std::any_cast<int64_t>(value_));
      break;
    case pb::TypeId::UINT_ID:
      proto.mutable_literal()->set_uint_value(std::any_cast<uint64_t>(value_));
      break;
    case pb::TypeId::STRING_ID:
      proto.mutable_literal()->set_str_value(
          std::any_cast<std::string>(value_));
      break;
    case pb::TypeId::BYTES_ID:
      proto.mutable_literal()->set_bytes_value(
          std::any_cast<std::string>(value_));
      break;
    case pb::TypeId::BOOL_ID:
      proto.mutable_literal()->set_bool_value(std::any_cast<bool>(value_));
      break;
    case pb::TypeId::FLOAT32_ID:
      proto.mutable_literal()->set_float_value(std::any_cast<float>(value_));
      break;
    case pb::TypeId::FLOAT64_ID:
      proto.mutable_literal()->set_double_value(std::any_cast<double>(value_));
      break;
    case pb::TypeId::TIMEINTERVAL_ID:
      proto.mutable_literal()->mutable_time_range()->set_seconds(
          absl::ToInt64Seconds(std::any_cast<absl::Duration>(value_)));
      break;
    default:
      LOG(FATAL) << "Invalid type id for literal: "
                 << type_spec_.value()->full_name() << kBugNotice;
  }
  return proto;
}

absl::StatusOr<const TypeSpec*> Literal::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  if (!type_hint.has_value()) {
    return build_type_spec_;
  }
  RET_CHECK(type_hint.value() != nullptr) << kBugNotice;
  if (type_hint.value()->IsAncestorOf(*build_type_spec_)) {
    return build_type_spec_;
  }
  // TODO(catalin): we may want to do something more special
  // and support more types.
  if (!type_hint.value()->IsConvertibleFrom(*build_type_spec_)) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot coerce a literal of type: "
           << build_type_spec_->full_name()
           << " into a: " << type_hint.value()->full_name();
  }
  return type_hint.value();
}

absl::StatusOr<std::unique_ptr<Literal>> Literal::Build(
    Scope* scope, const pb::Literal& value) {
  std::any literal_value;
  const TypeSpec* type_spec = nullptr;
  if (value.has_null_value()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameNull),
                     _ << "Cannot find standard type" << kBugNotice);
    literal_value = std::make_any<void*>(nullptr);
  } else if (value.has_str_value()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameString),
                     _ << "Cannot find standard type" << kBugNotice);
    // TODO(catalin): we need a UTF8 string checker.
    literal_value = std::make_any<std::string>(value.str_value());
  } else if (value.has_bytes_value()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameBytes),
                     _ << "Cannot find standard type" << kBugNotice);
    literal_value = std::make_any<std::string>(value.bytes_value());
  } else if (value.has_int_value()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameInt),
                     _ << "Cannot find standard type" << kBugNotice);
    literal_value = std::make_any<int64_t>(value.int_value());
  } else if (value.has_uint_value()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameUInt),
                     _ << "Cannot find standard type" << kBugNotice);
    literal_value = std::make_any<uint64_t>(value.uint_value());
  } else if (value.has_double_value()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameFloat64),
                     _ << "Cannot find standard type" << kBugNotice);
    literal_value = std::make_any<double>(value.double_value());
  } else if (value.has_float_value()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameFloat32),
                     _ << "Cannot find standard type" << kBugNotice);
    literal_value = std::make_any<float>(value.float_value());
  } else if (value.has_bool_value()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameBool),
                     _ << "Cannot find standard type" << kBugNotice);
    literal_value = std::make_any<bool>(value.bool_value());
  } else if (value.has_time_range()) {
    ASSIGN_OR_RETURN(type_spec, scope->FindTypeByName(kTypeNameTimeInterval),
                     _ << "Cannot find standard type" << kBugNotice);
    literal_value = std::make_any<absl::Duration>(
        absl::Seconds(value.time_range().seconds()));
  } else {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid literal structure: " << value.ShortDebugString();
  }
  return {absl::WrapUnique(new Literal(
      scope, type_spec, std::move(literal_value), value.original()))};
}

Identifier::Identifier(Scope* scope, ScopedName scoped_name,
                       NamedObject* object)
    : Expression(scope),
      scoped_name_(std::move(scoped_name)),
      object_(CHECK_NOTNULL(object)) {}

pb::ExpressionKind Identifier::expr_kind() const {
  return pb::ExpressionKind::EXPR_IDENTIFIER;
}

absl::StatusOr<const TypeSpec*> Identifier::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return CHECK_NOTNULL(object_->type_spec());
}

const ScopedName& Identifier::scoped_name() const { return scoped_name_; }

NamedObject* Identifier::object() const { return object_; }

absl::optional<NamedObject*> Identifier::named_object() const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  return object_;
}

std::string Identifier::DebugString() const { return scoped_name_.full_name(); }

pb::ExpressionSpec Identifier::ToProto() const {
  auto proto = Expression::ToProto();
  *proto.mutable_identifier() = scoped_name().ToProto();
  return proto;
}

FunctionResultExpression::FunctionResultExpression(
    Scope* scope, Function* parent_function, pb::FunctionResultKind result_kind,
    absl::optional<std::unique_ptr<Expression>> expression)
    : Expression(scope),
      result_kind_(result_kind),
      parent_function_(parent_function) {
  if (expression.has_value()) {
    children_.emplace_back(CHECK_NOTNULL(std::move(expression).value()));
  }
}

absl::optional<NamedObject*> FunctionResultExpression::named_object() const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  if (children_.empty()) {
    return {};
  }
  return children_.front()->named_object();
}

pb::ExpressionKind FunctionResultExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_FUNCTION_RESULT;
}

pb::FunctionResultKind FunctionResultExpression::result_kind() const {
  return result_kind_;
}

Function* FunctionResultExpression::parent_function() const {
  return parent_function_;
}

bool FunctionResultExpression::ContainsFunctionExit() const { return true; }

std::string FunctionResultExpression::DebugString() const {
  switch (result_kind_) {
    case pb::FunctionResultKind::RESULT_NONE:
      return "";
    case pb::FunctionResultKind::RESULT_RETURN:
      return absl::StrCat("return ", children_.front()->DebugString());
    case pb::FunctionResultKind::RESULT_YIELD:
      return absl::StrCat("yield ", children_.front()->DebugString());
    case pb::FunctionResultKind::RESULT_PASS:
      return "pass";
  }
  return "unknown";
}

absl::StatusOr<const TypeSpec*> FunctionResultExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  if (children_.empty()) {
    return TypeUnknown::Instance();
  }
  CHECK_EQ(children_.size(), 1ul);
  return children_.front()->type_spec(type_hint);
}

namespace {
struct TypeUpdater {
  std::string name;
  const TypeSpec* type_spec = nullptr;
  bool is_updated = false;
  size_t index = 0;
  absl::Status UpdateType(Expression* expression) {
    ++index;
    ASSIGN_OR_RETURN(
        const TypeSpec* crt_type, expression->type_spec(type_spec),
        _ << "Obtaining type for element: " << index << " in " << name);
    if (!type_spec->IsAncestorOf(*crt_type)) {
      return status::InvalidArgumentErrorBuilder()
             << "Invalid element " << index
             << " of type: " << crt_type->full_name() << " in " << name
             << " expecting: " << type_spec->full_name();
    }
    if (!type_spec->IsBound() && crt_type->IsBound()) {
      type_spec = crt_type;
      is_updated = true;
    }
    return absl::OkStatus();
  }
  void Reset() {
    index = 0;
    is_updated = false;
  }
};
}  // namespace

ArrayDefinitionExpression::ArrayDefinitionExpression(
    Scope* scope, std::vector<std::unique_ptr<Expression>> elements)
    : Expression(scope) {
  CHECK(!elements.empty());
  children_ = std::move(elements);
}

pb::ExpressionKind ArrayDefinitionExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_ARRAY_DEF;
}

std::string ArrayDefinitionExpression::DebugString() const {
  std::vector<std::string> elements;
  elements.reserve(children_.size());
  for (const auto& child : children_) {
    elements.emplace_back(child->DebugString());
  }
  return absl::StrCat("[", absl::StrJoin(elements, ", "), "]");
}

absl::StatusOr<const TypeSpec*> ArrayDefinitionExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  const TypeSpec* base_type = nullptr;
  TypeUpdater element_type{"array element"};
  if (type_hint.has_value()) {
    RET_CHECK(type_hint.value() != nullptr) << kBugNotice;
    if (type_hint.value()->type_id() == pb::TypeId::ANY_ID ||
        type_hint.value()->type_id() == pb::TypeId::ARRAY_ID ||
        type_hint.value()->type_id() == pb::TypeId::ITERABLE_ID) {
      ASSIGN_OR_RETURN(base_type, scope_->FindTypeByName(kTypeNameArray),
                       _ << "Finding base Array type " << kBugNotice);
    } else if (type_hint.value()->type_id() == pb::TypeId::SET_ID) {
      ASSIGN_OR_RETURN(base_type, scope_->FindTypeByName(kTypeNameSet),
                       _ << "Finding base Array type " << kBugNotice);
    } else {
      return status::InvalidArgumentErrorBuilder()
             << "Array definition cannot be converted to non set/array type: "
             << type_hint.value()->full_name();
    }
    element_type.type_spec = base_type->ResultType();
    RET_CHECK(element_type.type_spec != nullptr)
        << "Bad result type for: " << base_type->full_name();
  } else {
    ASSIGN_OR_RETURN(base_type, scope_->FindTypeByName(kTypeNameArray),
                     _ << "Finding base Array type " << kBugNotice);
    element_type.type_spec = scope_->FindTypeAny();
  }
  do {
    element_type.Reset();
    for (size_t i = 0; i < children_.size(); ++i) {
      RETURN_IF_ERROR(element_type.UpdateType(children_[i].get()));
    }
  } while (element_type.is_updated);
  ASSIGN_OR_RETURN(auto negotiated_type,
                   base_type->Bind({TypeBindingArg{element_type.type_spec}}),
                   _ << "Building type for array definition");
  negotiated_types_.emplace_back(std::move(negotiated_type));
  return negotiated_types_.back().get();
}

MapDefinitionExpression::MapDefinitionExpression(
    Scope* scope, std::vector<std::unique_ptr<Expression>> elements)
    : Expression(scope) {
  CHECK(!elements.empty());
  CHECK_EQ(elements.size() % 2, 0ul) << elements.size();
  children_ = std::move(elements);
}

pb::ExpressionKind MapDefinitionExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_MAP_DEF;
}

std::string MapDefinitionExpression::DebugString() const {
  std::vector<std::string> elements;
  CHECK_EQ(children_.size() % 2, 0ul);
  elements.reserve(children_.size() / 2);
  for (size_t i = 0; i < children_.size() / 2; ++i) {
    elements.emplace_back(absl::StrCat(children_[2 * i]->DebugString(), ": ",
                                       children_[2 * i + 1]->DebugString()));
  }
  return absl::StrCat("[", absl::StrJoin(elements, ", "), "]");
}

absl::StatusOr<const TypeSpec*> MapDefinitionExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  const TypeSpec* base_type = nullptr;
  TypeUpdater key_type{"map element key"};
  TypeUpdater value_type{"map element value"};
  ASSIGN_OR_RETURN(base_type, scope_->FindTypeByName(kTypeNameMap),
                   _ << "Finding base Map type " << kBugNotice);
  if (type_hint.has_value()) {
    RET_CHECK(type_hint.value() != nullptr) << kBugNotice;
    if (type_hint.value()->type_id() != pb::TypeId::MAP_ID) {
      return status::InvalidArgumentErrorBuilder()
             << "Map definition cannot be converted to non map type: "
             << type_hint.value()->full_name();
    }
    const TypeSpec* element_type = base_type->ResultType();
    RET_CHECK(element_type != nullptr)
        << "Bad result type for: " << base_type->full_name();
    RET_CHECK(element_type->parameters().size() == 2)
        << "Bad map result type: " << element_type->full_name();
    key_type.type_spec = element_type->parameters()[0];
    value_type.type_spec = element_type->parameters()[1];
  } else {
    key_type.type_spec = value_type.type_spec = scope_->FindTypeAny();
  }
  do {
    key_type.Reset();
    value_type.Reset();
    for (size_t i = 0; i < children_.size() / 2; ++i) {
      RETURN_IF_ERROR(key_type.UpdateType(children_[2 * i].get()));
      RETURN_IF_ERROR(value_type.UpdateType(children_[2 * i + 1].get()));
    }
  } while (key_type.is_updated || value_type.is_updated);
  ASSIGN_OR_RETURN(auto negotiated_type,
                   base_type->Bind({TypeBindingArg{key_type.type_spec},
                                    TypeBindingArg{value_type.type_spec}}),
                   _ << "Building type for map definition");
  negotiated_types_.emplace_back(std::move(negotiated_type));
  return negotiated_types_.back().get();
}

IfExpression::IfExpression(Scope* scope,
                           std::vector<std::unique_ptr<Expression>> condition,
                           std::vector<std::unique_ptr<Expression>> expression)
    : Expression(scope) {
  CHECK(!condition.empty());
  CHECK((condition.size() == expression.size()) ||
        ((condition.size() + 1) == expression.size()))
      << "Sizes: " << condition.size() << " / " << expression.size();
  condition_.reserve(condition.size());
  expression_.reserve(expression.size());
  children_.reserve(condition.size() + expression.size());
  for (size_t i = 0; i < condition.size(); ++i) {
    condition_.emplace_back(CHECK_NOTNULL(condition[i].get()));
    expression_.emplace_back(CHECK_NOTNULL(expression[i].get()));
    children_.emplace_back(std::move(condition[i]));
    children_.emplace_back(std::move(expression[i]));
  }
  if (expression.size() > condition.size()) {
    expression_.emplace_back(CHECK_NOTNULL(expression.back().get()));
    children_.emplace_back(std::move(expression.back()));
  }
}

namespace {
std::string Reindent(std::string s) {
  std::vector<std::string> elements;
  for (absl::string_view s : absl::StrSplit(s, '\n')) {
    elements.emplace_back(absl::StrCat("  ", s));
  }
  return absl::StrJoin(elements, "\n");
}
}  // namespace

std::string IfExpression::DebugString() const {
  CHECK(condition_.size() == expression_.size() ||
        (condition_.size() + 1 == expression_.size()));
  std::vector<std::string> elements;
  for (size_t i = 0; i < condition_.size(); ++i) {
    elements.emplace_back(absl::StrCat(i == 0 ? "if (" : "} elif {",
                                       condition_[i]->DebugString(), ") {"));
    elements.emplace_back(Reindent(expression_[i]->DebugString()));
  }
  if (expression_.size() > condition_.size()) {
    elements.emplace_back(absl::StrCat("} else {"));
    elements.emplace_back(Reindent(expression_.back()->DebugString()));
  }
  elements.emplace_back("}");
  return absl::StrJoin(elements, "\n");
}

const std::vector<Expression*>& IfExpression::condition() const {
  return condition_;
}
const std::vector<Expression*>& IfExpression::expression() const {
  return expression_;
}

pb::ExpressionKind IfExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_IF;
}

bool IfExpression::ContainsFunctionExit() const {
  if (expression_.size() == condition_.size()) {
    // Else not covered - cannot return on all paths.
    return false;
  }
  for (Expression* expression : expression_) {
    if (!expression->ContainsFunctionExit()) {
      return false;
    }
  }
  return true;
}

absl::StatusOr<const TypeSpec*> IfExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return TypeUnknown::Instance();
}

ExpressionBlock::ExpressionBlock(
    Scope* scope, std::vector<std::unique_ptr<Expression>> children)
    : Expression(scope) {
  CHECK(!children.empty());
  children_ = std::move(children);
}

pb::ExpressionKind ExpressionBlock::expr_kind() const {
  return pb::ExpressionKind::EXPR_BLOCK;
}

absl::optional<NamedObject*> ExpressionBlock::named_object() const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  return children_.back()->named_object();
}

bool ExpressionBlock::ContainsFunctionExit() const {
  for (const auto& child : children_) {
    if (child->ContainsFunctionExit()) {
      return true;
    }
  }
  return false;
}

std::string ExpressionBlock::DebugString() const {
  std::vector<std::string> elements;
  for (const auto& child : children_) {
    elements.emplace_back(child->DebugString());
  }
  return absl::StrJoin(elements, "\n");
}

absl::StatusOr<const TypeSpec*> ExpressionBlock::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return children_.back()->type_spec(type_hint);
}

IndexExpression::IndexExpression(Scope* scope,
                                 std::unique_ptr<Expression> object_expression,
                                 std::unique_ptr<Expression> index_expression)
    : Expression(scope) {
  children_.emplace_back(std::move(CHECK_NOTNULL(object_expression)));
  children_.emplace_back(std::move(CHECK_NOTNULL(index_expression)));
}

pb::ExpressionKind IndexExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_INDEX;
}

std::string IndexExpression::DebugString() const {
  return absl::StrCat(children_.front()->DebugString(), "[",
                      children_.back()->DebugString(), "]");
}

absl::StatusOr<const TypeSpec*> IndexExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  RET_CHECK(children_.size() == 2) << "Got: " << children_.size();
  ASSIGN_OR_RETURN(const TypeSpec* object_type, children_.front()->type_spec(),
                   _ << "Obtaining indexed object type");
  const TypeSpec* index_type = object_type->IndexType();
  if (!index_type) {
    return status::InvalidArgumentErrorBuilder()
           << "Objects of type: " << object_type->full_name()
           << " does not support indexed access";
  }
  ASSIGN_OR_RETURN(const TypeSpec* index_expr_kind,
                   children_.back()->type_spec(index_type),
                   _ << "Obtaining indexed expression type");
  if (!index_type->IsAncestorOf(*index_expr_kind)) {
    return status::InvalidArgumentErrorBuilder()
           << "Objects of type: " << object_type->full_name() << " expect a "
           << index_type->full_name() << " as index expression, but "
           << index_expr_kind->full_name() << " provided";
  }
  return GetIndexedType(object_type);
}

absl::StatusOr<const TypeSpec*> IndexExpression::GetIndexedType(
    const TypeSpec* object_type) const {
  const TypeSpec* indexed_type = object_type->IndexedType();
  if (indexed_type == nullptr) {
    return status::InvalidArgumentErrorBuilder()
           << "Objects of type: " << object_type->full_name()
           << " do not return indexed type value";
  }
  return indexed_type;
}

TupleIndexExpression::TupleIndexExpression(
    Scope* scope, std::unique_ptr<Expression> object_expression,
    std::unique_ptr<Expression> index_expression, size_t index)
    : IndexExpression(scope, std::move(object_expression),
                      std::move(index_expression)),
      index_(index) {}

pb::ExpressionKind TupleIndexExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_TUPLE_INDEX;
}

absl::StatusOr<const TypeSpec*> TupleIndexExpression::GetIndexedType(
    const TypeSpec* object_type) const {
  if (index_ >= object_type->parameters().size()) {
    return status::InvalidArgumentErrorBuilder()
           << "Tuples index: " << index_
           << " outside the range of tuple type: " << object_type->full_name();
  }
  return object_type->parameters()[index_];
}

LambdaExpression::LambdaExpression(Scope* scope, Function* lambda_function)
    : Expression(scope), lambda_function_(lambda_function) {}

pb::ExpressionKind LambdaExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_LAMBDA;
}

Function* LambdaExpression::lambda_function() const { return lambda_function_; }

absl::optional<NamedObject*> LambdaExpression::named_object() const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  return lambda_function_;
}

std::string LambdaExpression::DebugString() const {
  return lambda_function_->DebugString();
}

pb::ExpressionSpec LambdaExpression::ToProto() const {
  auto proto = Expression::ToProto();
  *proto.mutable_function_spec() = lambda_function_->ToProto();
  return proto;
}

absl::StatusOr<const TypeSpec*> LambdaExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  if (type_hint.has_value() && lambda_function_->type_spec()->IsAncestorOf(
                                   *CHECK_NOTNULL(type_hint.value()))) {
    return type_hint.value();
  }
  return lambda_function_->type_spec();
}

DotAccessExpression::DotAccessExpression(
    Scope* scope, std::unique_ptr<Expression> left_expression, ScopeName name,
    NamedObject* object)
    : Expression(scope), name_(name), object_(object) {
  children_.emplace_back(std::move(left_expression));
}
DotAccessExpression::DotAccessExpression(
    Scope* scope, std::unique_ptr<Expression> left_expression,
    absl::string_view name, NamedObject* object)
    : Expression(scope),
      name_(std::string(name), {std::string(name)}, {}),
      object_(object) {
  children_.emplace_back(std::move(left_expression));
}

pb::ExpressionKind DotAccessExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_DOT_ACCESS;
}

absl::optional<NamedObject*> DotAccessExpression::named_object() const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  return object_;
}

const ScopeName& DotAccessExpression::name() const { return name_; }

NamedObject* DotAccessExpression::object() const { return object_; }

std::string DotAccessExpression::DebugString() const {
  return absl::StrCat(children_.front()->DebugString(), ".", name_.name());
}

absl::StatusOr<const TypeSpec*> DotAccessExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return object_->type_spec();
}

FunctionCallExpression::FunctionCallExpression(
    Scope* scope, std::unique_ptr<FunctionBinding> function_binding,
    std::unique_ptr<Expression> left_expression,
    std::vector<std::unique_ptr<Expression>> argument_expressions,
    bool is_method_call)
    : Expression(scope),
      function_binding_(std::move(CHECK_NOTNULL(function_binding))),
      is_method_call_(is_method_call) {
  if (left_expression) {
    left_expression_ = std::move(left_expression);
  } else {
    CHECK(function_binding_->fun.has_value());
  }
  children_ = std::move(argument_expressions);
}

pb::ExpressionKind FunctionCallExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_FUNCTION_CALL;
}

FunctionBinding* FunctionCallExpression::function_binding() const {
  return function_binding_.get();
}

absl::optional<Expression*> FunctionCallExpression::left_expression() const {
  if (!left_expression_) {
    return {};
  }
  return left_expression_.get();
}

bool FunctionCallExpression::is_method_call() const { return is_method_call_; }

pb::ExpressionSpec FunctionCallExpression::ToProto() const {
  pb::ExpressionSpec proto;
  proto.set_kind(expr_kind());
  if (type_spec_.has_value()) {
    *proto.mutable_type_spec() = type_spec_.value()->ToProto();
  }
  auto spec = proto.mutable_call_spec();
  if (left_expression_ && !is_method_call_) {
    *spec->mutable_left_expression() = left_expression_->ToProto();
  } else {
    CHECK(function_binding_->fun.has_value());
    *spec->mutable_call_name() =
        function_binding_->fun.value()->qualified_call_name().ToProto();
  }
  if (is_method_call()) {
    spec->set_is_method(true);
  }
  for (size_t i = 0; i < function_binding_->call_expressions.size(); ++i) {
    auto arg = spec->add_argument();
    if (i < function_binding_->names.size()) {
      arg->set_name(function_binding_->names[i]);
    }
    if (function_binding_->call_expressions[i].has_value()) {
      *arg->mutable_value() =
          function_binding_->call_expressions[i].value()->ToProto();
    }
  }
  *spec->mutable_binding_type() = function_binding_->type_spec->ToProto();
  return proto;
}

std::string FunctionCallExpression::DebugString() const {
  std::string fname;
  if (left_expression_ && !is_method_call_) {
    fname = left_expression_->DebugString();
  } else {
    CHECK(function_binding_->fun.has_value());
    fname = function_binding_->fun.value()->qualified_call_name().full_name();
  }
  CHECK_EQ(function_binding_->call_expressions.size(),
           function_binding_->names.size());
  std::vector<std::string> args;
  args.reserve(function_binding_->names.size());
  for (size_t i = 0; i < function_binding_->call_expressions.size(); ++i) {
    args.emplace_back(absl::StrCat(
        function_binding_->names[i], " = ",
        (function_binding_->call_expressions[i].has_value()
             ? function_binding_->call_expressions[i].value()->DebugString()
             : "UNSPECIFIED")));
  }
  return absl::StrCat(fname, "(", absl::StrJoin(args, ", "), ")");
}

absl::StatusOr<const TypeSpec*> FunctionCallExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return CHECK_NOTNULL(
      CHECK_NOTNULL(function_binding_->type_spec)->ResultType());
}

ImportStatementExpression::ImportStatementExpression(
    Scope* scope, absl::string_view local_name, bool is_alias, Module* module)
    : Expression(scope),
      local_name_(local_name),
      is_alias_(is_alias),
      module_(CHECK_NOTNULL(module)) {}

pb::ExpressionKind ImportStatementExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_IMPORT_STATEMENT;
}

absl::optional<NamedObject*> ImportStatementExpression::named_object() const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  return module_;
}

const std::string& ImportStatementExpression::local_name() const {
  return local_name_;
}

bool ImportStatementExpression::is_alias() const { return is_alias_; }

Module* ImportStatementExpression::module() const { return module_; }

absl::StatusOr<const TypeSpec*> ImportStatementExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return module_->type_spec();
}

std::string ImportStatementExpression::DebugString() const {
  if (is_alias_) {
    return absl::StrCat("import ", module_->name(), " as ", local_name_);
  }
  return absl::StrCat("import ", module_->name());
}

pb::ExpressionSpec ImportStatementExpression::ToProto() const {
  auto proto = Expression::ToProto();
  auto spec = proto.mutable_import_spec();
  spec->set_local_name(local_name());
  if (is_alias()) {
    spec->set_is_alias(true);
  }
  return proto;
}

FunctionDefinitionExpression::FunctionDefinitionExpression(
    Scope* scope, Function* def_function)
    : Expression(scope), def_function_(CHECK_NOTNULL(def_function)) {}

pb::ExpressionKind FunctionDefinitionExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_FUNCTION_DEF;
}

absl::optional<NamedObject*> FunctionDefinitionExpression::named_object()
    const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  return def_function_;
}

Function* FunctionDefinitionExpression::def_function() const {
  return def_function_;
}

absl::StatusOr<const TypeSpec*> FunctionDefinitionExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return def_function_->type_spec();
}

std::string FunctionDefinitionExpression::DebugString() const {
  return def_function_->DebugString();
}

pb::ExpressionSpec FunctionDefinitionExpression::ToProto() const {
  auto proto = Expression::ToProto();
  *proto.mutable_function_spec() = def_function_->ToProto();
  return proto;
}

SchemaDefinitionExpression::SchemaDefinitionExpression(Scope* scope,
                                                       TypeStruct* def_schema)
    : Expression(scope), def_schema_(CHECK_NOTNULL(def_schema)) {
  CHECK_EQ(def_schema_->type_id(), pb::TypeId::STRUCT_ID);
  type_spec_ = def_schema_;
}

pb::ExpressionKind SchemaDefinitionExpression::expr_kind() const {
  return pb::ExpressionKind::EXPR_SCHEMA_DEF;
}

absl::optional<NamedObject*> SchemaDefinitionExpression::named_object() const {
  if (named_object_.has_value()) {
    return named_object_;
  }
  return def_schema_;
}

const TypeStruct* SchemaDefinitionExpression::def_schema() const {
  return def_schema_;
}

absl::StatusOr<const TypeSpec*> SchemaDefinitionExpression::NegotiateType(
    absl::optional<const TypeSpec*> type_hint) {
  return def_schema_;
}

std::string SchemaDefinitionExpression::DebugString() const {
  std::vector<std::string> elements;
  elements.reserve(def_schema_->fields().size());
  for (const auto& field : def_schema_->fields()) {
    elements.emplace_back(absl::StrCat("  ", field.name, ": ",
                                       field.type_spec->full_name(), ";"));
  }
  return absl::StrCat("schema ", def_schema_->name(), " = {\n",
                      absl::StrJoin(elements, "\n"), "\n}\n");
}
}  // namespace analysis
}  // namespace nudl
