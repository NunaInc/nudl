#include "nudl/analysis/function.h"

#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "glog/logging.h"
#include "nudl/analysis/pragma.h"
#include "nudl/analysis/types.h"
#include "nudl/grammar/dsl.h"
#include "nudl/status/status.h"

namespace nudl {
namespace analysis {

FunctionGroup::FunctionGroup(std::shared_ptr<ScopeName> scope_name,
                             Scope* parent)
    : Scope(std::move(scope_name), parent) {
  types_.emplace_back(TypeUnknown::Instance()->Clone());
}

pb::ObjectKind FunctionGroup::kind() const {
  return pb::ObjectKind::OBJ_FUNCTION_GROUP;
}

const TypeSpec* FunctionGroup::type_spec() const {
  CHECK(!types_.empty());
  return types_.back().get();
}

const std::vector<Function*> FunctionGroup::functions() const {
  return functions_;
}

bool FunctionGroup::IsFunctionGroup(const NamedObject& object) {
  return object.kind() == pb::ObjectKind::OBJ_FUNCTION_GROUP;
}

std::string FunctionGroup::DebugString() const {
  std::vector<std::string> body;
  body.reserve(functions_.size());
  for (const Function* fun : functions_) {
    body.emplace_back(fun->DebugString());
  }
  return absl::StrCat("function group ", name(), " {\n",
                      absl::StrJoin(body, "\n"), "\n}\n");
}

absl::Status FunctionGroup::AddFunction(Function* fun) {
  std::vector<TypeBindingArg> function_types;
  function_types.reserve(functions_.size() + 1);
  for (Function* child_fun : functions_) {
    if (child_fun->type_spec()->IsEqual(*fun->type_spec())) {
      return status::AlreadyExistsErrorBuilder()
             << "A function with the same name and signature "
                "already exists in "
             << parent()->full_name() << " adding: " << fun->full_name()
             << ": " << child_fun->type_spec()->full_name()
             << " while adding: " << fun->type_spec()->full_name();
    }
    function_types.emplace_back(child_fun->type_spec());
  }
  function_types.emplace_back(fun->type_spec());
  std::unique_ptr<TypeSpec> new_group_type;
  if (function_types.size() >= 2) {
    ASSIGN_OR_RETURN(new_group_type,
                     parent_->FindTypeUnion()->Bind(function_types),
                     _ << "Binding union type to function group" << kBugNotice);
  } else {
    new_group_type = fun->type_spec()->Clone();
  }
  if (new_group_type) {
    types_.emplace_back(std::move(new_group_type));
  }
  functions_.emplace_back(fun);
  return absl::OkStatus();
}

absl::optional<Function*> FunctionGroup::FindBinding(
    const Function* fun) const {
  for (Function* my_fun : functions()) {
    if (my_fun->IsBinding(fun)) {
      return my_fun;
    }
  }
  return {};
}

absl::StatusOr<ScopeName> FunctionGroup::GetNextFunctionName() {
  auto subfunction_name = scope_name().SuffixName(scope_name().size() - 1);
  if (functions_.empty()) {
    return scope_name().Subfunction(subfunction_name);
  }
  return scope_name().Subfunction(
      absl::StrCat(subfunction_name, "__", functions_.size(), "__"));
}

absl::StatusOr<std::vector<std::unique_ptr<FunctionBinding>>>
FunctionGroup::TryBindFunction(
    Function* function,
    const std::vector<FunctionCallArgument>& arguments,
    std::vector<std::unique_ptr<FunctionBinding>>* existing) const {
  ASSIGN_OR_RETURN(
      auto binding, function->BindArguments(arguments),
      _ << "Binding arguments to: " << function->full_name());
  RET_CHECK(binding->fun.has_value());
  std::vector<std::unique_ptr<FunctionBinding>> new_bindings;
  FunctionBinding* add_current = nullptr;
  FunctionBinding* skip_current = nullptr;
  for (auto& spec : *existing) {
    bool skip_spec = false;
    if (!spec->IsEqual(*binding)) {
      if (spec->IsAncestorOf(*binding)) {
        add_current = spec.get();
        skip_spec = true;
      } else if (binding->IsAncestorOf(*spec)) {
        skip_current = spec.get();
      }
    }
    if (!skip_spec) {
      new_bindings.emplace_back(std::move(spec));
    }
  }
  if (!skip_current) {
    new_bindings.emplace_back(std::move(binding));
  } else if (add_current) {
    // It is quite hard to envision how this would be reached in normal
    // run mode, unless we are crafting a bad `existing` vector.
    return status::InvalidArgumentErrorBuilder()
      << "Conflicting signatures were bound to possible bindings, inspecting: "
      << binding->full_name() << " / " << skip_current->full_name()
      << " / " << add_current->full_name() << kBugNotice;
  }
  return {std::move(new_bindings)};
}


absl::StatusOr<std::unique_ptr<FunctionBinding>> FunctionGroup::FindSignature(
    const std::vector<FunctionCallArgument>& arguments) {
  std::vector<absl::Status> bind_status;
  std::vector<std::unique_ptr<FunctionBinding>> matching_specs;
  for (auto function : functions_) {
    auto bind_result = TryBindFunction(function, arguments, &matching_specs);
    if (bind_result.ok()) {
      matching_specs = std::move(bind_result).value();
    } else {
      bind_status.emplace_back(std::move(bind_result).status());
    }
  }
  if (matching_specs.empty()) {
    bind_status.emplace_back(absl::NotFoundError(
        "Cannot find any function signature matching arguments"));
    return status::JoinStatus(std::move(bind_status));
  }
  if (matching_specs.size() > 1) {
    std::vector<std::string> result_status;
    result_status.emplace_back(
        "Found too many functions matching the provided call signature");
    for (const auto& spec : matching_specs) {
      result_status.emplace_back(spec->full_name());
    }
    return absl::InvalidArgumentError(absl::StrJoin(result_status, ", "));
  }
  return {std::move(matching_specs.front())};
}

namespace {
bool IsFunctionObjectKind(pb::ObjectKind kind) {
  static const auto* const kFunctionKinds =
      new absl::flat_hash_set<pb::ObjectKind>({
          pb::ObjectKind::OBJ_FUNCTION,
          pb::ObjectKind::OBJ_METHOD,
          pb::ObjectKind::OBJ_LAMBDA,
      });
  return kFunctionKinds->contains(kind);
}

std::string TypeSignature(const std::vector<TypeBindingArg>& type_arguments) {
  std::vector<std::string> components;
  components.reserve(type_arguments.size());
  for (const auto& ta : type_arguments) {
    CHECK(std::holds_alternative<const TypeSpec*>(ta));
    components.emplace_back(std::get<const TypeSpec*>(ta)->full_name());
  }
  return absl::StrCat("Signature(", absl::StrJoin(components, "|"), ")");
}
}  // namespace

Function::Function(std::shared_ptr<ScopeName> scope_name,
                   absl::string_view function_name, pb::ObjectKind object_kind,
                   Scope* parent, Scope* definition_scope)
    : Scope(std::move(scope_name), CHECK_NOTNULL(parent)),
      function_name_(function_name),
      definition_scope_(CHECK_NOTNULL(definition_scope)),
      kind_(object_kind),
      type_spec_(CHECK_NOTNULL(parent->FindTypeFunction())),
      type_any_(parent_->FindTypeAny()) {
  CHECK(IsFunctionObjectKind(kind_)) << "Bad function kind: " << kind_;
}

Function::~Function() {
  while (!created_type_specs_.empty()) {
    created_type_specs_.pop_back();
  }
  while (!default_values_store_.empty()) {
    default_values_store_.pop_back();
  }
  while (!arguments_.empty()) {
    arguments_.pop_back();
  }
}

const TypeSpec* Function::type_spec() const {
  return CHECK_NOTNULL(type_spec_);
}

const std::string& Function::function_name() const { return function_name_; }

std::string Function::call_name() const {
  CHECK(!scope_name().empty());
  return std::string(absl::StripPrefix(
      scope_name().SuffixName(scope_name().size() - 1), "::"));
}

ScopedName Function::qualified_call_name() const {
  return ScopedName(module_scope()->scope_name_ptr(), call_name());
}

std::string Function::full_name() const {
  // TODO(catalin): May want to reduce the details here:
  return absl::StrCat("Function ", function_name_, " [", call_name(),
                      "] result: ", ResultKindName(result_kind_),
                      ", type: ", type_spec_->full_name());
}

pb::ObjectKind Function::kind() const { return kind_; }

pb::FunctionResultKind Function::result_kind() const { return result_kind_; }

Scope* Function::definition_scope() const { return definition_scope_; }

const std::vector<std::unique_ptr<Function>>& Function::bindings() const {
  return bindings_;
}

const absl::flat_hash_set<Function*>& Function::bindings_set() const {
  return bindings_set_;
}

const std::vector<std::unique_ptr<VarBase>>& Function::arguments() const {
  return arguments_;
}

const std::vector<absl::optional<Expression*>>& Function::default_values()
    const {
  return default_values_;
}

absl::optional<size_t> Function::first_default_value_index() const {
  return first_default_value_index_;
}

const TypeSpec* Function::result_type() const {
  if (type_spec_->ResultType()) {
    return type_spec_->ResultType();
  }
  return type_any_;
}

std::shared_ptr<pb::ExpressionBlock> Function::function_body() const {
  return function_body_;
}

bool Function::is_native() const { return !native_impl_.empty(); }

const absl::flat_hash_map<std::string, std::string>& Function::native_impl()
    const {
  return native_impl_;
}

bool Function::IsBinding(const Function* fun) const {
  return fun == this || bindings_set_.contains(fun);
}

// bool Function::IsBound() const { return type_spec_->IsBound(); }

bool Function::HasUndefinedArgTypes() const {
  for (const auto& arg : arguments_) {
    if (CHECK_NOTNULL(arg->type_spec())->type_id() == pb::TypeId::ANY_ID) {
      return true;
    }
  }
  return false;
}

bool Function::IsFunctionKind(const NamedObject& object) {
  return IsFunctionObjectKind(object.kind());
}

absl::StatusOr<Function*> Function::BuildInScope(
    Scope* parent, const pb::FunctionDefinition& element,
    absl::string_view lambda_name, const CodeContext& context) {
  std::string function_name;
  pb::ObjectKind object_kind = pb::ObjectKind::OBJ_FUNCTION;
  if (!lambda_name.empty()) {
    RET_CHECK(element.name().empty())
        << "Don't provide a name in function definition for lambdas";
    RET_CHECK(!element.is_method())
        << "Cannot have lambdas declared as methods";  // for now at least :)
    object_kind = pb::ObjectKind::OBJ_LAMBDA;
    function_name = lambda_name;
  } else {
    RET_CHECK(lambda_name.empty());
    function_name = element.name();
    if (element.is_method()) {
      object_kind = pb::ObjectKind::OBJ_METHOD;
    }
  }
  if (!NameUtil::IsValidName(function_name)) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid function name: `" << function_name << "`";
  }
  ASSIGN_OR_RETURN(ScopeName function_group_scope_name,
                   parent->scope_name().Subfunction(function_name));
  ScopedName group_name(parent->scope_name_ptr(), function_name);
  FunctionGroup* function_group = nullptr;
  auto fun_group_result = parent->FindName(parent->scope_name(), group_name);
  if (fun_group_result.ok()) {
    if (!FunctionGroup::IsFunctionGroup(*fun_group_result.value())) {
      return status::AlreadyExistsErrorBuilder()
             << "An object named: " << function_name
             << " already defined in: " << parent->full_name()
             << " and is not a function, but: "
             << fun_group_result.value()->full_name()
             << context.ToErrorInfo("In function definition");
    }
    function_group = static_cast<FunctionGroup*>(fun_group_result.value());
  } else {
    auto function_group_ptr = std::make_unique<FunctionGroup>(
        std::make_shared<ScopeName>(std::move(function_group_scope_name)),
        parent);
    function_group = function_group_ptr.get();
    RETURN_IF_ERROR(parent->AddSubScope(std::move(function_group_ptr)))
        << "Adding function group to scope"
        << context.ToErrorInfo("In function definition");
  }
  ASSIGN_OR_RETURN(auto function_scope_name,
                   function_group->GetNextFunctionName());
  auto fun = absl::WrapUnique(new Function(  // using new per protected.
      std::make_shared<ScopeName>(std::move(function_scope_name)),
      function_name, object_kind, function_group, parent));
  auto fun_ptr = fun.get();
  // Need to first add to parent, so we get the names
  RETURN_IF_ERROR(function_group->AddSubScope(std::move(fun)))
    << context.ToErrorInfo("Registering function definition");
  RETURN_IF_ERROR(fun_ptr->InitializeDefinition(element, context));
  RETURN_IF_ERROR(function_group->AddFunction(fun_ptr))
    << context.ToErrorInfo("Registering function definition");

