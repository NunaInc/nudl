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
#ifndef NUDL_ANALYSIS_FUNCTION_H__
#define NUDL_ANALYSIS_FUNCTION_H__

#include <memory>
#include <string>
#include <utility>
#include <vector>


#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nudl/analysis/errors.h"
#include "nudl/analysis/expression.h"
#include "nudl/analysis/scope.h"
#include "nudl/analysis/type_spec.h"
#include "nudl/analysis/vars.h"
#include "nudl/proto/analysis.pb.h"
#include "nudl/proto/dsl.pb.h"

namespace nudl {
namespace analysis {

class Function;

// This represents a binding to a function call after checking the
// arguments, their types and names.
struct FunctionBinding {
  // The type of the function that is to be called.
  const TypeFunction* fun_type = nullptr;
  // The function object that was bound to the call - may be missed.
  absl::optional<Function*> fun;
  // The types of the arguments.
  std::vector<TypeBindingArg> type_arguments;
  // The expressions for the arguments - some may be default values.
  // Has the same size as type_arguments.
  std::vector<absl::optional<Expression*>> call_expressions;
  // Specific bindings done for each arguments, in case they are functions.
  std::vector<absl::optional<FunctionBinding*>> call_sub_bindings;
  // If the corresponding expression comes from a default value.
  std::vector<bool> is_default_value;
  // The names of the arguments. Has the same size as type_arguments.
  std::vector<std::string> names;

  // The bound type of the function resulted from the call. This is
  // obtained by binding the type_arguments to the fun->type_spec(),
  // using the default return type of the function.
  const TypeSpec* type_spec = nullptr;

  // NOTE:
  //    type_arguments, call_expressions, call_sub_bindings and names
  //    All have the same size. Checked by this:
  // May switch to a single vector, of a struct, but they are harder to
  // fill that that way.. to see.
  void CheckCounts() const;

  // Tries to bind a vector of arguments to a function object.
  // TODO(catalin): Can probably change arguments to absl::Span<const...>
  static absl::StatusOr<std::unique_ptr<FunctionBinding>> Bind(
      Function* fun, const std::vector<FunctionCallArgument>& arguments,
      std::vector<std::unique_ptr<FunctionBinding>>* failed_bindings);
  static absl::StatusOr<std::unique_ptr<FunctionBinding>> BindType(
      const TypeFunction* fun_type, const PragmaHandler* pragmas,
      const std::vector<FunctionCallArgument>& arguments,
      std::vector<std::unique_ptr<FunctionBinding>>* failed_bindings);

  // If this binding is less specific than the provided binding.
  bool IsAncestorOf(const FunctionBinding& binding) const;
  bool IsEqual(const FunctionBinding& binding) const;
  std::string FunctionNameForLog() const;
  std::string full_name() const;

 private:
  FunctionBinding(const TypeFunction* fun_type, const PragmaHandler* pragmas);
  explicit FunctionBinding(Function* fun);
  absl::Status BindImpl(const std::vector<FunctionCallArgument>& arguments);
  absl::Status BindDefaultValue(absl::string_view arg_name,
                                const TypeSpec* arg_type,
                                LocalNamesRebinder* rebinder);
  absl::Status BindArgument(absl::string_view arg_name,
                            const TypeSpec* arg_type,
                            const FunctionCallArgument& call_arg,
                            LocalNamesRebinder* rebinder);
  absl::Status UseRemainingArguments(
      const std::vector<FunctionCallArgument>& arguments);
  absl::StatusOr<std::pair<const TypeSpec*, std::optional<FunctionBinding*>>>
  RebindFunctionArgument(absl::string_view arg_name,
                         const FunctionCallArgument& call_arg,
                         const TypeSpec* call_type,
                         const TypeSpec* rebuilt_type);

  // Storage for return type and other types created during the binding
  // process.
  std::vector<std::unique_ptr<TypeSpec>> stored_types;
  std::vector<std::unique_ptr<FunctionBinding>> sub_bindings;
  const PragmaHandler* pragmas;
  size_t num_args = 0;
  size_t fun_index = 0;
  size_t arg_index = 0;
};

class FunctionGroup : public Scope {
 public:
  // This groups together functions defined with the same name,
  // but different argument signature.
  FunctionGroup(std::shared_ptr<ScopeName> scope_name, Scope* parent,
                bool is_method_group);

  pb::ObjectKind kind() const override;
  const TypeSpec* type_spec() const override;
  const std::vector<Function*> functions() const;

  // The last component of the name:
  std::string call_name() const;
  // The fully qualified name.
  ScopedName qualified_call_name() const;

  absl::Status AddFunction(Function* fun);
  absl::StatusOr<ScopeName> GetNextFunctionName();

  absl::StatusOr<std::unique_ptr<FunctionBinding>> FindSignature(
      const std::vector<FunctionCallArgument>& arguments) const;

  // Find the function in this group that is, or binds the provided
  // Function object.
  absl::optional<Function*> FindBinding(const Function* fun) const;

