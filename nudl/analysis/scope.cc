#include "nudl/analysis/scope.h"

#include <typeindex>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "glog/logging.h"
#include "nudl/analysis/expression.h"
#include "nudl/analysis/function.h"
#include "nudl/analysis/module.h"
#include "nudl/analysis/pragma.h"
#include "nudl/analysis/types.h"
#include "nudl/analysis/vars.h"
#include "nudl/grammar/dsl.h"
#include "nudl/status/status.h"

namespace nudl {
namespace analysis {

Scope::Scope(Scope* built_in_scope)
    : BaseNameStore(""),
      scope_name_(std::make_shared<ScopeName>()),
      parent_(nullptr),
      top_scope_(this),
      built_in_scope_(built_in_scope ? built_in_scope : this),
      module_scope_(this),
      // Note: this is something delicate and to be considered:
      //   - do we want to pass the built-in type store as the type
      //   store of these scopes? For flexibility, probably yes, as is now.
      top_type_store_(built_in_scope ? nullptr
                                     : std::make_unique<GlobalTypeStore>()),
      type_store_(built_in_scope ? built_in_scope->type_store()
                                 : top_type_store_.get()) {}

Scope::Scope(std::shared_ptr<ScopeName> scope_name, Scope* parent,
             bool is_module)
    : BaseNameStore(scope_name->name()),
      scope_name_(std::move(scope_name)),
      parent_(CHECK_NOTNULL(parent)),
      top_scope_(CHECK_NOTNULL(parent->top_scope())),
      built_in_scope_(CHECK_NOTNULL(parent->built_in_scope())),
      module_scope_(is_module ? this : CHECK_NOTNULL(parent->module_scope())),
      type_store_(CHECK_NOTNULL(parent->type_store())) {}

Scope::~Scope() {
  // We need to release scopes and expressions in reverse order.
  // May add a utility for that.
  while (!expressions_.empty()) {
    expressions_.pop_back();
  }
  while (!defined_names_.empty()) {
    defined_names_.pop_back();
  }
}

pb::ObjectKind Scope::kind() const { return pb::ObjectKind::OBJ_SCOPE; }

const ScopeName& Scope::scope_name() const { return *scope_name_; }

const TypeSpec* Scope::type_spec() const {
  if (!expressions_.empty()) {
    auto type_spec = expressions_.back()->stored_type_spec();
    if (type_spec.has_value()) {
      return type_spec.value();
    }
  }
  return TypeUnknown::Instance();
}

absl::optional<NameStore*> Scope::parent_store() const {
  if (!parent_) {
    return {};
  }
  return parent_;
}

std::shared_ptr<ScopeName> Scope::scope_name_ptr() const { return scope_name_; }

Scope* Scope::parent() const { return parent_; }

Scope* Scope::top_scope() const { return top_scope_; }

Scope* Scope::module_scope() const { return module_scope_; }

bool Scope::is_module() const { return module_scope_ == this; }

Scope* Scope::built_in_scope() const { return built_in_scope_; }

GlobalTypeStore* Scope::type_store() const { return type_store_; }

PragmaHandler* Scope::pragma_handler() const {
  CHECK_EQ(CHECK_NOTNULL(module_scope())->kind(), pb::ObjectKind::OBJ_MODULE);
  return static_cast<Module*>(module_scope())->pragma_handler();
}

const std::vector<std::unique_ptr<Expression>>& Scope::expressions() const {
  return expressions_;
}

void Scope::add_expression(std::unique_ptr<Expression> expression) {
  expressions_.emplace_back(std::move(expression));
}

std::string Scope::full_name() const {
  std::string s(BaseNameStore::full_name());
  if (built_in_scope_ == this) {
    absl::StrAppend(&s, " [Built in Scope]");
  }
  if (top_scope_ == this) {
    absl::StrAppend(&s, " [Top Scope]");
  }
  return s;
}

std::string Scope::NextLocalName(absl::string_view name,
                                 absl::string_view prefix) {
  ++next_name_id_;
  return absl::StrCat(prefix, name, "_", next_name_id_);
}

std::string Scope::NextBindingName(absl::string_view name) {
  auto it = binding_name_index_.find(name);
  if (it == binding_name_index_.end()) {
    it = binding_name_index_.emplace(name, 0ul).first;
  }
  return absl::StrCat(name, "__bind_", ++it->second);
}

absl::Status Scope::AddSubScope(std::unique_ptr<Scope> scope) {
  if (!scope_name_->IsPrefixScope(scope->scope_name())) {
    return status::InvalidArgumentErrorBuilder()
           << "Expected child scope name: " << scope->name() << " to "
           << "be prefixed by the parent name: " << name();
  }
  RETURN_IF_ERROR(type_store_->AddScope(scope->scope_name_ptr()))
      << "Creating child type store for: " << scope->name()
      << " while adding to parent: " << name();
  const std::string local_name(
      scope->scope_name().SuffixName(scope_name_->size()));
  RETURN_IF_ERROR(AddChildStore(local_name, scope.get()));
  defined_names_.emplace_back(std::move(scope));
  return absl::OkStatus();
}

absl::Status Scope::AddOwnedChildStore(absl::string_view local_name,
                                       std::unique_ptr<NameStore> store) {
  if (IsScopeKind(*store)) {
    return AddSubScope(
        std::unique_ptr<Scope>(static_cast<Scope*>(store.release())));
  }
  return BaseNameStore::AddOwnedChildStore(local_name, std::move(store));
}

absl::Status Scope::AddDefinedVar(std::unique_ptr<VarBase> var_base) {
  CHECK_EQ(var_base->parent_store().value_or(nullptr), this)
      << " Parent store of: " << var_base->full_name()
      << " expected to be: " << full_name();
  if (!NameUtil::IsValidName(var_base->name())) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid name for: " << var_base->full_name()
           << " to add to scope: " << full_name();
  }
  RETURN_IF_ERROR(AddChildStore(var_base->name(), var_base.get()));
  defined_names_.emplace_back(std::move(var_base));
  return absl::OkStatus();
}

absl::StatusOr<Scope*> Scope::AddNewLocalScope(absl::string_view local_name) {
  ASSIGN_OR_RETURN(auto local_scope_name,
                   scope_name().Subname(NextLocalName(local_name)),
                   _ << "Adding local scope name: " << local_name);
  auto local_scope = std::make_unique<Scope>(
      std::make_shared<ScopeName>(std::move(local_scope_name)), this, false);
  auto local_scope_ptr = local_scope.get();
  RETURN_IF_ERROR(AddSubScope(std::move(local_scope)))
      << "Adding local scope: " << local_name;
  return local_scope_ptr;
}

absl::StatusOr<NamedObject*> Scope::FindName(const ScopeName& lookup_scope,
                                             const ScopedName& scoped_name) {
  std::vector<absl::Status> find_status;
  absl::optional<Function*> local_function;
  std::vector<NamedObject*> result;
  if (lookup_scope.name() == name()) {
    local_function = FindFunctionAncestor();
    if (scoped_name.scope_name().empty()) {
      if (HasName(scoped_name.name(), false)) {
        return GetName(scoped_name.name(), false);
      } else {
        find_status.emplace_back(status::StatusWriter(absl::NotFoundError(""))
                                 << "Cannot find name: `" << scoped_name.name()
                                 << "` in local " << name());
      }
    } else {
      // Finally search it here:
      auto scope_result = FindChildStore(scoped_name.scope_name());
      if (scope_result.ok()) {
        const bool is_unaccessible_function =
            (Function::IsFunctionKind(*scope_result.value()) &&
             (!local_function.has_value() ||
              local_function.value() != scope_result.value()));
        if (scope_result.value()->HasName(scoped_name.name(), false)) {
          if (!is_unaccessible_function || scope_result.value() == this) {
            return scope_result.value()->GetName(scoped_name.name(), false);
          } else {
            find_status.emplace_back(
                status::StatusWriter(absl::NotFoundError(""))
                << "Found name: " << scoped_name.name()
                << " in function: " << scope_result.value()->name()
                << " cannot be accessed from scope: " << lookup_scope.name());
          }
        }
        if (!is_unaccessible_function) {
          // TODO(catalin): Here find closest names, 'Did you mean ...'
          find_status.emplace_back(
              status::StatusWriter(absl::NotFoundError(""))
              << "Cannot find name: `" << scoped_name.name()
              << "` in child name store " << scope_result.value()->name()
              << "; Available names: "
              << absl::StrJoin(scope_result.value()->DefinedNames(), ", "));
        }
      } else {
        find_status.emplace_back(status::StatusWriter(absl::NotFoundError(""))
                                 << "Cannot find name store: `"
                                 << scoped_name.scope_name().name()
                                 << "` in local " << name());
      }
    }
  }
  for (size_t i = lookup_scope.size() + 1; i > 0; --i) {
    const ScopeName prefix_scope(lookup_scope.PrefixScopeName(i - 1));
    if (!prefix_scope.function_names().empty() &&
        (!scoped_name.scope_name().module_name().empty())) {
      // May actually bail out directly on the !function_names().empty()
      continue;
    }
    const ScopeName crt_name(prefix_scope.Subscope(scoped_name.scope_name()));
    auto result = top_scope()->FindChildStore(crt_name);
    if (result.ok()) {
      const bool is_unaccessible_function =
          (Function::IsFunctionKind(*result.value()) &&
           (!local_function.has_value() ||
            local_function.value() != result.value()));
      if (result.value()->HasName(scoped_name.name(), false)) {
        if (!is_unaccessible_function || result.value() == this) {
          return result.value()->GetName(scoped_name.name(), false);
        } else {
          find_status.emplace_back(status::StatusWriter(absl::NotFoundError(""))
                                   << "Found name: " << scoped_name.name()
                                   << " in function: " << result.value()->name()
                                   << " cannot be accessed from scope: "
                                   << lookup_scope.name());
        }
      } else if (!result.value()->name().empty() &&
                 (!is_unaccessible_function || result.value() == this)) {
        // TODO(catalin): Here find closest names, 'Did you mean ...'
        find_status.emplace_back(
            status::StatusWriter(absl::NotFoundError(""))
            << "Cannot find name: `" << scoped_name.name() << "` in name store "
            << result.value()->name() << " from: " << crt_name.name()
            << " available names: "
            << absl::StrJoin(result.value()->DefinedNames(), ", "));
      }
    }
  }
  if (built_in_scope() && built_in_scope() != this) {
    auto result = built_in_scope()->FindName(ScopeName(), scoped_name);
    if (result.ok()) {
      return result;
    }
  }
  if (find_status.empty()) {
    find_status.emplace_back(status::StatusWriter(absl::NotFoundError(""))
                             << "Cannot find name: `" << scoped_name.full_name()
                             << "` looked up in scope: `" << lookup_scope.name()
                             << "`");
  }
  if (scoped_name.scope_name().function_names().empty()) {
    auto type_result =
        type_store_->FindType(lookup_scope, scoped_name.ToTypeSpec());
    if (type_result.ok()) {
      return {const_cast<TypeSpec*>(type_result.value())};
    }
    find_status.emplace_back(status::StatusWriter(absl::NotFoundError(""))
                             << "Cannot find type name: `"
                             << scoped_name.full_name() << "` either");
  }
  return status::JoinStatus(find_status);
}

absl::StatusOr<const TypeSpec*> Scope::FindType(const pb::TypeSpec& type_spec) {
  return type_store_->FindType(scope_name(), type_spec);
}

absl::StatusOr<const TypeSpec*> Scope::FindTypeByName(
    absl::string_view type_name) {
  ASSIGN_OR_RETURN(auto type_spec, grammar::ParseTypeSpec(type_name),
                   _ << "For type_name: `" << type_name << "`");
  return FindType(*type_spec);
}

const TypeSpec* Scope::FindTypeAny() {
  auto result = FindTypeByName(kTypeNameAny);
  CHECK_OK(result.status())
      << "Cannot find type `Any` - Badly initialized scope" << kBugNotice;
  return result.value();
}

const TypeSpec* Scope::FindTypeFunction() {
  auto result = FindTypeByName(kTypeNameFunction);
  CHECK_OK(result.status())
      << "Cannot find type `Function` - Badly initialized scope" << kBugNotice;
  return result.value();
}

const TypeSpec* Scope::FindTypeBool() {
  auto result = FindTypeByName(kTypeNameBool);
  CHECK_OK(result.status())
      << "Cannot find type `Bool` - Badly initialized scope" << kBugNotice;
  return result.value();
}

const TypeSpec* Scope::FindTypeInt() {
  auto result = FindTypeByName(kTypeNameInt);
  CHECK_OK(result.status())
      << "Cannot find type `Int` - Badly initialized scope" << kBugNotice;
  return result.value();
}

const TypeSpec* Scope::FindTypeUnion() {
  auto result = FindTypeByName(kTypeNameUnion);
  CHECK_OK(result.status())
      << "Cannot find type `Union` - Badly initialized scope" << kBugNotice;
  return result.value();
}

const TypeSpec* Scope::FindTypeGenerator() {
  auto result = FindTypeByName(kTypeNameGenerator);
  CHECK_OK(result.status())
      << "Cannot find type `Generator` - Badly initialized scope" << kBugNotice;
  return result.value();
}

namespace {
bool IsScopeObjectKind(pb::ObjectKind kind) {
  static const auto* const kFunctionKinds =
      new absl::flat_hash_set<pb::ObjectKind>({
          pb::ObjectKind::OBJ_SCOPE,
          pb::ObjectKind::OBJ_METHOD,
          pb::ObjectKind::OBJ_CONSTRUCTOR,
          pb::ObjectKind::OBJ_FUNCTION,
          pb::ObjectKind::OBJ_FUNCTION_GROUP,
          pb::ObjectKind::OBJ_MODULE,
          pb::ObjectKind::OBJ_LAMBDA,
      });
  return kFunctionKinds->contains(kind);
}
}  // namespace

bool Scope::IsScopeKind(const NamedObject& object) {
  return IsScopeObjectKind(object.kind());
}

absl::optional<Function*> Scope::FindFunctionAncestor() {
  Scope* scope = this;
  while (scope && scope->parent() != scope) {
    if (Function::IsFunctionKind(*scope)) {
      return static_cast<Function*>(scope);
    }
    scope = scope->parent();
  }
  return {};
}

namespace {
absl::StatusOr<std::unique_ptr<FunctionBinding>> FindFunctionInStore(
    NameStore* store, const ScopeName& lookup_scope, const ScopedName& name,
    const std::vector<FunctionCallArgument>& arguments) {
  std::unique_ptr<FunctionBinding> binding;
  ASSIGN_OR_RETURN(
      auto object, store->FindName(lookup_scope, name),
      _ << "Finding function " << name.name() << " in " << store->full_name());
  if (Function::IsFunctionKind(*object)) {
    ASSIGN_OR_RETURN(binding,
                     static_cast<Function*>(object)->BindArguments(arguments),
                     _ << "Binding call arguments to function " << name.name()
                       << " from " << store->full_name());
    return {std::move(binding)};
  } else if (FunctionGroup::IsFunctionGroup(*object)) {
    ASSIGN_OR_RETURN(
        binding, static_cast<FunctionGroup*>(object)->FindSignature(arguments),
        _ << "Finding signature to bind to function " << name.name() << " from "
          << store->full_name());
    return {std::move(binding)};
  }
  return status::NotFoundErrorBuilder()
         << "The found object: " << object->full_name() << " in "
         << store->full_name() << " is not a function";
}
}  // namespace

absl::StatusOr<std::unique_ptr<FunctionBinding>> Scope::FindFunctionByName(
    const ScopedName& name, const TypeSpec* type_spec,
    const std::vector<FunctionCallArgument>& arguments) {
  std::vector<absl::Status> find_status;
  std::vector<absl::Status> bind_status;
  if (type_spec && type_spec->type_member_store()) {
    auto type_stores = type_spec->type_member_store()->FindBindingOrder();
    for (auto type_store : type_stores) {
      auto find_result =
          FindFunctionInStore(type_store, scope_name(), name, arguments);
      if (find_result.ok()) {
        return find_result;
      }
      if (absl::IsNotFound(find_result.status())) {
        find_status.emplace_back(std::move(find_result).status());
      } else {
        bind_status.emplace_back(std::move(find_result).status());
      }
    }
  }
  auto find_result = FindFunctionInStore(this, scope_name(), name, arguments);
  if (find_result.ok()) {
    return find_result;
  }
  if (absl::IsNotFound(find_result.status())) {
    find_status.emplace_back(std::move(find_result).status());
  } else {
    bind_status.emplace_back(std::move(find_result).status());
  }
  if (!bind_status.empty()) {
    return status::JoinStatus(bind_status);
  }
  return status::JoinStatus(find_status);
}

namespace {
bool IsParameterDefined(const pb::Assignment& element) {
  for (auto qualifier : element.qualifier()) {
    if (qualifier == pb::QualifierType::QUAL_PARAM) {
      return true;
    }
  }
  return false;
}
absl::Status CheckNoRedefinitions(const VarBase* var_base,
                                  const pb::Assignment& element,
                                  const CodeContext& context) {
  if (element.has_type_spec()) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot redefine type in reassignment for: "
           << var_base->full_name()
           << context.ToErrorInfo("Redefining variable");
  }
  if (!element.qualifier().empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot use qualifiers in reassignment for: "
           << var_base->full_name()
           << context.ToErrorInfo("Redefining variable");
  }
  return absl::OkStatus();
}
}  // namespace