  return fun_ptr;
}

absl::Status Function::InitializeParameterDefinition(
    const pb::FunctionParameter& param) {
  if (!NameUtil::IsValidName(param.name())) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid parameter name: `" << param.name() << "`";
  }
  if (arguments_map_.contains(param.name())) {
    return status::InvalidArgumentErrorBuilder()
           << "Parameter named: " << param.name() << " already defined";
  }
  const TypeSpec* type_spec = nullptr;
  if (param.has_type_spec()) {
    ASSIGN_OR_RETURN(type_spec, FindType(param.type_spec()),
                     _ << "For type of parameter: " << param.name());
  } else {
    type_spec = FindTypeAny();
  }
  absl::optional<Expression*> default_value;
  if (param.has_default_value()) {
    ASSIGN_OR_RETURN(
        auto default_value_ptr,
        definition_scope_->BuildExpression(param.default_value()),
        _ << "For default value of function parameter: " << param.name());
    ASSIGN_OR_RETURN(auto default_value_type,
                     default_value_ptr->type_spec(type_spec),
                     _ << "Determining type of default value of parameter: "
                       << param.name());
    // TODO(catalin): same compatibility / convertsion things here ..
    if (!type_spec->IsAncestorOf(*default_value_type)) {
      return status::InvalidArgumentErrorBuilder()
             << "Default value for parameter: " << param.name()
             << " of type: " << default_value_type->full_name()
             << " is incompatible with declared type of parameter: "
             << type_spec->full_name();
    }
    if (!param.has_type_spec()) {
      type_spec = default_value_type;
    }
    if (!first_default_value_index_.has_value()) {
      first_default_value_index_ = arguments_.size();
    }
    default_value = default_value_ptr.get();
    default_values_store_.emplace_back(std::move(default_value_ptr));
  } else if (first_default_value_index_.has_value()) {
    return status::InvalidArgumentErrorBuilder()
           << "No default value for parameter: " << param.name()
           << " after a parameter that has a default value provided: "
           << arguments_[first_default_value_index_.value()]->name();
  }
  auto arg = std::make_unique<Argument>(param.name(), type_spec, this);
  RETURN_IF_ERROR(AddChildStore(param.name(), arg.get()))
      << "Registering function parameter: " << param.name();
  default_values_.emplace_back(default_value);
  arguments_map_.emplace(param.name(), arg.get());
  arguments_.emplace_back(std::move(arg));
  return absl::OkStatus();
}

