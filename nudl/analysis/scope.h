#ifndef NUDL_ANALYSIS_SCOPE_H__
#define NUDL_ANALYSIS_SCOPE_H__

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "nudl/analysis/errors.h"
#include "nudl/analysis/named_object.h"
#include "nudl/analysis/names.h"
#include "nudl/analysis/type_spec.h"
#include "nudl/analysis/type_store.h"
#include "nudl/proto/analysis.pb.h"
#include "nudl/proto/dsl.pb.h"

namespace nudl {
namespace analysis {

class Expression;
class Function;
struct FunctionBinding;
class PragmaHandler;
class VarBase;

// An argument provided to a function call.
struct FunctionCallArgument {
  // The provided name of the argument. Empty if no name.
  absl::optional<std::string> name;
  // The value provided for the argument.
  absl::optional<Expression*> value;
  // Alternate can provide a type:
  absl::optional<const TypeSpec*> type_spec;

  bool is_valid() { return value.has_value() || type_spec.has_value(); }
  // Determines the type of this argument - from value or type_spec.
  absl::StatusOr<const TypeSpec*> ArgType(
      absl::optional<const TypeSpec*> type_hint = {}) const;
};

// This represents a programming scope, which is a place
// where names can be defined through expressions.
class Scope : public BaseNameStore {
 public:
  // This is to build a top level scope.
  explicit Scope(Scope* built_in_scope = nullptr);
  // This builds a child scope scope.
  Scope(std::shared_ptr<ScopeName> scope_name, Scope* parent,
        bool is_module = false);

  virtual ~Scope();

  // Name of this scope.
  const ScopeName& scope_name() const;
  // Parent scope of this one:
  Scope* parent() const;
  // The scope at the top of the scope tree.
  Scope* top_scope() const;
  // The ancestor scope at the module level.
  Scope* module_scope() const;
  // If this is a module type of scope
  bool is_module() const;
  // Returns the handler of pragmas for this Scope
  PragmaHandler* pragma_handler() const;

  // The scopes that contain all the built-in functions.
  Scope* built_in_scope() const;
  // The type store used by this scope tree:
  GlobalTypeStore* type_store() const;
  // Name of this scope in pointer form.
  std::shared_ptr<ScopeName> scope_name_ptr() const;
  // Expressions defined within this scope.
  const std::vector<std::unique_ptr<Expression>>& expressions() const;
  // Adds an expression into the scope - mainly for testing.
  void add_expression(std::unique_ptr<Expression> expression);

  // The type of name object we are: kScope
  pb::ObjectKind kind() const override;

  // We add a little more details here:
  std::string full_name() const override;

  // Normally returning the type of last expression.
  const TypeSpec* type_spec() const override;

  // This returns the parent_ scope.
  absl::optional<NameStore*> parent_store() const override;

  // Finds a named expression that is looked up this scope.
  absl::StatusOr<NamedObject*> FindName(const ScopeName& lookup_scope,
                                        const ScopedName& scoped_name) override;

  // Adds a child scope to this one. We expect our scope name to
  // be a prefix in the provided scope_name.
  absl::Status AddSubScope(std::unique_ptr<Scope> scope);

  // Adds a variable based object defined in this scope.
  absl::Status AddDefinedVar(std::unique_ptr<VarBase> var_base);

  // Finds a type that is looked up this scope.
  absl::StatusOr<const TypeSpec*> FindType(const pb::TypeSpec& type_spec);
  // Parses the provided name and returns the type.
  absl::StatusOr<const TypeSpec*> FindTypeByName(absl::string_view type_name);
  // Returs a local name for unnamed scopes and objects.
  static inline constexpr absl::string_view kLocalNamePrefix = "__local_";
  std::string NextLocalName(absl::string_view name,
                            absl::string_view prefix = kLocalNamePrefix);

  // Returns the next name for binding the specified function name.
  // This should be called at module level.
  std::string NextBindingName(absl::string_view name);

  // Builds an expression object from the provided proto.
  absl::StatusOr<std::unique_ptr<Expression>> BuildExpression(
      const pb::Expression& expression);