absl::StatusOr<std::tuple<VarBase*, bool>> Scope::ProcessVarFind(
    const ScopedName& name, const pb::Assignment& element,
    Expression* assign_expression, const CodeContext& context) {
  auto find_result = FindName(scope_name(), name);
  if (find_result.ok()) {
    NamedObject* scoped_object = find_result.value();
    ASSIGN_OR_RETURN(auto var_base, ValidateAssignment(name, scoped_object),
                     _ << context.ToErrorInfo("In assignment expression"));
    RETURN_IF_ERROR(CheckNoRedefinitions(var_base, element, context));
    return std::make_tuple(var_base, false);
  }
  if (!name.scope_name().empty()) {
    return {status::StatusWriter(find_result.status()) << context.ToErrorInfo(
                "Cannot find name in assignment expression")};
  }
  // We try to add the variable:
  const TypeSpec* type_spec = nullptr;
  if (element.has_type_spec()) {
    ASSIGN_OR_RETURN(type_spec, FindType(element.type_spec()),
                     _ << context.ToErrorInfo(absl::StrCat(
                         "Finding type for assignment of: ", name.name())));
  } else {
    ASSIGN_OR_RETURN(
        type_spec, CHECK_NOTNULL(assign_expression)->type_spec(),
        _ << context.ToErrorInfo(absl::StrCat(
            "Determining type of assing expression for: ", name.name())));
  }
  if (type_spec->type_id() == pb::TypeId::FUNCTION_ID &&
      !type_spec->IsBound()) {
    absl::flat_hash_set<std::string> unbound_types;
    TypeUtils::FindUnboundTypes(type_spec, &unbound_types);
    return status::InvalidArgumentErrorBuilder()
           << "In definition of: " << name.name()
           << ", when defining a "
              "variable typed as a Function, this type needs to be bound. "
              "Please add non-abstract type specifications to all arguments "
              "and define the return value as well if necessary. Type found: "
           << type_spec->full_name()
           << " unbound types: " << absl::StrJoin(unbound_types, ", ");
  }
  std::unique_ptr<VarBase> new_var;
  if (IsParameterDefined(element)) {
    new_var = std::make_unique<Parameter>(name.name(), CHECK_NOTNULL(type_spec),
                                          this);
  } else {
    new_var =
        std::make_unique<Var>(name.name(), CHECK_NOTNULL(type_spec), this);
  }
  VarBase* var_base = new_var.get();  // save before the move:
  RETURN_IF_ERROR(AddDefinedVar(std::move(new_var)))
      << context.ToErrorInfo("Defining a new variable in scope.");
  return std::make_tuple(var_base, true);
}