absl::Status Function::InitializeDefinition(
    const pb::FunctionDefinition& element, const CodeContext& context) {
  if ((!element.has_expression_block() ||
       element.expression_block().expression().empty()) &&
      element.snippet().empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "No body defined in function: " << name()
           << context.ToErrorInfo("In function definition");
  }
  absl::Status param_status;
  for (const auto& param : element.param()) {
    auto status = InitializeParameterDefinition(param);
    MergeErrorStatus(
        context.AppendErrorToStatus(status, "In function definition parameter"),
        param_status);
  }
  // Would accumulate too many error if continuing.
  RETURN_IF_ERROR(param_status);

  const TypeSpec* result_type = FindTypeAny();
  if (element.has_result_type()) {
    ASSIGN_OR_RETURN(
        result_type, FindType(element.result_type()),
        _ << "Finding return type of function: " << name()
          << context.ToErrorInfo("In function return type definition"));
  }
  // TODO(catalin): we may ease this, but for now I see only potential trouble
  //   if we allow returning union.
  // For now commenting out - the bindings will have a check.
  //
  // if (result_type->type_id() == pb::TypeId::UNION_ID) {
  //   return status::InvalidArgumentErrorBuilder()
  //         << "The provided result type: " << result_type->full_name()
  //         << " - is unacceptable as a function result type.";
  // }
  RETURN_IF_ERROR(UpdateFunctionType(result_type))
      << context.ToErrorInfo("In function definition");

  if (element.has_expression_block()) {
    function_body_ =
        std::make_shared<pb::ExpressionBlock>(element.expression_block());
    // TODO(catalin): here is a discussion - it would imply that the function
    //   has to be typed in order to process it - and I would really like to
    //   build the body here - to be studied...
    // Things become less clear on lambdas which may habitually be defined
    // without types.
    if (!HasUndefinedArgTypes()) {
      RETURN_IF_ERROR(BuildFunctionBody())
          << "Building function body"
          << context.ToErrorInfo("In function definition");
    }
  } else {
    for (const auto& snippet : element.snippet()) {
      native_impl_.emplace(snippet.name(), snippet.body());
    }
  }
  if (kind_ == pb::ObjectKind::OBJ_METHOD) {
    RETURN_IF_ERROR(InitializeAsMethod())
        << "Setting up function: " << name() << " as a type method"
        << context.ToErrorInfo("In function definition");
  }
  return absl::OkStatus();
}

absl::Status Function::UpdateFunctionType(const TypeSpec* result_type) {
  if (result_type->type_id() == pb::TypeId::FUNCTION_ID &&
      !result_type->IsBound()) {
    absl::flat_hash_set<std::string> unbound_types;
    TypeUtils::FindUnboundTypes(result_type, &unbound_types);
    return status::InvalidArgumentErrorBuilder()
           << "In function: " << call_name()
           << ", when the returning value "
              "of a function is typed as a Function, this type needs to be "
              "bound. "
              "Please add non-abstract type specifications to all arguments "
              "and "
              "define the return value as well if necessary. Type found: "
           << result_type->full_name()
           << " unbound types: " << absl::StrJoin(unbound_types, ", ");
  }
  std::vector<TypeBindingArg> bindings;
  bindings.reserve(arguments_.size() + 1);
  for (const auto& arg : arguments_) {
    bindings.push_back(CHECK_NOTNULL(arg->type_spec()));
  }
  if (type_signature_.empty()) {
    type_signature_ = TypeSignature(bindings);
    bindings_map_.emplace(type_signature_, this);
  }
  bindings.push_back(result_type);
  ASSIGN_OR_RETURN(
      auto function_type_spec, type_spec_->Bind(bindings),
      // this may be a bug if we get - we should check types beforehand.
      _ << "Creating bind function type for " << name());
  RET_CHECK(function_type_spec->type_id() == pb::TypeId::FUNCTION_ID)
      << "For: " << function_type_spec->full_name();
  auto fun_type = static_cast<TypeFunction*>(function_type_spec.get());
  fun_type->set_first_default_value_index(first_default_value_index_);
  RET_CHECK(fun_type->arguments().size() == arguments_.size());
  for (size_t i = 0; i < arguments_.size(); ++i) {
    fun_type->set_argument_name(i, arguments_[i]->name());
  }
  type_spec_ = CHECK_NOTNULL(function_type_spec.get());
  created_type_specs_.emplace_back(std::move(function_type_spec));
  return absl::OkStatus();
}

absl::StatusOr<VarBase*> Function::ValidateAssignment(
    const ScopedName& name, NamedObject* object) const {
  ASSIGN_OR_RETURN(auto var_base, Scope::ValidateAssignment(name, object));
  VarBase* root_var = CHECK_NOTNULL(var_base->GetRootVar());
  if (!IsAncestorOf(root_var)) {
    return status::InvalidArgumentErrorBuilder()
           << "Function: " << call_name() << " cannot assign variables "
           << "or fields of variables outside its scope: "
           << root_var->full_name() << " through name: `" << name.name()
           << "` for: `" << var_base->full_name() << "`";
  }
  if (object->kind() == pb::ObjectKind::OBJ_ARGUMENT &&
      !object->type_spec()->IsBasicType()) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot reassign function argument: " << object->name()
           << " of non-basic type: " << object->type_spec()->full_name();
  }
  return var_base;
}

absl::Status Function::AddAsMethod(const TypeSpec* member_type) {
  auto type_member_store = CHECK_NOTNULL(member_type->type_member_store());
  RETURN_IF_ERROR(type_member_store->AddName(function_name(), this))
      << "Adding defined function " << name()
      << " as a method of type: " << member_type->full_name();
  return absl::OkStatus();
}

absl::Status Function::InitializeAsMethod() {
  CHECK_EQ(kind_, pb::ObjectKind::OBJ_METHOD);
  if (arguments_.empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "Method function requires at least a parameter, "
              "to specify which type is to be bound to.";
  }
  auto member_type = arguments_.front()->type_spec();
  if (member_type->type_id() == pb::TypeId::UNION_ID) {
    for (const auto param : member_type->parameters()) {
      RETURN_IF_ERROR(AddAsMethod(param));
    }
  } else {
    RETURN_IF_ERROR(AddAsMethod(member_type));
  }
  return absl::OkStatus();
}