  // Builds all expressions from the provided expression block, and
  // adds them to the returned expression block.
  // If register_return, we register the last expression as a result to
  // the bottom-most function.
  absl::StatusOr<std::unique_ptr<Expression>> BuildExpressionBlock(
      const pb::ExpressionBlock& expression_block,
      bool register_return = false);

  // Finds the closest ancestor which is a function kind.
  // Returns null if none.
  absl::optional<Function*> FindFunctionAncestor();

  // Finds a function with the provided name, first looking for a member
  // of the type_spec, if not null.
  absl::StatusOr<std::unique_ptr<FunctionBinding>> FindFunctionByName(
      const ScopedName& name, const TypeSpec* type_spec,
      const std::vector<FunctionCallArgument>& arguments);

  // As we use this a lot, and we really expect to have it available, we wrap
  // Any type finding in a no-fail but CHECK function.
  const TypeSpec* FindTypeAny();
  const TypeSpec* FindTypeFunction();
  const TypeSpec* FindTypeBool();
  const TypeSpec* FindTypeInt();
  const TypeSpec* FindTypeUnion();

  static bool IsScopeKind(const NamedObject& object);

 protected:
  // Validates if the provided scope name/object can be assigned in this scope.
  virtual absl::StatusOr<VarBase*> ValidateAssignment(
      const ScopedName& name, NamedObject* object) const;

  // Processes the name in an assignment expression, maybe creating a new one.
  absl::StatusOr<std::tuple<VarBase*, bool>> ProcessVarFind(
      const ScopedName& name, const pb::Assignment& element,
      Expression* assign_expression, const CodeContext& context);

  // Processes the assignment element in this scope.
  absl::Status ProcessAssignment(const pb::Assignment& element,
                                 const CodeContext& context);

  absl::StatusOr<std::unique_ptr<Expression>> BuildAssignment(
      const pb::Assignment& element, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildLiteral(
      const pb::Literal& element, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildIdentifier(
      const pb::Identifier& element, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildFunctionResult(
      const pb::Expression* result_expression,
      pb::FunctionResultKind result_kind, const CodeContext& context);

  absl::StatusOr<std::unique_ptr<Expression>> BuildOperator(
      const pb::OperatorExpression& element, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildUnaryOperator(
      const pb::OperatorExpression& element, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildTernaryOperator(
      const pb::OperatorExpression& element, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildBinaryOperator(
      const pb::OperatorExpression& element, const CodeContext& context);
  // Once the operands of an operator expression are determined, this
  // builds the actual operator call.
  absl::StatusOr<std::unique_ptr<Expression>> BuildOperatorCall(
      absl::string_view name, std::vector<std::unique_ptr<Expression>> operands,
      const CodeContext& context);

  absl::StatusOr<std::unique_ptr<Expression>> BuildArrayDefinition(
      const pb::ArrayDefinition& array_def, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildMapDefinition(
      const pb::MapDefinition& map_def, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildIfEspression(
      const pb::IfExpression& if_expr, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildIndexExpression(
      const pb::IndexExpression& expression, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildLambdaExpression(
      const pb::FunctionDefinition& expression, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildDotExpression(
      const pb::DotExpression& expression, const CodeContext& context);
  absl::StatusOr<std::unique_ptr<Expression>> BuildFunctionCall(
      const pb::FunctionCall& expression,
      std::unique_ptr<Expression> left_expression, const CodeContext& context);

  absl::StatusOr<std::unique_ptr<Expression>> BuildFunctionApply(
      std::unique_ptr<FunctionBinding> apply_function,
      std::unique_ptr<Expression> left_expression,
      std::vector<std::unique_ptr<Expression>> arguments, bool is_method_call,
      const CodeContext& context);

  std::shared_ptr<ScopeName> const scope_name_;
  Scope* const parent_;
  Scope* const top_scope_;
  Scope* const built_in_scope_;
  Scope* const module_scope_;
  // This is set only by the top scope.
  std::unique_ptr<GlobalTypeStore> const top_type_store_;
  GlobalTypeStore* const type_store_;

  size_t next_name_id_ = 0;

  std::vector<std::unique_ptr<NamedObject>> defined_names_;
  std::vector<std::unique_ptr<Expression>> expressions_;
  // This is more for obtaining better names in bindings.
  absl::flat_hash_map<std::string, size_t> binding_name_index_;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_SCOPE_H__