absl::StatusOr<VarBase*> Scope::ValidateAssignment(const ScopedName& name,
                                                   NamedObject* object) const {
  if (!VarBase::IsVarKind(*object)) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot assign an object of this kind: " << object->full_name();
  }
  // TODO(catalin): differentiate the scopes and such in overrides.
  return static_cast<VarBase*>(object);
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildExpression(
    const pb::Expression& expression) {
  CodeContext context = CodeContext::FromProto(expression);
  std::string expression_type;
  if (expression.has_literal()) {
    return BuildLiteral(expression.literal(), context);
  } else if (expression.has_identifier()) {
    return BuildIdentifier(expression.identifier(), context);
  } else if (expression.has_operator_expr()) {
    return BuildOperator(expression.operator_expr(), context);
  } else if (expression.has_function_call()) {
    return BuildFunctionCall(expression.function_call(), nullptr, context);
  } else if (expression.has_dot_expr()) {
    return BuildDotExpression(expression.dot_expr(), context);
  } else if (expression.has_index_expr()) {
    return BuildIndexExpression(expression.index_expr(), context);
  } else if (expression.has_lambda_def()) {
    return BuildLambdaExpression(expression.lambda_def(), context);
  } else if (expression.has_if_expr()) {
    return BuildIfExpression(expression.if_expr(), context);
  } else if (expression.has_array_def()) {
    return BuildArrayDefinition(expression.array_def(), context);
  } else if (expression.has_map_def()) {
    return BuildMapDefinition(expression.map_def(), context);
  } else if (expression.has_assignment()) {
    return BuildAssignment(expression.assignment(), context);
  } else if (expression.has_yield_expr()) {
    return BuildFunctionResult(&expression.yield_expr(),
                               pb::FunctionResultKind::RESULT_YIELD, context);
  } else if (expression.has_return_expr()) {
    return BuildFunctionResult(&expression.return_expr(),
                               pb::FunctionResultKind::RESULT_RETURN, context);
  } else if (expression.has_pass_expr()) {
    return BuildFunctionResult({}, pb::FunctionResultKind::RESULT_PASS,
                               context);
  } else if (expression.has_empty_struct()) {
    return std::make_unique<EmptyStruct>(this);
  } else if (expression.has_pragma_expr()) {
    ASSIGN_OR_RETURN(
        auto nop_expression,
        pragma_handler()->HandlePragma(this, expression.pragma_expr()),
        _ << context.ToErrorInfo("In pragma expression"));
    return {std::move(nop_expression)};
  } else if (expression.has_error()) {
    return status::FailedPreconditionErrorBuilder()
           << "Parse error detected: " << expression.error().description()
           << context.ToErrorInfo("In expression");
  } else if (expression.has_with_expr()) {
    return status::UnimplementedErrorBuilder()
           << "`with` expression not implemented yet"
           << context.ToErrorInfo("In with expression");
  }
  return status::InvalidArgumentErrorBuilder()
         << "Improper expression built"
         << context.ToErrorInfo("For expression");
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildAssignment(
    const pb::Assignment& element, const CodeContext& context) {
  ASSIGN_OR_RETURN(ScopedName name,
                   ScopedName::FromIdentifier(element.identifier()),
                   _ << context.ToErrorInfo("Invalid assign identifier"));
  ASSIGN_OR_RETURN(auto expression, BuildExpression(element.value()),
                   _ << context.ToErrorInfo("Building assign expression"));
  VarBase* var_base;
  bool is_initial_assignment;
  // cannot use auto [..] in ASSIGN_OR_RETURN - tbd.
  ASSIGN_OR_RETURN(
      std::tie(var_base, is_initial_assignment),
      ProcessVarFind(name, element, expression.get(), context),
      _ << context.ToErrorInfo("Finding or building assigned var"));
  ASSIGN_OR_RETURN(auto converted_expression,
                   var_base->Assign(std::move(expression)),
                   _ << "In assignment of: " << var_base->full_name()
                     << context.ToErrorInfo("Type mismatch in assignment"));
  // TODO(catalin): binding - do I want to have this here - see how this
  //   plays out w/ functions.
  // if (!var_base->type_spec()->IsBound()) {
  //   return status::InvalidArgumentErrorBuilder()
  //      << "Defined variables cannot be of an unbound type for "
  //      << var_base->full_name() << " - "
  //      << var_base->type_spec()->full_name();
  // }
  return {std::make_unique<Assignment>(
      this, std::move(name), var_base, std::move(converted_expression),
      element.has_type_spec(), is_initial_assignment)};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildLiteral(
    const pb::Literal& element, const CodeContext& context) {
  ASSIGN_OR_RETURN(auto expression, Literal::Build(this, element),
                   _ << "Building literal from `" << element.original() << "`"
                     << context.ToErrorInfo("In literal expression"));
  return {std::move(expression)};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildIdentifier(
    const pb::Identifier& element, const CodeContext& context) {
  ASSIGN_OR_RETURN(auto scoped_name, ScopedName::FromIdentifier(element),
                   _ << context.ToErrorInfo("In identifier name"));
  ASSIGN_OR_RETURN(auto named_object, FindName(scope_name(), scoped_name),
                   _ << context.ToErrorInfo("Finding identifier in scope"));
  RET_CHECK(named_object->type_spec()) << kBugNotice;
  if (Function::IsMethodKind(*named_object)) {
    if (element.name().size() > 1) {
      pb::Identifier object_identifier(element);
      object_identifier.mutable_name()->RemoveLast();
      ASSIGN_OR_RETURN(auto left_expression,
                       BuildIdentifier(object_identifier, context));
      return std::make_unique<DotAccessExpression>(
          this, std::move(left_expression), *element.name().rbegin(),
          named_object);
    }  // TODO(catalin): add a case for `this` if present
  }
  // TODO(catalin): Do I want to enable special treatment
  //    if (FunctionGroup::IsFunctionGroup(*named_object)) {..}
  return {
      std::make_unique<Identifier>(this, std::move(scoped_name), named_object)};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildFunctionResult(
    absl::optional<const pb::Expression*> result_expression,
    pb::FunctionResultKind result_kind, const CodeContext& context) {
  auto parent_function = FindFunctionAncestor();
  if (!parent_function.has_value()) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot " << Function::ResultKindName(result_kind)
           << " outside of a function scope."
           << context.ToErrorInfo("In result passing expression");
  }
  absl::optional<std::unique_ptr<Expression>> inner_expression;
  if (result_expression.has_value()) {
    ASSIGN_OR_RETURN(inner_expression,
                     BuildExpression(*result_expression.value()));
    // TODO(catalin): Here may be time to massage the type of inner_expression
    //  to maybe convert to parent_function.value()->result_type().
  }
  auto expression = std::make_unique<FunctionResultExpression>(
      this, parent_function.value(), result_kind, std::move(inner_expression));
  RETURN_IF_ERROR(parent_function.value()->RegisterResultExpression(
      result_kind, expression.get(), false))
      << "Registering " << Function::ResultKindName(result_kind)
      << " expression "
         " with function: "
      << parent_function.value()->full_name()
      << context.ToErrorInfo("In function return value");
  return {std::move(expression)};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildOperator(
    const pb::OperatorExpression& element, const CodeContext& context) {
  RET_CHECK(!element.op().empty() && !element.argument().empty())
      << "Badly built operator expression: " << element.op().size()
      << " operators " << element.argument().size() << " arguments "
      << kBugNotice << context.ToErrorInfo("In operator expression");
  std::unique_ptr<Expression> expression;
  if (element.argument().size() == 1) {
    return BuildUnaryOperator(element, context);
  } else if (element.op().size() == 1 && element.argument().size() == 3) {
    return BuildTernaryOperator(element, context);
  } else {
    return BuildBinaryOperator(element, context);
  }
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildUnaryOperator(
    const pb::OperatorExpression& element, const CodeContext& context) {
  RET_CHECK(element.op().size() == 1 && element.argument().size() == 1)
      << "Badly built unary operator expression: " << element.op().size()
      << " operators " << element.argument().size() << " arguments "
      << kBugNotice << context.ToErrorInfo("In unary operator expression");

  static const auto* const kUnaryOperators =
      new absl::flat_hash_map<std::string, std::string>({
          {"+", "__pos__"},
          {"-", "__neg__"},
          {"~", "__inv__"},
          {"not", "__not__"},
      });
  auto it = kUnaryOperators->find(element.op(0));
  if (it == kUnaryOperators->end()) {
    return status::InvalidArgumentErrorBuilder()
           << "Unknown unary operator: " << element.op(0)
           << context.ToErrorInfo("In unary operator expression");
  }
  ASSIGN_OR_RETURN(auto operand, BuildExpression(element.argument(0)));
  std::vector<std::unique_ptr<Expression>> operands;
  operands.emplace_back(std::move(operand));
  return BuildOperatorCall(it->second, std::move(operands), context);
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildTernaryOperator(
    const pb::OperatorExpression& element, const CodeContext& context) {
  RET_CHECK(element.op().size() == 1 && element.argument().size() == 3);
  static const auto* const kTernaryOperators =
      new absl::flat_hash_map<std::string, std::string>({
          {"?", "__if__"},
          {"between", "__between__"},
      });
  auto it = kTernaryOperators->find(element.op(0));
  if (it == kTernaryOperators->end()) {
    return status::InvalidArgumentErrorBuilder()
           << "Unknown ternary operator: " << element.op(0)
           << context.ToErrorInfo("In ternary operator expression");
  }
  std::vector<std::unique_ptr<Expression>> operands;
  for (const auto& arg : element.argument()) {
    ASSIGN_OR_RETURN(auto operand, BuildExpression(arg));
    operands.emplace_back(std::move(operand));
  }
  return BuildOperatorCall(it->second, std::move(operands), context);
}

namespace {
std::vector<std::unique_ptr<Expression>> BuildOperands(
    std::unique_ptr<Expression> left, std::unique_ptr<Expression> right) {
  std::vector<std::unique_ptr<Expression>> result;
  result.emplace_back(std::move(left));
  result.emplace_back(std::move(right));
  return result;
}
}  // namespace

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildBinaryOperator(
    const pb::OperatorExpression& element, const CodeContext& context) {
  RET_CHECK(element.op().size() >= 1 &&
            element.argument().size() == element.op().size() + 1)
      << "Badly built binary operator expression: " << element.op().size()
      << " operators " << element.argument().size() << " arguments "
      << kBugNotice << context.ToErrorInfo("In binary operator expression");
  static const auto* const kBinaryOperators =
      new absl::flat_hash_map<std::string, std::pair<std::string, bool>>({
          {"*", {"__mul__", false}},     {"/", {"__div__", false}},
          {"%", {"__mod__", false}},     {"+", {"__add__", false}},
          {"-", {"__sub__", false}},     {"<<", {"__lshift__", false}},
          {">>", {"__rshift__", false}}, {"<", {"__lt__", true}},
          {">", {"__gt__", true}},       {"<=", {"__le__", true}},
          {">=", {"__ge__", true}},      {"==", {"__eq__", true}},
          {"!=", {"__ne__", true}},      {"&", {"__bit_and__", false}},
          {"^", {"__bit_xor__", false}}, {"|", {"__bit_or__", false}},
          {"and", {"__and__", false}},   {"xor", {"__xor__", false}},
          {"or", {"__or__", false}},
      });
  std::unique_ptr<Expression> last_operand;
  for (int i = 0; i < element.op().size(); ++i) {
    auto it = kBinaryOperators->find(element.op(i));
    if (it == kBinaryOperators->end()) {
      return status::InvalidArgumentErrorBuilder()
             << "Unknown binary operator: " << element.op(0)
             << context.ToErrorInfo("In binary operator expression");
    }
    if (it->second.second) {
      ASSIGN_OR_RETURN(auto left_operand, BuildExpression(element.argument(i)));
      ASSIGN_OR_RETURN(auto right_operand,
                       BuildExpression(element.argument(i + 1)));
      ASSIGN_OR_RETURN(
          auto op_operand,
          BuildOperatorCall(
              it->second.first,
              BuildOperands(std::move(left_operand), std::move(right_operand)),
              context));
      if (last_operand) {
        ASSIGN_OR_RETURN(
            last_operand,
            BuildOperatorCall(
                "__and__",
                BuildOperands(std::move(last_operand), std::move(op_operand)),
                context));
      } else {
        last_operand = std::move(op_operand);
      }
    } else {
      std::vector<std::unique_ptr<Expression>> operands;
      if (!last_operand) {
        CHECK_EQ(i, 0);
        ASSIGN_OR_RETURN(last_operand, BuildExpression(element.argument(i)));
      }
      ASSIGN_OR_RETURN(auto right_operand,
                       BuildExpression(element.argument(i + 1)));
      ASSIGN_OR_RETURN(
          last_operand,
          BuildOperatorCall(
              it->second.first,
              BuildOperands(std::move(last_operand), std::move(right_operand)),
              context));
    }
  }
  return {std::move(CHECK_NOTNULL(last_operand))};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildOperatorCall(
    absl::string_view name, std::vector<std::unique_ptr<Expression>> operands,
    const CodeContext& context) {
  RET_CHECK(!operands.empty());
  ASSIGN_OR_RETURN(auto scoped_name, ScopedName::Parse(name));
  ASSIGN_OR_RETURN(auto type_spec, operands.front()->type_spec(),
                   _ << "Determining type of first operand"
                     << context.ToErrorInfo("Applying operator on operands"));
  std::vector<FunctionCallArgument> arguments;
  arguments.reserve(operands.size());
  for (const auto& op : operands) {
    arguments.emplace_back(FunctionCallArgument{{}, op.get(), {}});
  }
  ASSIGN_OR_RETURN(auto op_function,
                   FindFunctionByName(scoped_name, type_spec, arguments),
                   _ << "Finding operator function: " << name
                     << context.ToErrorInfo("Applying operator on operands"));
  return BuildFunctionApply(std::move(op_function), nullptr,
                            std::move(operands), false, context);
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildFunctionApply(
    std::unique_ptr<FunctionBinding> apply_function,
    std::unique_ptr<Expression> left_expression,
    std::vector<std::unique_ptr<Expression>> argument_expressions,
    bool is_method_call, const CodeContext& context) {
  if (apply_function->fun.has_value()) {
    RETURN_IF_ERROR(apply_function->fun.value()->Bind(apply_function.get()))
        << "Binding function instance"
        << context.ToErrorInfo("Applying function call");
  }
  return absl::make_unique<FunctionCallExpression>(
      this, std::move(apply_function), std::move(left_expression),
      std::move(argument_expressions), is_method_call);
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildArrayDefinition(
    const pb::ArrayDefinition& array_def, const CodeContext& context) {
  if (array_def.element().empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "Empty array definition not allowed" << kBugNotice
           << context.ToErrorInfo("In array definition");
  }
  std::vector<std::unique_ptr<Expression>> elements;
  elements.reserve(array_def.element().size());
  for (const auto& element : array_def.element()) {
    ASSIGN_OR_RETURN(auto expression, BuildExpression(element),
                     _ << "In array element definition");
    elements.emplace_back(std::move(expression));
  }
  return std::make_unique<ArrayDefinitionExpression>(this, std::move(elements));
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildMapDefinition(
    const pb::MapDefinition& map_def, const CodeContext& context) {
  if (map_def.element().empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "Empty map definition not allowed" << kBugNotice
           << context.ToErrorInfo("In map definition");
  }
  std::vector<std::unique_ptr<Expression>> elements;
  elements.reserve(map_def.element().size() * 2);
  for (const auto& element : map_def.element()) {
    if (!element.has_key() || !element.has_value()) {
      return status::InvalidArgumentErrorBuilder()
             << "Map element missing key " << kBugNotice
             << context.ToErrorInfo("In map element definition");
    }
    ASSIGN_OR_RETURN(auto key_element, BuildExpression(element.key()),
                     _ << "In map element definition of key");
    ASSIGN_OR_RETURN(auto value_element, BuildExpression(element.value()),
                     _ << "In map element definition of value");
    elements.emplace_back(std::move(key_element));
    elements.emplace_back(std::move(value_element));
  }
  return std::make_unique<MapDefinitionExpression>(this, std::move(elements));
}

namespace {
bool IsResultReturnExpression(const pb::Expression& expression) {
  return (expression.has_yield_expr() || expression.has_return_expr() ||
          expression.has_pass_expr());
}
bool MustUseFunctionCallResultType(const TypeSpec* type_spec) {
  static const auto kTypesNoUse = new absl::flat_hash_set<pb::TypeId>(
      {pb::TypeId::UNKNOWN_ID, pb::TypeId::NULL_ID});
  return !kTypesNoUse->contains(static_cast<pb::TypeId>(type_spec->type_id()));
}
}  // namespace

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildExpressionBlock(
    const pb::ExpressionBlock& expression_block, bool register_return) {
  RET_CHECK(!expression_block.expression().empty());
  absl::Status build_status;
  std::vector<std::unique_ptr<Expression>> expressions;
  bool last_is_result = false;
  Expression* last_expression = nullptr;
  int last_expression_index = 0;
  bool contains_return = false;
  for (int i = 0; i < expression_block.expression_size(); ++i) {
    const auto& expression = expression_block.expression(i);
    if (contains_return) {
      CodeContext context = CodeContext::FromProto(expression);
      MergeErrorStatus(status::InvalidArgumentErrorBuilder()
                           << "Meaningless expression after function return"
                           << context.ToErrorInfo("In expression block"),
                       build_status);
    }
    auto expression_result = BuildExpression(expression);
    MergeErrorStatus(expression_result.status(), build_status);
    if (expression_result.ok()) {
      if (expression_result.value()->expr_kind() !=
          pb::ExpressionKind::EXPR_NOP) {
        last_expression = expression_result.value().get();
        last_expression_index = i;
      }
      if (expression_result.value()->ContainsFunctionExit()) {
        contains_return = true;
      }
      // TODO(catalin): maybe negotiate types here..
      expressions.emplace_back(std::move(expression_result).value());
      last_is_result = IsResultReturnExpression(expression);
    }
  }
  RETURN_IF_ERROR(build_status);
  RET_CHECK(!expressions.empty());
  const size_t max_size =
      register_return ? expressions.size() - 1ul : expressions.size();
  CHECK_LE(max_size, expressions.size());
  for (size_t i = 0; i < max_size; ++i) {
    CodeContext context =
        CodeContext::FromProto(expression_block.expression(i));
    ASSIGN_OR_RETURN(auto type_spec, expressions[i]->type_spec(),
                     _ << "Determining result type of expression");
    if (expressions[i]->expr_kind() == pb::ExpressionKind::EXPR_FUNCTION_CALL &&
        MustUseFunctionCallResultType(type_spec)) {
      return status::FailedPreconditionErrorBuilder()
             << "Meaningful result of function / operator call returning "
             << type_spec->full_name() << " is unused"
             << context.ToErrorInfo("In function call expression");
    }
  }
  if (register_return && !last_is_result && !contains_return) {
    if (last_expression == nullptr) {
      return status::InvalidArgumentErrorBuilder()
             << "Expression block that needs to produce something, does "
                "not have any proper expressions defined";
    }
    auto parent_function = FindFunctionAncestor();
    RET_CHECK(parent_function.has_value())
        << "Expecting to be inside a function " << kBugNotice;
    CodeContext context = CodeContext::FromProto(
        expression_block.expression(last_expression_index));
    RETURN_IF_ERROR(parent_function.value()->RegisterResultExpression(
        pb::FunctionResultKind::RESULT_NONE, last_expression, contains_return))
        << context.ToErrorInfo("Registering default function return");
    if (last_expression->stored_type_spec().has_value() &&
        (last_expression->stored_type_spec().value()->type_id() !=
         pb::TypeId::UNKNOWN_ID)) {
      last_expression->set_is_default_return();
    }
  }
  return {std::make_unique<ExpressionBlock>(this, std::move(expressions))};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildIfExpression(
    const pb::IfExpression& if_expr, const CodeContext& context) {
  if (if_expr.condition().empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "No condition provided" << kBugNotice
           << context.ToErrorInfo("In if expression");
  }
  if (if_expr.condition().size() != if_expr.expression_block().size() &&
      (if_expr.condition().size() + 1 != if_expr.expression_block().size())) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid number of conditions and expressions provided: "
           << if_expr.condition().size() << " conditions "
           << if_expr.expression_block().size() << " expressions " << kBugNotice
           << context.ToErrorInfo("In if expression");
  }
  std::vector<std::unique_ptr<Expression>> conditions;
  std::vector<std::unique_ptr<Expression>> expressions;
  conditions.reserve(if_expr.condition().size());
  expressions.reserve(if_expr.expression_block().size());
  const TypeSpec* bool_type = FindTypeBool();
  for (int i = 0; i < if_expr.condition().size(); ++i) {
    ASSIGN_OR_RETURN(auto condition, BuildExpression(if_expr.condition(i)),
                     _ << "In if expression condition " << (i + 1));
    // TODO(catalin): plug auto conversion here
    ASSIGN_OR_RETURN(auto condition_type, condition->type_spec(bool_type),
                     _ << "Determining type of if condition " << (i + 1));
    if (!bool_type->IsEqual(*condition_type)) {
      return status::InvalidArgumentErrorBuilder()
             << "If statement condition " << (i + 1)
             << " does not return a boolean value but: "
             << condition_type->full_name()
             << context.ToErrorInfo("In if expression");
    }
    ASSIGN_OR_RETURN(auto if_scope, AddNewLocalScope("ifexpr"),
                     _ << context.ToErrorInfo("In if expression"));
    ASSIGN_OR_RETURN(
        auto expression,
        if_scope->BuildExpressionBlock(if_expr.expression_block(i)),
        _ << "In if expression branch expression " << (i + 1));
    conditions.emplace_back(std::move(condition));
    expressions.emplace_back(std::move(expression));
  }
  // Leftover else - maybe
  if (if_expr.condition().size() < if_expr.expression_block().size()) {
    ASSIGN_OR_RETURN(auto if_scope, AddNewLocalScope("ifexpr"),
                     _ << context.ToErrorInfo("In if expression"));
    ASSIGN_OR_RETURN(auto expression,
                     if_scope->BuildExpressionBlock(
                         if_expr.expression_block(if_expr.condition().size())),
                     _ << "In else expression branch expression");
    expressions.emplace_back(std::move(expression));
  }
  return {std::make_unique<IfExpression>(this, std::move(conditions),
                                         std::move(expressions))};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildIndexExpression(
    const pb::IndexExpression& expression, const CodeContext& context) {
  ASSIGN_OR_RETURN(auto object_expression,
                   BuildExpression(expression.object()));
  ASSIGN_OR_RETURN(auto index_expression, BuildExpression(expression.index()));
  ASSIGN_OR_RETURN(const TypeSpec* object_type, object_expression->type_spec(),
                   _ << "Obtaining indexed object type"
                     << context.ToErrorInfo("In indexed expression"));
  std::unique_ptr<Expression> result_expression;
  if (object_type->type_id() == pb::TypeId::TUPLE_ID) {
    RETURN_IF_ERROR(
        index_expression->type_spec(CHECK_NOTNULL(object_type->IndexType()))
            .status())
        << "Determining type of index expression"
        << context.ToErrorInfo("In indexed expression");
    auto index_value = index_expression->StaticValue();
    size_t index;
    if (index_value.has_value() && std::type_index(index_value.type()) ==
                                       std::type_index(typeid(int64_t))) {
      index = static_cast<size_t>(std::any_cast<int64_t>(index_value));
    } else if (index_value.has_value() &&
               std::type_index(index_value.type()) ==
                   std::type_index(typeid(uint64_t))) {
      index = static_cast<size_t>(std::any_cast<uint64_t>(index_value));
    } else {
      return status::InvalidArgumentErrorBuilder()
             << "Tuples require a static integer index"
             << context.ToErrorInfo("In tuple indexed expression");
    }
    if (index >= object_type->parameters().size()) {
      return status::InvalidArgumentErrorBuilder()
             << "Tuples index: " << index
             << " out of tuple type range: " << object_type->parameters().size()
             << " for type: " << object_type->full_name()
             << context.ToErrorInfo("In tuple indexed expression");
    }
    result_expression = absl::make_unique<TupleIndexExpression>(
        this, std::move(object_expression), std::move(index_expression), index);
  } else {
    result_expression = std::make_unique<IndexExpression>(
        this, std::move(object_expression), std::move(index_expression));
  }
  RETURN_IF_ERROR(result_expression->type_spec().status())
      << "Determining the indexed object type"
      << context.ToErrorInfo("In indexed expression");
  return {std::move(result_expression)};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildLambdaExpression(
    const pb::FunctionDefinition& expression, const CodeContext& context) {
  const std::string function_name = module_scope()->NextLocalName("lambda");
  ASSIGN_OR_RETURN(
      Function * lambda_function,
      Function::BuildInScope(this, expression, function_name, context),
      _ << "Defining lambda function");
  return {std::make_unique<LambdaExpression>(this, lambda_function)};
}

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildDotExpression(
    const pb::DotExpression& expression, const CodeContext& context) {
  RET_CHECK(expression.has_left())
      << "Missing left part of expression" << kBugNotice
      << context.ToErrorInfo("In dot expression");
  ASSIGN_OR_RETURN(auto left_expression, BuildExpression(expression.left()));
  ASSIGN_OR_RETURN(const TypeSpec* left_type, left_expression->type_spec(),
                   _ << "Determining type of left part of expression"
                     << context.ToErrorInfo("In dot expression"));
  RET_CHECK(left_type->type_member_store() != nullptr)
      << "For type: " << left_type->full_name() << kBugNotice;
  if (expression.has_name()) {
    ASSIGN_OR_RETURN(auto object_name, ScopedName::Parse(expression.name()),
                     _ << context.ToErrorInfo("In dot expression name"));
    ASSIGN_OR_RETURN(
        auto object,
        left_type->type_member_store()->FindName(scope_name(), object_name),
        _ << context.ToErrorInfo("Finding dot expression name"));
    return std::make_unique<DotAccessExpression>(
        this, std::move(left_expression), expression.name(), object);
  }
  // The only way we hit here, in the way the grammar is built,
  // is if we do a module-type construct like foo.X<Y>(3)
  RET_CHECK(expression.has_function_call() &&
            !expression.function_call().has_expr_spec())
      << "Badly built dot expression" << kBugNotice;
  return BuildFunctionCall(expression.function_call(),
                           std::move(left_expression), context);
}

// Helper class for preparing a function call expression.
class FunctionCallHelper {
 public:
  FunctionCallHelper(Scope* scope, const pb::FunctionCall& expression,
                     std::unique_ptr<Expression> left_expression,
                     const CodeContext& context)
      : scope_(scope),
        expression_(expression),
        left_expression_(std::move(left_expression)),
        context_(context),
        function_name_store_(scope) {}
  absl::StatusOr<std::unique_ptr<Expression>> PrepareCall() {
    RETURN_IF_ERROR(PrepareLeftExpression());
    if (expression_.has_identifier()) {
      RETURN_IF_ERROR(PrepareIdentifier());
    } else if (expression_.has_type_spec()) {
      RETURN_IF_ERROR(PrepareTypeConstruct());
    } else if (left_expression_) {
      PrepapareObjectFromLeftExpression();
    } else {
      return absl::InvalidArgumentError("Badly built function call ");
    }
    RETURN_IF_ERROR(PrepareArguments());
    std::unique_ptr<FunctionBinding> function_binding;
    if (call_type_constructor_) {
      ASSIGN_OR_RETURN(
          function_binding,
          FindFunctionInStore(
              call_type_constructor_->type_member_store(), scope_->scope_name(),
              ScopedName::Parse(kConstructorName).value(), arguments_));
      is_method_call_ = true;
    } else if (call_object_) {
      ASSIGN_OR_RETURN(
          function_binding, BindingFromCallObject(),
          _ << "Preparing call from object: " << call_object_->full_name());
    } else {
      RET_CHECK(left_expression_ != nullptr) << "Got in a bad analysis state";
      ASSIGN_OR_RETURN(
          const TypeSpec* type_spec, left_expression_->type_spec(),
          _ << "Determining the type of function producing expression.");
      ASSIGN_OR_RETURN(function_binding, BindingFromType(type_spec));
    }
    return scope_->BuildFunctionApply(
        std::move(function_binding), std::move(left_expression_),
        std::move(argument_expressions_), is_method_call_, context_);
  }

 private:
  absl::Status PrepareLeftExpression() {
    if (expression_.has_expr_spec()) {
      RET_CHECK(left_expression_ == nullptr)
          << "Cannot provide a built left expression in a function call where "
             "expression is provided in call";
      ASSIGN_OR_RETURN(left_expression_,
                       scope_->BuildExpression(expression_.expr_spec()));
    }
    if (left_expression_) {
      ASSIGN_OR_RETURN(
          const TypeSpec* left_type, left_expression_->type_spec(),
          _ << "Determining type of the left part of call expression");
      RET_CHECK(left_type->type_member_store() != nullptr)
          << "For type: " << left_type->full_name() << kBugNotice;
      function_name_store_ = left_type->type_member_store();
    }
    return absl::OkStatus();
  }

  absl::Status PrepareIdentifier() {
    RET_CHECK(expression_.has_identifier());
    ASSIGN_OR_RETURN(object_name_,
                     ScopedName::FromIdentifier(expression_.identifier()),
                     _ << "In function name identifier");
    ASSIGN_OR_RETURN(call_object_,
                     function_name_store_->FindName(scope_->scope_name(),
                                                    object_name_.value()),
                     _ << "Finding function name");
    if (expression_.identifier().name().size() > 1) {
      // for x().y.z() need to get a handle of x().y directly:
      pb::Identifier source_object_identifier(expression_.identifier());
      source_object_identifier.mutable_name()->RemoveLast();
      ASSIGN_OR_RETURN(auto source_object_name,
                       ScopedName::FromIdentifier(source_object_identifier),
                       _ << "Building source object name" << kBugNotice);
      ASSIGN_OR_RETURN(auto source_object,
                       function_name_store_->FindName(scope_->scope_name(),
                                                      source_object_name),
                       _ << "Finding function source object" << kBugNotice);
      if (!left_expression_) {
        left_expression_ = std::make_unique<Identifier>(
            scope_, source_object_name, source_object);
      } else {
        ASSIGN_OR_RETURN(
            auto source_scope_name,
            source_object_name.scope_name().Subname(source_object_name.name()));
        left_expression_ = std::make_unique<DotAccessExpression>(
            scope_, std::move(left_expression_), std::move(source_scope_name),
            source_object);
      }
    }
    if (left_expression_) {
      method_source_expression_ = left_expression_.get();
      left_expression_ = std::make_unique<DotAccessExpression>(
          scope_, std::move(left_expression_),
          *expression_.identifier().name().rbegin(), call_object_);
    }
    return absl::OkStatus();
  }
  absl::Status PrepareTypeConstruct() {
    // this is probably a templated build.
    Scope* type_lookup_scope = scope_;
    if (left_expression_) {
      auto named_object = left_expression_->named_object();
      if (named_object.has_value() &&
          Scope::IsScopeKind(*named_object.value())) {
        type_lookup_scope = static_cast<Scope*>(named_object.value());
      }
    }
    ASSIGN_OR_RETURN(call_type_constructor_,
                     type_lookup_scope->FindType(expression_.type_spec()),
                     _ << "Finding type to construct");
    return absl::OkStatus();
  }
  void PrepapareObjectFromLeftExpression() {
    // left_expression returns a function directly, but we may need an object
    // to that.
    auto call_object_value = CHECK_NOTNULL(left_expression_)->named_object();
    if (call_object_value.has_value()) {
      if (left_expression_->expr_kind() ==
              pb::ExpressionKind::EXPR_DOT_ACCESS &&
          !left_expression_->children().empty()) {
        method_source_expression_ = left_expression_->children().front().get();
      }
      call_object_ = call_object_value.value();
    }
  }
  absl::Status PrepareArguments() {
    if (call_object_ && call_object_->kind() == pb::ObjectKind::OBJ_TYPE) {
      call_type_constructor_ = static_cast<const TypeSpec*>(call_object_);
      call_object_ = nullptr;
    }
    if (call_object_ && Function::IsMethodKind(*call_object_)) {
      // TODO(catalin): work out this case if becomes a problem
      if (method_source_expression_) {
        arguments_.emplace_back(
            FunctionCallArgument{{}, method_source_expression_, {}});
        is_method_call_ = true;
      }
    }
    for (const auto& arg : expression_.argument()) {
      FunctionCallArgument farg;
      if (arg.has_name()) {
        farg.name = arg.name();
      }
      if (!arg.has_value()) {
        return status::InvalidArgumentErrorBuilder()
               << "No value provided for function call argument " << kBugNotice;
      }
      ASSIGN_OR_RETURN(auto expression, scope_->BuildExpression(arg.value()));
      farg.value = expression.get();
      arguments_.emplace_back(std::move(farg));
      argument_expressions_.emplace_back(std::move(expression));
    }
    return absl::OkStatus();
  }
  absl::StatusOr<std::unique_ptr<FunctionBinding>> BindingFromCallObject() {
    RET_CHECK(call_object_ != nullptr) << "No call object";
    if (Function::IsFunctionKind(*call_object_)) {
      return static_cast<Function*>(call_object_)->BindArguments(arguments_);
    }
    if (FunctionGroup::IsFunctionGroup(*call_object_)) {
      return static_cast<FunctionGroup*>(call_object_)
          ->FindSignature(arguments_);
    }
    return BindingFromType(call_object_->type_spec());
  }
  absl::StatusOr<std::unique_ptr<FunctionBinding>> BindingFromType(
      const TypeSpec* type_spec) {
    if (type_spec->type_id() != pb::TypeId::FUNCTION_ID) {
      return status::InvalidArgumentErrorBuilder()
             << "Cannot call non-function type: " << type_spec->full_name();
    }
    const TypeFunction* fun_type_spec =
        static_cast<const TypeFunction*>(type_spec);
    ASSIGN_OR_RETURN(auto function_binding,
                     FunctionBinding::BindType(
                         fun_type_spec, scope_->pragma_handler(), arguments_),
                     _ << "Binding call arguments to function type: "
                       << type_spec->full_name());
    if (!left_expression_ && call_object_) {
      RET_CHECK(object_name_.has_value());
      left_expression_ = std::make_unique<Identifier>(
          scope_, std::move(object_name_).value(), call_object_);
    }
    return {std::move(function_binding)};
  }

  // These are given:
  Scope* const scope_;
  const pb::FunctionCall& expression_;
  std::unique_ptr<Expression> left_expression_;
  const CodeContext& context_;
  // Where to find the function to call:
  NameStore* function_name_store_;
  // The object representing the function call: a Function or a FunctionGroup
  NamedObject* call_object_ = nullptr;
  // If this is a type construction call, this points to the constructed type.
  const TypeSpec* call_type_constructor_ = nullptr;
  // In case the expression_ names the call object directly, this is how:
  std::optional<ScopedName> object_name_;
  // If this is a method call, this points to the object / first argument.
  Expression* method_source_expression_ = nullptr;
  // Prepared arguments for the call:
  std::vector<std::unique_ptr<Expression>> argument_expressions_;
  std::vector<FunctionCallArgument> arguments_;
  bool is_method_call_ = false;
};

absl::StatusOr<std::unique_ptr<Expression>> Scope::BuildFunctionCall(
    const pb::FunctionCall& expression,
    std::unique_ptr<Expression> left_expression, const CodeContext& context) {
  FunctionCallHelper call_helper(this, expression, std::move(left_expression),
                                 context);
  ASSIGN_OR_RETURN(auto call_expression, call_helper.PrepareCall(),
                   _ << context.ToErrorInfo("In function call"));
  return {std::move(call_expression)};
}

absl::StatusOr<const TypeSpec*> FunctionCallArgument::ArgType(
    absl::optional<const TypeSpec*> type_hint) const {
  if (type_spec.has_value()) {
    return CHECK_NOTNULL(type_spec.value());
  }
  RET_CHECK(value.has_value());
  return CHECK_NOTNULL(value.value())->type_spec(type_hint);
}

}  // namespace analysis
}  // namespace nudl