absl::StatusOr<pb::FunctionResultKind> Function::RegisterResultKind(
    pb::FunctionResultKind result_kind) {
  if (result_kind_ == result_kind) {
    return result_kind;
  }
  switch (result_kind_) {
    case pb::FunctionResultKind::RESULT_NONE:
      result_kind_ = result_kind;
      break;
    case pb::FunctionResultKind::RESULT_PASS:
      if (result_kind == pb::FunctionResultKind::RESULT_NONE) {
        return absl::InvalidArgumentError(
            "When using just `pass` in a function, the last expression "
            "must explicitly `yield`");
      }
      if (result_kind == pb::FunctionResultKind::RESULT_RETURN) {
        return absl::InvalidArgumentError(
            "Can only `yield` in a function that uses `pass` -"
            " `return` is not acceptable");
      }
      break;
    case pb::FunctionResultKind::RESULT_YIELD:
      if (result_kind == pb::FunctionResultKind::RESULT_RETURN) {
        return absl::InvalidArgumentError(
            "Cannot `return` in a function that uses `yield`");
      }
      if (result_kind == pb::FunctionResultKind::RESULT_NONE) {
        // Last statement in a Yield function is actually a pass
        return pb::FunctionResultKind::RESULT_PASS;
      }
      break;
    case pb::FunctionResultKind::RESULT_RETURN:
      if (result_kind == pb::FunctionResultKind::RESULT_PASS ||
          result_kind == pb::FunctionResultKind::RESULT_YIELD) {
        return absl::InvalidArgumentError(
            "Cannot `yield` or `pass` in functions that use `return`");
      }
      if (result_kind == pb::FunctionResultKind::RESULT_NONE) {
        // Last statement in a Yield function is actually a return
        return pb::FunctionResultKind::RESULT_RETURN;
      }
      break;
  }
  return result_kind;
}

namespace {
bool AreCompatibleResultTypes(const TypeSpec* type_a, const TypeSpec* type_b) {
  if (type_a->IsConvertibleFrom(*type_b) ||
      type_b->IsConvertibleFrom(*type_a)) {
    return true;
  }
  if (type_a->type_id() == pb::TypeId::NULL_ID ||
      type_b->type_id() == pb::TypeId::NULL_ID) {
    return true;
  }
  return false;
}
}  // namespace

absl::Status Function::RegisterResultExpression(
    pb::FunctionResultKind result_kind, Expression* expression) {
  ASSIGN_OR_RETURN(auto coerced_result_kind, RegisterResultKind(result_kind),
                   _ << "Checking result expression for: " << full_name());
  if (coerced_result_kind == pb::FunctionResultKind::RESULT_PASS) {
    result_expressions_.push_back({result_kind});
    return absl::OkStatus();
  }
  const TypeSpec* result_type = type_spec_->ResultType();
  // TODO(catalin): may want to add some automatic conversion here:
  ASSIGN_OR_RETURN(const TypeSpec* type_spec,
                   expression->type_spec(result_type),
                   _ << "Negotiating result type of return expression.");
  if (!type_spec || type_spec->type_id() == pb::TypeId::UNKNOWN_ID) {
    // We allow a type unknown expression iff this is the default
    // last statement return and we have a result registered already.
    // TODO(cpopescu): There may be a discussion here, if this is kosher.
    // As we may have an if statement w/ no returns on one path, but is
    // fine for now.
    if (!result_expressions_.empty() &&
        result_kind == pb::FunctionResultKind::RESULT_NONE) {
      return absl::OkStatus();
    }
    return status::InvalidArgumentErrorBuilder()
           << "The result expression of function " << full_name()
           << " does not have a type associated with it. "
           << "Please explicitly return or yield value for expression";
  }
  if (!result_type->IsAncestorOf(*type_spec)) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot return: " << type_spec->full_name()
           << " in a function "
              "that expects a: "
           << result_type->full_name() << " result for: " << full_name();
  }
  // We may ease on this - but generally we expect bound values
  // to be returned in functions.
  if (!type_spec->IsBound() &&
      type_spec->type_id() != pb::TypeId::FUNCTION_ID) {
    return status::InvalidArgumentErrorBuilder()
           << "The provided result type: " << type_spec->full_name()
           << " of returned expression is unbound and not a function";
  }
  // If result type declared for this function is not bound, we
  // expect that return values on all paths to be compatible
  // with each-other.
  if (!result_type->IsBound()) {
    for (const auto& result : result_expressions_) {
      if (!result.type_spec) {
        continue;
      }
      if (!AreCompatibleResultTypes(type_spec, result.type_spec)) {
        return status::InvalidArgumentErrorBuilder()
               << "The provided result type of return expression: "
               << type_spec->full_name()
               << " is incompatible with previous returned expression: "
               << result.type_spec->full_name();
      }
    }
  }
  result_expressions_.push_back({result_kind, expression, type_spec});
  return absl::OkStatus();
}

absl::Status Function::UpdateFunctionTypeOnResults() {
  RET_CHECK(!result_type_negotiated_)
      << "Cannot renegotiate the return type of a function";
  if (result_kind_ == pb::FunctionResultKind::RESULT_PASS) {
    return status::InvalidArgumentErrorBuilder()
           << "Function that uses `pass` needs to yield some values. "
           << "For: " << full_name();
  }
  const TypeSpec* result_type = type_spec_->ResultType();
  RET_CHECK(result_type != nullptr)
      << "No result type for function: " << full_name() << kBugNotice;
  std::vector<const TypeSpec*> registered_result_types;
  registered_result_types.reserve(result_expressions_.size());
  for (const auto& result : result_expressions_) {
    if (!result.type_spec) {
      continue;
    }
    // We should have caught this already.
    RET_CHECK(result_type->IsAncestorOf(*result.type_spec))
        << "Invalid type of result expression in " << full_name();
    registered_result_types.emplace_back(result.type_spec);
  }
  // We should have caught this already:
  RET_CHECK(!registered_result_types.empty())
      << "No results for function: " << full_name();
  if (result_type->IsBound() ||
      result_type->type_id() == pb::TypeId::FUNCTION_ID) {
    // If a bound result type or a function type was registered, we go with it:
    result_type_negotiated_ = true;
    return absl::OkStatus();
  }
  const TypeSpec* new_result_type = nullptr;
  const TypeSpec* null_result_type = nullptr;
  // Else we find the most ancestral result type
  for (auto type_spec : registered_result_types) {
    if (type_spec->type_id() == pb::TypeId::NULL_ID) {
      null_result_type = type_spec;
    } else {
      if (!new_result_type || type_spec->IsConvertibleFrom(*new_result_type)) {
        new_result_type = type_spec;
      }
    }
  }
  if (!new_result_type) {
    if (!null_result_type) {
      return status::InvalidArgumentErrorBuilder()
             << "No proper return type was found for instantiation of "
                "function: "
             << full_name();
    }
    new_result_type = null_result_type;
  } else if (null_result_type) {
    ASSIGN_OR_RETURN(
        auto nullable_type, FindTypeByName(kTypeNameNullable),
        _ << "Finding base type Nullable for function return type binding"
          << kBugNotice);
    ASSIGN_OR_RETURN(
        auto result_type,
        nullable_type->Bind(std::vector<TypeBindingArg>({new_result_type})),
        _ << "Creating Nullable type for: " << new_result_type->full_name()
          << " during function return type binding");
    new_result_type = result_type.get();
    created_type_specs_.emplace_back(std::move(result_type));
  }
  RET_CHECK(result_type->IsAncestorOf(*new_result_type))
      << "The inferred new result type for function: " << full_name() << ": "
      << new_result_type->full_name() << " is not convertible to original";
  RETURN_IF_ERROR(UpdateFunctionType(new_result_type))
      << "Updating the result type for function: " << full_name()
      << " to newly inferred " << new_result_type->full_name();
  return absl::OkStatus();
}