  std::string DebugString() const override;

  // If this corresponds to the 'main' function in a binary.
  bool is_main() const;

  static bool IsFunctionGroup(const NamedObject& object);

 private:
  absl::StatusOr<std::vector<std::unique_ptr<FunctionBinding>>> TryBindFunction(
      Function* function, const std::vector<FunctionCallArgument>& arguments,
      std::vector<std::unique_ptr<FunctionBinding>>* existing) const;

  const bool is_method_group_;
  std::vector<Function*> functions_;
  std::vector<std::unique_ptr<TypeSpec>> types_;
  bool is_main_ = false;
};

inline constexpr absl::string_view kConstructorName = "__init__";

class Function : public Scope {
 public:
  // Builds a function from definition, and adds it to the parent scope.
  // Returns a reference pointer to the newly added function - which
  // upon return is owned by the
  static absl::StatusOr<Function*> BuildInScope(
      Scope* parent, const pb::FunctionDefinition& element,
      absl::string_view lambda_name, const CodeContext& context);

  ~Function() override;

  // This returns the type of the function.
  const TypeSpec* type_spec() const override;
  // The name used to define this function:
  const std::string& function_name() const;
  // The name registered in the scope for call purpose.
  // May be different from function_name, as it can be a function
  // with the same name, or an instance of some sort.
  std::string call_name() const;
  // The fully qualified name of the function including module.
  ScopedName qualified_call_name() const;

  // Recomposes the function signature from name, arguments, and return type:
  std::string full_name() const override;
  // The kind of this object.
  pb::ObjectKind kind() const override;
  // The way in which the result is produced by the function.
  pb::FunctionResultKind result_kind() const;
  // The function group this function belongs to:
  FunctionGroup* function_group() const;
  // The scope in which the function was defined.
  Scope* definition_scope() const;
  // All specific type bindings:
  const std::vector<std::unique_ptr<Function>>& bindings() const;
  // A set with all bindings:
  const absl::flat_hash_map<Function*, std::pair<size_t, std::string>>&
  bindings_by_function() const;
  // A map from binding signature to bound function:
  const absl::flat_hash_map<std::string,
                            std::pair<std::optional<size_t>, Function*>>&
  bindings_by_name() const;
  // Returns true if fun is a binding of this function (or itself):
  bool IsBinding(const Function* fun) const;

  // Argument definitions of this function.
  const std::vector<std::unique_ptr<VarBase>>& arguments() const;
  // Default values provided for arguments.
  const std::vector<absl::optional<Expression*>>& default_values() const;
  // The index at which we start having default values:
  absl::optional<size_t> first_default_value_index() const;

  // Return type of the function.
  const TypeSpec* result_type() const;

  // The body of the function in proto format, if non native.
  std::shared_ptr<pb::ExpressionBlock> function_body() const;

  // If this function does not have a known concrete implementation
  // in itself (though it may have some concrete bindings, for specific
  // types)
  bool is_abstract() const;

  // If the function has a native implementation
  bool is_native() const;

  // A special case of native - for structure constructs:
  bool is_struct_constructor() const;
  // If this native function should not be converted:
  bool is_skip_conversion() const;

  // TypeSignature of this function / corresponding binding:
  std::string type_signature() const;
  // The possibly abstract parent that bound this function with types.
  absl::optional<Function*> binding_parent() const;

  // The native implementation blocks:
  const absl::flat_hash_map<std::string, std::string>& native_impl() const;

  // If all arguments and return values of the function are bound.
  // bool IsBound() const;

  // If contains undefined typed argument.
  bool HasUndefinedArgTypes() const;

  // Creates a new function in which arguments and types are bound
  // to bound types. Possibly updates the binding->function to
  // a newly created instance
  absl::StatusOr<Function*> Bind(FunctionBinding* binding,
                                 bool update_function);

  // TODO(catalin): Can probably change arguments to absl::Span<const...>
  absl::StatusOr<std::unique_ptr<FunctionBinding>> BindArguments(
      const std::vector<FunctionCallArgument>& arguments);

  // If provided named object is a function.
  static bool IsFunctionKind(const NamedObject& object);
  static bool IsMethodKind(const NamedObject& object);
  static bool IsFunctionMainKind(const NamedObject& object);
  // Returns a name for a result kind of a function.
  static absl::string_view ResultKindName(pb::FunctionResultKind result_kind);

  // Registers a result generating expression with the function.
  absl::Status RegisterResultExpression(pb::FunctionResultKind result_kind,
                                        Expression* expression,
                                        bool accept_unknown_type);

  pb::FunctionDefinitionSpec ToProto() const;
  std::string DebugString() const override;

  // Next functions are public for testing only:

  absl::StatusOr<VarBase*> ValidateAssignment(
      const ScopedName& name, NamedObject* object) const override;

  // Adds this function as a member method of provided type.
  absl::Status AddAsMethod(const TypeSpec* member_type);