absl::string_view Function::ResultKindName(pb::FunctionResultKind result_kind) {
  switch (result_kind) {
    case pb::FunctionResultKind::RESULT_NONE:
      return "default";
    case pb::FunctionResultKind::RESULT_RETURN:
      return "return";
    case pb::FunctionResultKind::RESULT_YIELD:
      return "yield";
    case pb::FunctionResultKind::RESULT_PASS:
      return "pass";
  }
  return "unknown";
}

FunctionBinding::FunctionBinding(Function* fun)
    : fun_type(static_cast<const TypeFunction*>(fun->type_spec())),
      fun(fun),
      pragmas(fun->pragma_handler()) {
  num_args = fun->arguments().size();
  CHECK_EQ(num_args, fun->default_values().size());
  CHECK_EQ(fun_type->parameters().size(), num_args + 1)
      << "For: " << fun << " => " << fun->full_name();
  type_arguments.reserve(num_args + 1);
  call_expressions.reserve(num_args);
  names.reserve(num_args);
}

FunctionBinding::FunctionBinding(const TypeFunction* fun_type,
                                 const PragmaHandler* pragmas)
    : fun_type(fun_type), pragmas(pragmas) {
  CHECK(!fun_type->parameters().empty());
  num_args = fun_type->parameters().size() - 1;
  type_arguments.reserve(fun_type->parameters().size());
  call_expressions.reserve(num_args);
  names.reserve(num_args);
}

absl::StatusOr<std::unique_ptr<FunctionBinding>> FunctionBinding::Bind(
    Function* fun, const std::vector<FunctionCallArgument>& arguments) {
  RET_CHECK(fun->type_spec()->type_id() == pb::TypeId::FUNCTION_ID &&
            !fun->type_spec()->parameters().empty())
    << "Improperly built function: " << fun->full_name();
  auto result = absl::WrapUnique(new FunctionBinding(fun));
  RETURN_IF_ERROR(result->BindImpl(arguments));
  return {std::move(result)};
}

absl::StatusOr<std::unique_ptr<FunctionBinding>> FunctionBinding::BindType(
    const TypeFunction* fun_type, const PragmaHandler* pragmas,
    const std::vector<FunctionCallArgument>& arguments) {
  if (fun_type->type_id() != pb::TypeId::FUNCTION_ID ||
      fun_type->parameters().empty()) {
    // This is possible with some constructs, opposed to RET_CHECK
    // in the function above.
    return status::InvalidArgumentErrorBuilder()
           << "Cannot build binding for improper function type: "
           << fun_type->full_name() << " - this means that you may need to "
           << "provide some extra argument / type information";
  }
  auto result = absl::WrapUnique(new FunctionBinding(fun_type, pragmas));
  RETURN_IF_ERROR(result->BindImpl(arguments));
  return {std::move(result)};
}

absl::Status FunctionBinding::BindImpl(
    const std::vector<FunctionCallArgument>& arguments) {
  LOG_IF(INFO, pragmas->log_bindings())
      << "Starting the bind for: " << FunctionNameForLog();
  LocalNamesRebinder rebinder;
  absl::Status bind_status;
  RET_CHECK(num_args == fun_type->arguments().size());
  while (fun_index < num_args && arg_index < arguments.size()) {
    absl::Status arg_status;
    const auto& current_arg = fun_type->arguments()[fun_index];
    if (arguments[arg_index].name.has_value() &&
        current_arg.name != arguments[arg_index].name.value()) {
      arg_status =
          BindDefaultValue(current_arg.name, current_arg.type_spec, &rebinder);
    } else {
      arg_status = BindArgument(current_arg.name, current_arg.type_spec,
                                arguments[arg_index], &rebinder);
      ++arg_index;
    }
    status::UpdateOrAnnotate(bind_status, arg_status);
    ++fun_index;
  }
  while (fun_index < num_args) {
    const auto& current_arg = fun_type->arguments()[fun_index];
    status::UpdateOrAnnotate(
        bind_status,
        BindDefaultValue(current_arg.name, current_arg.type_spec, &rebinder));
    ++fun_index;
  }
  status::UpdateOrAnnotate(bind_status, UseRemainingArguments(arguments));
  RETURN_IF_ERROR(bind_status);
  std::vector<const TypeSpec*> fun_bind_types;
  fun_bind_types.reserve(type_arguments.size() + 1);
  for (const auto& binding : type_arguments) {
    fun_bind_types.emplace_back(std::get<const TypeSpec*>(binding));
  }
  fun_bind_types.emplace_back(CHECK_NOTNULL(fun_type->ResultType()));
  ASSIGN_OR_RETURN(
      type_spec,
      rebinder.RebuildFunctionWithComponents(fun_type, fun_bind_types),
      _ << "Rebuilding function type for binding");
  LOG_IF(INFO, pragmas->log_bindings())
      << "BIND LOG: " << FunctionNameForLog()
      << " Rebuilt function type with components "
      << " from: " << fun_type->ResultType()->full_name() << " to "
      << type_spec->full_name();
  std::move(rebinder.allocated_types.begin(), rebinder.allocated_types.end(),
            std::back_inserter(stored_types));
  LOG_IF(INFO, pragmas->log_bindings())
      << "BIND LOG: Finishing the bind of: " << full_name();
  return bind_status;
}

absl::Status FunctionBinding::BindDefaultValue(absl::string_view arg_name,
                                               const TypeSpec* arg_type,
                                               LocalNamesRebinder* rebinder) {
  if (!fun_type->first_default_value_index().has_value() ||
      fun_type->first_default_value_index().value() > fun_index ||
      (fun.has_value() &&
       (fun.value()->default_values().size() <= fun_index ||
        !fun.value()->default_values()[fun_index].has_value()))) {
    return status::InvalidArgumentErrorBuilder()
           << "No value provided for function parameter: " << arg_name
           << " which has no default value";
  }
  if (fun.has_value()) {
    const auto& default_value = fun.value()->default_values()[fun_index];
    ASSIGN_OR_RETURN(const TypeSpec* default_type_spec,
                     default_value.value()->type_spec(),
                     _ << "Obtaining type for default value of " << arg_name);
    if (!default_type_spec->IsEqual(*arg_type)) {
      RETURN_IF_ERROR(rebinder->ProcessType(arg_type, default_type_spec))
          << "Rebinding argument type for: " << arg_name
          << " from declared type: " << arg_type->full_name()
          << " to default value type: " << default_type_spec->full_name();
      ASSIGN_OR_RETURN(auto rebuilt_type,
                       rebinder->RebuildType(arg_type, default_type_spec),
                       _ << "Rebuilding argument type for: " << arg_name);
      LOG_IF(INFO, pragmas->log_bindings())
          << "BIND LOG: " << FunctionNameForLog()
          << " Rebuilt default value for argument: " << arg_name
          << " from: " << arg_type->full_name() << " to "
          << rebuilt_type->full_name();
      type_arguments.emplace_back(TypeBindingArg{rebuilt_type});
      // TODO(catalin): autoconversion.
      if (!rebuilt_type->IsAncestorOf(*default_type_spec)) {
        return status::InvalidArgumentErrorBuilder()
               << "Type of default value for argument: " << arg_name << ": "
               << default_type_spec->full_name()
               << " is not compatible with inferred type for the call: "
               << rebuilt_type->full_name();
      }
    } else {
      type_arguments.emplace_back(TypeBindingArg{default_type_spec});
    }
    call_expressions.emplace_back(default_value.value());
  } else {
    type_arguments.emplace_back(TypeBindingArg{arg_type});
    call_expressions.emplace_back(absl::optional<Expression*>{});
  }
  names.emplace_back(std::string(arg_name));
  return absl::OkStatus();
}

std::string FunctionBinding::FunctionNameForLog() const {
  if (fun.has_value()) {
    return fun.value()->call_name();
  }
  return "function from expression";
}

std::string FunctionBinding::full_name() const {
  if (fun.has_value()) {
    return absl::StrCat(
        "Function binding of ", fun.value()->full_name(),
        " as ", type_spec->full_name());
  } else {
    return absl::StrCat(
        "Function type binding of ", fun_type->full_name(),
        " as ", type_spec->full_name());
  }
}


absl::StatusOr<const TypeSpec*> FunctionBinding::RebindFunctionArgument(
    absl::string_view arg_name, const FunctionCallArgument& call_arg,
    const TypeSpec* rebuilt_type) {
  if (rebuilt_type->type_id() != size_t(pb::TypeId::FUNCTION_ID) ||
      (rebuilt_type->IsBound() &&
       (!fun.has_value() || !fun.value()->is_native())) ||
      !call_arg.value.has_value()) {
    LOG_IF(INFO, pragmas->log_bindings() && (rebuilt_type->type_id() !=
                                             size_t(pb::TypeId::FUNCTION_ID)))
        << "BIND LOG: " << FunctionNameForLog()
        << " Skipping rebinding function argument: " << arg_name
        << " and keeping: " << rebuilt_type->full_name();
    return rebuilt_type;
  }
  auto call_object = call_arg.value.value()->named_object();
  Function* sub_function = nullptr;
  FunctionGroup* sub_group = nullptr;
  if (call_object.has_value()) {
    auto named_object = call_arg.value.value()->named_object().value();
    if (Function::IsFunctionKind(*named_object)) {
      sub_function = static_cast<Function*>(named_object);
    } else if (FunctionGroup::IsFunctionGroup(*named_object)) {
      sub_group = static_cast<FunctionGroup*>(named_object);
    }
  }
  if (!sub_function && !sub_group) {
    if (rebuilt_type->IsBound()) {
      LOG_IF(INFO, pragmas->log_bindings())
          << "BIND LOG: " << FunctionNameForLog()
          << " Keeping rebuilt type for " << arg_name
          << " as it is bound, and have no function object: "
          << rebuilt_type->full_name();
      return rebuilt_type;
    }
    // This is a delicte error to grasp. Basically:
    //   f = (s => s + 1)
    //   sum(map(list, f)))
    // leaves us in the dark of the nature of the underlying function.
    // as f, inferred as Function<Any, Any> can be messed up at any type.
    //
    // Setting however f = (s : Int => s + 1) solves the issues,
    // as type of f is now fully bound.
    //
    return status::FailedPreconditionErrorBuilder()
           << "Cannot determine the source for the function provided by "
              "argument: "
           << arg_name << " in the call of " << FunctionNameForLog()
           << ", and the type that we inferred is not fully defined. We "
              "suggest annotating the types of the function that you use "
              "as argument to a more precise annotation (e.g. remove Any, "
              "Unions and such). Function type found at this point: `"
           << rebuilt_type->full_name() << "`";
  }
  std::vector<FunctionCallArgument> subargs;
  for (size_t i = 0; i + 1 < rebuilt_type->parameters().size(); ++i) {
    subargs.emplace_back(
        FunctionCallArgument{{}, {}, rebuilt_type->parameters()[i]});
  }
  std::unique_ptr<FunctionBinding> sub_binding;
  if (sub_function) {
    ASSIGN_OR_RETURN(sub_binding, sub_function->BindArguments(subargs),
                     _ << "Binding sub-arguments in function argument: "
                       << arg_name << " in call to: " << FunctionNameForLog());
  } else {
    CHECK(sub_group != nullptr);  // we check above that have one of them set.
    ASSIGN_OR_RETURN(sub_binding, sub_group->FindSignature(subargs),
                     _ << "Binding sub-arguments in function group argument: "
                       << arg_name << " in call to: " << FunctionNameForLog());
  }
  if (sub_binding->fun.has_value()) {
    LOG_IF(INFO, pragmas->log_bindings())
        << "BIND LOG: " << FunctionNameForLog()
        << " Re-binding object argument function for " << arg_name << ": "
        << sub_binding->fun.value()->full_name()
        << " per re-bound call to type: " << sub_binding->type_spec->full_name()
        << " from: " << rebuilt_type->full_name();
    RETURN_IF_ERROR(sub_binding->fun.value()->Bind(sub_binding.get()))
        << "Binding sub-function argument: " << arg_name
        << " in call to: " << FunctionNameForLog();
    call_arg.value.value()->set_named_object(sub_binding->fun.value());
  } else {
    LOG_IF(INFO, pragmas->log_bindings())
        << "BIND LOG: " << FunctionNameForLog()
        << " Re-bound function call type for " << arg_name
        << " to: " << sub_binding->type_spec->full_name()
        << " from: " << rebuilt_type->full_name()
        << " but no object function to re-bind";
  }
  rebuilt_type = CHECK_NOTNULL(sub_binding->type_spec);
  sub_bindings.emplace_back(std::move(sub_binding));
  return rebuilt_type;
}