 protected:
  Function(std::shared_ptr<ScopeName> scope_name,
           absl::string_view function_name, pb::ObjectKind object_kind,
           FunctionGroup* parent, Scope* definition_scope);

  // Next functions are used during initialization.

  // Initialization for a function definition - called from Build.
  absl::Status InitializeDefinition(const pb::FunctionDefinition& element,
                                    const CodeContext& context);
  // Initializes next argument from a proto parameter.
  absl::Status InitializeParameterDefinition(
      const pb::FunctionParameter& param);
  // Initializes this function as a method of the first argument type.
  absl::Status InitializeAsMethod();
  // Initializes this function as a constructor for result_type.
  absl::Status InitializeAsConstructor(const TypeSpec* result_type);

  // Another way to initialize a function, as a bind instance from
  // the function in binding->fun; type_signature provided as a shortcut.
  absl::Status InitBindInstance(Function* binding_parent,
                                absl::string_view type_signature,
                                FunctionBinding* binding);

  // Builds the expression from function_body, and binds the computed
  // result type.
  absl::Status BuildFunctionBody();

  // Used to check the result kind of a new return expression
  absl::StatusOr<pb::FunctionResultKind> RegisterResultKind(
      pb::FunctionResultKind result_kind);

  // Initializes / updates the type of this function (function_type_spec)
  // based on the existing arguments_ and provided result type.
  absl::Status UpdateFunctionType(const TypeSpec* result_type);

  // Updates the return type of the function based on the returned
  // values during the function.
  absl::Status UpdateFunctionTypeOnResults();

  // The defined function name - note that name_ may be different,
  // based on instance.
  std::string function_name_;
  // Function group it belongs to - same as parent, but already typed.
  FunctionGroup* const function_group_;
  // The scope where the function was defined - may not be the parent_.
  Scope* const definition_scope_;
  // The kind of the function:
  const pb::ObjectKind kind_;
  // Arguments of the function, in order:
  // Note: we use the term 'argument' for actual function parameter,
  // to differentiate from module-level config parameters.
  std::vector<std::unique_ptr<VarBase>> arguments_;
  // Default values for the arguments - sime may be null.
  std::vector<absl::optional<Expression*>> default_values_;
  std::vector<std::unique_ptr<Expression>> default_values_store_;
  // The first index for which we have default values for parameters:
  absl::optional<size_t> first_default_value_index_;
  // Arguments of the function, mapped from their name:
  absl::flat_hash_map<std::string, VarBase*> arguments_map_;
  // The current type signature of this function - reference.
  const TypeSpec* type_spec_ = nullptr;
  // A reference to type any, so we don't return null result type.
  const TypeSpec* type_any_ = nullptr;
  // Ad-hoc types created during return type negotiation.
  std::vector<std::unique_ptr<TypeSpec>> created_type_specs_;
  // The signature of the argument types to this function.
  std::string type_signature_;
  // The contents of the body of the function.
  // This is set for unbound functions, and expressions are initialized
  // upon calls, with bound parameters.
  std::shared_ptr<pb::ExpressionBlock> function_body_;
  // If has native implementation $$...$$end, this has elements:
  absl::flat_hash_map<std::string, std::string> native_impl_;

  // Expressions returning values in this function.
  // For PASS we have nulls registered as expressions.
  struct ResultExpression {
    pb::FunctionResultKind result_kind = pb::FunctionResultKind::RESULT_NONE;
    Expression* expression = nullptr;
    const TypeSpec* type_spec = nullptr;
  };
  std::vector<ResultExpression> result_expressions_;
  // The way in which the function produces the result:
  pb::FunctionResultKind result_kind_ = pb::FunctionResultKind::RESULT_NONE;
  // If the return type of this function was negotiated:
  bool result_type_negotiated_ = false;

  // If we are a bound function, this points to the 'parent' from
  // which we were bound.:
  absl::optional<Function*> binding_parent_;
  // Functions instantiated per type Bind calls.
  std::vector<std::unique_ptr<Function>> bindings_;
  // Maps from bound function to bind index and binding signature.
  absl::flat_hash_map<Function*, std::pair<size_t, std::string>>
      bindings_by_function_;
  // Map from binding type signature to bound function and index in parent.
  absl::flat_hash_map<std::string, std::pair<std::optional<size_t>, Function*>>
      bindings_by_name_;
  // Binds that failed at some point, keep them around for unified destruction.
  std::vector<std::unique_ptr<Function>> failed_instances_;
};

// Annotations for semi-native structure implementations, which
// have to be dynamically generated during conversion:
inline constexpr absl::string_view kStructObjectConstructor =
    "__struct_object_constructor__";
inline constexpr absl::string_view kStructCopyConstructor =
    "__struct_copy_constructor__";
// Native tag to add for skipping the default conversion of a function:
inline constexpr absl::string_view kFunctionSkipConversion = "skip_conversion";

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_FUNCTION_H__