absl::Status FunctionBinding::BindArgument(absl::string_view arg_name,
                                           const TypeSpec* arg_type,
                                           const FunctionCallArgument& call_arg,
                                           LocalNamesRebinder* rebinder) {
  ASSIGN_OR_RETURN(const TypeSpec* call_type, call_arg.ArgType(arg_type),
                   _ << "Obtaining type for call argument: " << arg_name
                     << " in call of: " << FunctionNameForLog());
  // Insight: Can pass an specific subtype - however if is a function
  // I can pass a more general function (ie. a supertype).
  // A.g. I can pass a Function<Numeric> for an argument Function<Int>.

  // TODO(catalin): work more on the functions w/ default arguments and their
  //   type compatibility.
  const bool is_function = (arg_type->type_id() == pb::TypeId::FUNCTION_ID &&
                            call_type->type_id() == pb::TypeId::FUNCTION_ID);
  // call_type->IsAncestorOf(*arg_type) && !call_type->IsEqual(*arg_type);
  const TypeSpec* rebuilt_type = arg_type;
  if (!is_function) {
    if (!arg_type->IsAncestorOf(*call_type)) {
      return status::InvalidArgumentErrorBuilder()
             << "Provided value for argument " << arg_name << " of "
             << FunctionNameForLog() << ": `" << call_type->full_name()
             << "` is incompatible with declared type of argument: `"
             << arg_type->full_name() << "`";
    }
    RETURN_IF_ERROR(rebinder->ProcessType(arg_type, call_type))
        << "Rebinding argument type for: " << arg_name
        << " from declared type: `" << arg_type->full_name() << "`"
        << " to call value type: `" << call_type->full_name() << "`";
    ASSIGN_OR_RETURN(rebuilt_type, rebinder->RebuildType(arg_type, call_type),
                     _ << "Rebuilding argument type for: " << arg_name);
    LOG_IF(INFO, pragmas->log_bindings())
        << "BIND LOG: " << FunctionNameForLog()
        << " Non function argument: " << arg_name
        << " rebuilt from: " << arg_type->full_name()
        << " to: " << rebuilt_type->full_name()
        << " with call type: " << call_type->full_name();
  } else {
    // Bind any local names first:
    RETURN_IF_ERROR(rebinder->ProcessType(arg_type, call_type))
        << "Processing default function argument types for " << arg_name
        << " - param type: `" << arg_type->full_name()
        << "`, called with type: `" << call_type->full_name() << "`";
    ASSIGN_OR_RETURN(auto rebuilt_arg_type,
                     rebinder->RebuildType(arg_type, arg_type));
    RET_CHECK(CHECK_NOTNULL(rebuilt_arg_type)->type_id() ==
              pb::TypeId::FUNCTION_ID);
    LOG_IF(INFO, pragmas->log_bindings())
        << "BIND LOG: " << FunctionNameForLog() << " argument: " << arg_name
        << " rebuilt from: " << arg_type->full_name()
        << " to: " << rebuilt_type->full_name()
        << " with call type: " << call_type->full_name();
    ASSIGN_OR_RETURN(
        auto rebuilt_call_type,
        RebindFunctionArgument(arg_name, call_arg, rebuilt_arg_type),
        _ << "Rebinding function argument: " << arg_name
          << " from: `" << arg_type->full_name()
          << "`, rebuilt as type: `" << rebuilt_arg_type->full_name()
          << "` and called with type: `" << call_type->full_name() << "`");
    RET_CHECK(CHECK_NOTNULL(rebuilt_call_type)->type_id() ==
              pb::TypeId::FUNCTION_ID);
    auto fun_arg_type = static_cast<const TypeFunction*>(rebuilt_arg_type);
    auto call_arg_type = static_cast<const TypeFunction*>(rebuilt_call_type);
    ASSIGN_OR_RETURN(
        auto new_call_type, fun_arg_type->BindWithFunction(*call_arg_type),
        _ << "Binding function parameter: " << arg_name << " of type `"
        << arg_type->full_name()
        << "` to call argument of type: `" << call_type->full_name() << "`");


    // Reassign the unknown types in original arg type to the final
    // deduced call type.
    RETURN_IF_ERROR(rebinder->ProcessType(arg_type, new_call_type.get()))
        << "Processing rebound function type for: " << arg_name
        << " from declared type: `" << arg_type->full_name()
        << "` to type: `" << new_call_type->full_name() << "`";
    RETURN_IF_ERROR(rebinder->ProcessType(rebuilt_arg_type->ResultType(),
                                          new_call_type->ResultType()))
        << "Processing rebound function result type for: " << arg_name
        << " from declared type: `" << arg_type->ResultType()->full_name()
        << "` to call value type: `"
        << new_call_type->ResultType()->full_name() << "`";
    LOG_IF(INFO, pragmas->log_bindings())
        << "BIND LOG: " << FunctionNameForLog()
        << " Arg Rebinding: " << arg_name << std::endl
        << "   arg_type: " << arg_type->full_name() << std::endl
        << "   call_type: " << call_type->full_name() << std::endl
        << "   rebuilt_arg_type: " << rebuilt_arg_type->full_name() << std::endl
        << "   rebuilt_call_type: " << rebuilt_call_type->full_name()
        << std::endl
        << "   new_call_type: " << new_call_type->full_name() << std::endl;

    arg_type = rebuilt_arg_type;
    call_type = new_call_type.get();
    rebinder->allocated_types.emplace_back(std::move(new_call_type));
    rebuilt_type = arg_type;
  }
  LOG_IF(INFO, pragmas->log_bindings())
      << "BIND LOG: " << FunctionNameForLog() << " Argument " << arg_name
      << " from: " << arg_type->full_name() << " to "
      << rebuilt_type->full_name();
  type_arguments.emplace_back(TypeBindingArg{rebuilt_type});
  call_expressions.emplace_back(call_arg.value);
  names.emplace_back(arg_name);
  return absl::OkStatus();
}

absl::Status FunctionBinding::UseRemainingArguments(
    const std::vector<FunctionCallArgument>& arguments) {
  // TODO(catalin): here we would use these if varargs are accepted
  //  by function.
  if (arg_index < arguments.size()) {
    return status::InvalidArgumentErrorBuilder()
           << "There are: " << (arguments.size() - arg_index)
           << " unused arguments provided for function call";
  }
  return absl::OkStatus();
}

bool FunctionBinding::IsAncestorOf(const FunctionBinding& binding) const {
  return type_spec->IsAncestorOf(*binding.type_spec);
}

bool FunctionBinding::IsEqual(const FunctionBinding& binding) const {
  return type_spec->IsEqual(*binding.type_spec);
}

absl::StatusOr<std::unique_ptr<FunctionBinding>> Function::BindArguments(
    const std::vector<FunctionCallArgument>& arguments) {
  LOG_IF(INFO, pragma_handler()->log_bindings())
      << "BIND LOG: " << call_name() << " Start argument binding";
  // First look up the existing bindings:
  std::vector<TypeBindingArg> type_arguments;
  type_arguments.reserve(arguments.size());
  for (size_t i = 0; i < arguments.size(); ++i) {
    ASSIGN_OR_RETURN(const TypeSpec* call_type, arguments[i].ArgType(),
                     _ << "Obtaining type for call argument " << i);
    type_arguments.emplace_back(TypeBindingArg{call_type});
  }
  const std::string type_signature = TypeSignature(type_arguments);
  auto it = bindings_map_.find(type_signature);
  if (it == bindings_map_.end()) {
    if (binding_parent_.has_value()) {
      LOG_IF(INFO, pragma_handler()->log_bindings())
          << "BIND LOG: " << call_name() << " Binding in parent";
      return binding_parent_.value()->BindArguments(arguments);
    }
    LOG_IF(INFO, pragma_handler()->log_bindings())
        << "BIND LOG: " << call_name() << " Binding in this";
    return FunctionBinding::Bind(this, arguments);
  }
  LOG_IF(INFO, pragma_handler()->log_bindings())
      << "BIND LOG: " << call_name()
      << " Binding in existing bind: " << it->second->full_name();
  // Bind directly to the function that we know would match directly :)
  return FunctionBinding::Bind(it->second, arguments);
}

absl::Status Function::Bind(FunctionBinding* binding) {
  RET_CHECK(binding->fun == this)
      << "Expecting to call the bind on same function to "
         "which arguments were bound"
      << kBugNotice;
  RET_CHECK(binding->call_expressions.size() == arguments_.size())
      << "Badly built function binding" << kBugNotice;
  RET_CHECK(binding->type_arguments.size() == arguments_.size())
      << "Badly built function binding" << kBugNotice;

  if (is_native()) {
    return absl::OkStatus();
  }

  const std::string type_signature = TypeSignature(binding->type_arguments);

  // TODO(catalin): this method of binding is partly annoying, because
  //   we basically produce a new function that needs to be defined in
  //   the original module for each call on the function, so we cannot
  //   keep a predefined library and redo just the binding.
  Function* bound_function = nullptr;
  auto it = bindings_map_.find(type_signature);
  if (it != bindings_map_.end()) {
    bound_function = it->second;
    LOG_IF(INFO, pragma_handler()->log_bindings())
        << "BIND LOG: " << call_name() << " Using Old bind: " << type_signature
        << " => " << bound_function->full_name();
  } else {
    CHECK_GT(scope_name().size(), 1ul);
    ASSIGN_OR_RETURN(
        ScopeName bind_name,
        scope_name()
            .PrefixScopeName(scope_name().size() - 1)
            .Subfunction(module_scope()->NextBindingName(call_name())),
        _ << "Creating function bind name" << kBugNotice);
    auto bind_instance = absl::WrapUnique(
        new Function(std::make_shared<ScopeName>(std::move(bind_name)),
                     function_name_, kind_, parent_, definition_scope_));
    RETURN_IF_ERROR(bind_instance->InitBindInstance(type_signature, binding))
        << "Binding function instance to type signature" << kBugNotice;
    RETURN_IF_ERROR(
        parent_->AddChildStore(bind_instance->call_name(), bind_instance.get()))
        << "Adding type-specific binding of function: " << call_name() << ", "
        << bind_instance->full_name() << " to parent store";
    bound_function = bind_instance.get();
    bindings_map_.emplace(type_signature, bound_function);
    bindings_set_.emplace(bound_function);
    bindings_.emplace_back(std::move(bind_instance));
    LOG_IF(INFO, pragma_handler()->log_bindings())
        << "BIND LOG: " << call_name() << " Created new bind " << type_signature
        << " => " << bound_function->full_name();
  }
  RETURN_IF_ERROR(bound_function->BuildFunctionBody())
      << "Binding call arguments to function instance of " << call_name()
      << " - bound instance: " << bound_function->full_name();
  binding->fun = bound_function;
  binding->type_spec = bound_function->type_spec();
  return absl::OkStatus();
}

absl::Status Function::InitBindInstance(absl::string_view type_signature,
                                        FunctionBinding* binding) {
  RET_CHECK(!binding_parent_.has_value());
  RET_CHECK(binding->fun.has_value());
  binding_parent_ = binding->fun;
  size_t size = binding->call_expressions.size();
  RET_CHECK(size == binding->fun.value()->arguments().size()) << kBugNotice;
  RET_CHECK(size == binding->type_arguments.size()) << kBugNotice;
  RET_CHECK(size == binding->names.size()) << kBugNotice;
  type_signature_ = type_signature;
  for (size_t i = 0; i < binding->fun.value()->arguments().size(); ++i) {
    RET_CHECK(
        std::holds_alternative<const TypeSpec*>(binding->type_arguments[i]));
    auto arg = std::make_unique<Argument>(
        binding->names[i],
        std::get<const TypeSpec*>(binding->type_arguments[i]), this);
    RETURN_IF_ERROR(AddChildStore(binding->names[i], arg.get()));
    default_values_.emplace_back(binding->fun.value()->default_values()[i]);
    arguments_map_.emplace(binding->names[i], arg.get());
    arguments_.emplace_back(std::move(arg));
  }
  first_default_value_index_ =
      binding->fun.value()->first_default_value_index();
  // For now just bind the source function return type:
  RETURN_IF_ERROR(UpdateFunctionType(binding->fun.value()->result_type()));
  function_body_ = binding->fun.value()->function_body();

  return absl::OkStatus();
}

absl::Status Function::BuildFunctionBody() {
  if (!expressions_.empty()) {
    return absl::OkStatus();  // already built
  }
  RET_CHECK(function_body_ != nullptr) << kBugNotice;
  ASSIGN_OR_RETURN(
      auto expression, BuildExpressionBlock(*function_body_, true),
      _ << "In function definition of: `" << function_name() << "`");
  expressions_.emplace_back(std::move(expression));
  RETURN_IF_ERROR(UpdateFunctionTypeOnResults());
  return absl::OkStatus();
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

pb::FunctionDefinitionSpec Function::ToProto() const {
  pb::FunctionDefinitionSpec proto;
  *proto.mutable_scope_name() = scope_name().ToProto();
  proto.set_kind(kind());
  for (size_t i = 0; i < arguments_.size(); ++i) {
    auto param = proto.add_parameter();
    param->set_name(arguments_[i]->name());
    *param->mutable_type_spec() = arguments_[i]->type_spec()->ToProto();
    if (default_values_[i].has_value()) {
      *param->mutable_default_value() = default_values_[i].value()->ToProto();
    }
  }
  *proto.mutable_result_type() = result_type()->ToProto();
  proto.set_function_name(function_name());
  *proto.mutable_qualified_name() = qualified_call_name().ToProto();
  if (!expressions_.empty()) {
    for (const auto& expression : expressions_) {
      *proto.add_body() = expression->ToProto();
    }
  } else if (is_native()) {
    for (auto it : native_impl_) {
      auto snippet = proto.add_native_snippet();
      snippet->set_name(it.first);
      snippet->set_body(it.second);
    }
  }
  for (const auto& binding : bindings_) {
    *proto.add_binding() = binding->ToProto();
  }
  return proto;
}

std::string Function::DebugString() const {
  std::string prefix;
  switch (kind()) {
    case pb::ObjectKind::OBJ_FUNCTION:
      prefix = absl::StrCat("def ", call_name());
      break;
    case pb::ObjectKind::OBJ_METHOD:
      prefix = absl::StrCat("def method ", call_name());
      break;
    case pb::ObjectKind::OBJ_LAMBDA:
      break;
    default:
      prefix = "_UNKNOWN_";
  }
  std::vector<std::string> args;
  args.reserve(arguments_.size());
  for (size_t i = 0; i < arguments_.size(); ++i) {
    args.emplace_back(absl::StrCat("  ", arguments_[i]->name(), ": ",
                                   arguments_[i]->type_spec()->full_name()));
    if (default_values_[i].has_value()) {
      absl::StrAppend(&args.back(), " = ",
                      default_values_[i].value()->DebugString());
    }
  }
  std::vector<std::string> body;
  if (!expressions_.empty()) {
    for (const auto& expression : expressions_) {
      body.emplace_back(Reindent(expression->DebugString()));
    }
  } else if (is_native()) {
    for (auto it : native_impl_) {
      body.emplace_back(
          absl::StrCat("$$", it.first, "\n", it.second, "\n$$end"));
    }
  } else {
    body.emplace_back("pass;   // Unbound");
  }
  std::vector<std::string> result;
  result.emplace_back(absl::StrCat(prefix, "(\n", absl::StrJoin(args, "\n"),
                                   "\n) => ", result_type()->full_name(),
                                   " {\n", absl::StrJoin(body, "\n"), "\n}"));
  for (const auto& binding : bindings_) {
    result.emplace_back(binding->DebugString());
  }
  return absl::StrJoin(result, "\n");
}
}  // namespace analysis
}  // namespace nudl
