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
#ifndef NUDL_ANALYSIS_EXPRESSION_H__
#define NUDL_ANALYSIS_EXPRESSION_H__

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "nudl/analysis/type_spec.h"
#include "nudl/analysis/types.h"

namespace nudl {
namespace analysis {

class Expression;
class ExpressionVisitor;
class Function;
struct FunctionBinding;
class FunctionGroup;
class Module;
class Scope;
class VarBase;

using CloneOverride =
    std::function<std::unique_ptr<Expression>(const Expression*)>;

class Expression {
 public:
  explicit Expression(Scope* scope);
  virtual ~Expression();

  absl::optional<const TypeSpec*> stored_type_spec() const;

  absl::StatusOr<const TypeSpec*> type_spec(
      absl::optional<const TypeSpec*> type_hint = {});

  // The kind of this expression
  virtual pb::ExpressionKind expr_kind() const = 0;
  // If this expression returns / refers an object, this is the place
  // to return it.
  virtual absl::optional<NamedObject*> named_object() const;
  // Switches the name object on rebinding.
  virtual void set_named_object(NamedObject* object);
  // If the expression performs a function performs a return/yield etc.
  virtual bool ContainsFunctionExit() const;

  // Calls the provided function in-order for this expression and
  // its children. If visiting `this` expression returns `false`,
  // visiting the children is canceled and this function returns
  // `false` as well.
  virtual bool VisitExpressions(ExpressionVisitor* visitor);

  // The scope in which the expression was built.
  Scope* scope() const;

  // Accesses the underlying expressions.
  const std::vector<std::unique_ptr<Expression>>& children() const;

  // Returns the statically evaluated value of this expression, in C++.
  virtual std::any StaticValue() const;

  // Builds an expression as a proto.
  virtual pb::ExpressionSpec ToProto() const;

  // Creates a copy of this expression.
  virtual std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const = 0;

  // Returns a string that describes the expression - for debug purposes.
  virtual std::string DebugString() const = 0;

  // This marks the expression as the last in the function and a
  // nominal "return"
  bool is_default_return() const;
  void set_is_default_return();

 protected:
  const std::any& value() const;
  virtual absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) = 0;
  // Can be used by Clone() to set the general type info of a cloned
  // expression.
  std::unique_ptr<Expression> CopyTypeInfo(
      std::unique_ptr<Expression> clone) const;
  std::vector<std::unique_ptr<Expression>> CloneChildren(
      const CloneOverride& clone_override) const;

  Scope* const scope_;
  std::vector<std::unique_ptr<Expression>> children_;
  absl::optional<const TypeSpec*> type_spec_;
  absl::optional<const TypeSpec*> type_hint_;
  absl::optional<NamedObject*> named_object_;
  bool is_default_return_ = false;
};

class ExpressionVisitor {
 public:
  ExpressionVisitor();
  virtual ~ExpressionVisitor();
  virtual bool Visit(Expression* expression) = 0;
  virtual void Reset();

 private:
  bool PerformVisit(Expression* expression);

  friend class Expression;
  absl::flat_hash_set<Expression*> visited_;
};

// A no-operation expression - usually created on pragmas.
class NopExpression : public Expression {
 public:
  NopExpression(Scope* scope,
                absl::optional<std::unique_ptr<Expression>> child = {});

  pb::ExpressionKind expr_kind() const override;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
};

// This represents an assignment, for which the types are set, and the
// var->Assign() was just called with value.
// <var> = <value> in scope <scope>.
class Assignment : public Expression {
 public:
  Assignment(Scope* scope, ScopedName name, VarBase* var,
             std::unique_ptr<Expression> value, bool has_type_spec,
             bool is_initial_assignment);

  pb::ExpressionKind expr_kind() const override;
  absl::optional<NamedObject*> named_object() const override;

  const ScopedName& name() const;
  VarBase* var() const;
  bool has_type_spec() const;
  bool is_initial_assignment() const;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  ScopedName name_;
  VarBase* const var_;
  const bool has_type_spec_;
  const bool is_initial_assignment_;
};

// This represents a special construct: [], which can represent multiple
// entities. We use the hint on type negotiation to set it.
class EmptyStruct : public Expression {
 public:
  explicit EmptyStruct(Scope* scope);

  pb::ExpressionKind expr_kind() const override;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
};

// An expression representing a literal value.
class Literal : public Expression {
 public:
  // Main builder method for the literal - use this normally.
  static absl::StatusOr<std::unique_ptr<Literal>> Build(
      Scope* scope, const pb::Literal& value);

  pb::ExpressionKind expr_kind() const override;
  std::any StaticValue() const override;

  // The type used for building.
  const TypeSpec* build_type_spec() const;
  // The value used for building the expression.
  const std::any& value() const;
  // The original string representation of the expression.
  const std::string& str_value() const;

  // Checks if the type held by value matches the expected type for type_spec.
  static absl::Status CheckType(const TypeSpec* type_spec,
                                const std::any& value);

  pb::ExpressionSpec ToProto() const override;
  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  // Used to coerce the type into something more restricted if necessary.
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;

  // Constructor, once the things are settled.
  Literal(Scope* scope, const TypeSpec* type_spec, std::any value,
          std::string str_value);

  const TypeSpec* build_type_spec_;
  std::any value_;
  std::string str_value_;
};

// Acesses a named object through an identifier.
class Identifier : public Expression {
 public:
  Identifier(Scope* scope, ScopedName scoped_name, NamedObject* object);

  pb::ExpressionKind expr_kind() const override;
  absl::optional<NamedObject*> named_object() const override;

  const ScopedName& scoped_name() const;
  NamedObject* object() const;

  pb::ExpressionSpec ToProto() const override;
  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;

  const ScopedName scoped_name_;
  NamedObject* const object_;
};

// A function that returns from a function: pass, yield, return.
class FunctionResultExpression : public Expression {
 public:
  FunctionResultExpression(
      Scope* scope, Function* parent_function,
      pb::FunctionResultKind result_kind,
      absl::optional<std::unique_ptr<Expression>> expression);

  absl::optional<NamedObject*> named_object() const override;
  pb::ExpressionKind expr_kind() const override;

  pb::FunctionResultKind result_kind() const;
  Function* parent_function() const;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

  bool ContainsFunctionExit() const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;

  const pb::FunctionResultKind result_kind_;
  Function* const parent_function_;
};

// An array definition with form: [elem1, elem2, ... ]
class ArrayDefinitionExpression : public Expression {
 public:
  ArrayDefinitionExpression(Scope* scope,
                            std::vector<std::unique_ptr<Expression>> elements);

  pb::ExpressionKind expr_kind() const override;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  absl::StatusOr<const TypeSpec*> NegotiateTuple(const TypeSpec* tuple_type);

  std::vector<std::unique_ptr<TypeSpec>> negotiated_types_;
};

// An map definition with form: [key1: val1, key2: val2, ... ]
class MapDefinitionExpression : public Expression {
 public:
  // When building this expression we expect keys & values interleaved
  // in elements array [key1, value1, key2, value2 ...]
  MapDefinitionExpression(Scope* scope,
                          std::vector<std::unique_ptr<Expression>> elements);

  pb::ExpressionKind expr_kind() const override;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  std::vector<std::unique_ptr<TypeSpec>> negotiated_types_;
};

// An (named) tuple definition with form:
//  {name1: type1? = val1, name2: type2? = val2, ...]
class TupleDefinitionExpression : public Expression {
 public:
  // When building this expression we expect keys & values interleaved
  // in elements array [key1, value1, key2, value2 ...]
  TupleDefinitionExpression(Scope* scope, std::vector<std::string> names,
                            std::vector<absl::optional<const TypeSpec*>> types,
                            std::vector<std::unique_ptr<Expression>> elements);

  pb::ExpressionKind expr_kind() const override;

  std::string DebugString() const override;
  pb::ExpressionSpec ToProto() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

  const std::vector<std::string>& names() const;
  const std::vector<absl::optional<const TypeSpec*>>& types() const;
  void CheckSizes() const;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  std::vector<std::string> names_;
  std::vector<absl::optional<const TypeSpec*>> types_;
  std::vector<std::unique_ptr<TypeSpec>> negotiated_types_;
};

// A composed if expression
// if (condition[0]) { expression[0] }
// elif (condition[1]} { expression[1] }
// ...
// elif (condition[n]} { expression[n] }
// else { expression[n + 1] }
class IfExpression : public Expression {
 public:
  // We expect expression.size() == condition.size() or
  // expression.size() == condition.size() + 1
  IfExpression(Scope* scope, std::vector<std::unique_ptr<Expression>> condition,
               std::vector<std::unique_ptr<Expression>> expression);

  pb::ExpressionKind expr_kind() const override;

  const std::vector<Expression*>& condition() const;
  const std::vector<Expression*>& expression() const;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

  // This returns true if we return on all paths.
  bool ContainsFunctionExit() const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  std::vector<Expression*> condition_;
  std::vector<Expression*> expression_;
};

// A block of expressions that execute one after another.
class ExpressionBlock : public Expression {
 public:
  ExpressionBlock(Scope* scope,
                  std::vector<std::unique_ptr<Expression>> children);

  pb::ExpressionKind expr_kind() const override;
  absl::optional<NamedObject*> named_object() const override;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;
  bool ContainsFunctionExit() const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
};

// An expression that accesses a value in a collection by an index.
class IndexExpression : public Expression {
 public:
  IndexExpression(Scope* scope, std::unique_ptr<Expression> object_expression,
                  std::unique_ptr<Expression> index_expression);

  pb::ExpressionKind expr_kind() const override;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

  virtual absl::StatusOr<const TypeSpec*> GetIndexedType(
      const TypeSpec* object_type) const;  // public for testing

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
};

// An expression that returns a static index from a tuple.
class TupleIndexExpression : public IndexExpression {
 public:
  TupleIndexExpression(Scope* scope,
                       std::unique_ptr<Expression> object_expression,
                       std::unique_ptr<Expression> index_expression,
                       size_t index);

  pb::ExpressionKind expr_kind() const override;

  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

  absl::StatusOr<const TypeSpec*> GetIndexedType(
      const TypeSpec* object_type) const override;

 protected:
  const size_t index_;
};

// An expression containing the definition of a lambda function.
class LambdaExpression : public Expression {
 public:
  LambdaExpression(Scope* scope, Function* lambda_function,
                   FunctionGroup* lambda_group);

  pb::ExpressionKind expr_kind() const override;
  absl::optional<NamedObject*> named_object() const override;

  Function* lambda_function() const;
  FunctionGroup* lambda_group() const;

  pb::ExpressionSpec ToProto() const override;
  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;

  Function* lambda_function_;
  FunctionGroup* const lambda_group_;
  std::vector<std::unique_ptr<FunctionBinding>> lambda_bindings_;
};

class DotAccessExpression : public Expression {
 public:
  DotAccessExpression(Scope* scope, std::unique_ptr<Expression> left_expression,
                      ScopeName name, NamedObject* object);
  DotAccessExpression(Scope* scope, std::unique_ptr<Expression> left_expression,
                      absl::string_view name, NamedObject* object);

  pb::ExpressionKind expr_kind() const override;
  absl::optional<NamedObject*> named_object() const override;

  const ScopeName& name() const;
  NamedObject* object() const;

  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;
  std::string DebugString() const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  const ScopeName name_;
  NamedObject* const object_;
};

class FunctionCallExpression : public Expression {
 public:
  FunctionCallExpression(
      Scope* scope, std::unique_ptr<FunctionBinding> function_binding,
      std::unique_ptr<Expression> left_expression,
      std::vector<std::unique_ptr<Expression>> argument_expressions,
      bool is_method_call);

  pb::ExpressionKind expr_kind() const override;

  FunctionBinding* function_binding() const;
  absl::optional<Expression*> left_expression() const;
  bool is_method_call() const;

  const absl::flat_hash_set<Function*>& dependent_functions() const;
  void set_dependent_functions(absl::flat_hash_set<Function*> fun);

  pb::ExpressionSpec ToProto() const override;
  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;
  bool VisitExpressions(ExpressionVisitor* visitor) override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;

  std::unique_ptr<FunctionBinding> function_binding_;
  std::unique_ptr<Expression> left_expression_;
  absl::flat_hash_set<Function*> dependent_functions_;
  bool is_method_call_;
};

// These next, are not really expressions, they are more like
// declarations but they generate code.

class ImportStatementExpression : public Expression {
 public:
  ImportStatementExpression(Scope* scope, absl::string_view local_name,
                            bool is_alias, Module* module);

  pb::ExpressionKind expr_kind() const override;
  absl::optional<NamedObject*> named_object() const override;

  const std::string& local_name() const;
  bool is_alias() const;
  Module* module() const;

  pb::ExpressionSpec ToProto() const override;
  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  const std::string local_name_;
  const bool is_alias_;
  Module* const module_;
};

class FunctionDefinitionExpression : public Expression {
 public:
  FunctionDefinitionExpression(Scope* scope, Function* def_function);

  pb::ExpressionKind expr_kind() const override;
  absl::optional<NamedObject*> named_object() const override;

  Function* def_function() const;

  pb::ExpressionSpec ToProto() const override;
  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  Function* const def_function_;
};

class SchemaDefinitionExpression : public Expression {
 public:
  SchemaDefinitionExpression(Scope* scope, TypeStruct* def_schema);

  pb::ExpressionKind expr_kind() const override;
  absl::optional<NamedObject*> named_object() const override;

  const TypeStruct* def_schema() const;

  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  TypeStruct* const def_schema_;
};

class TypeDefinitionExpression : public Expression {
 public:
  TypeDefinitionExpression(Scope* scope, absl::string_view type_name,
                           const TypeSpec* defined_type_spec);

  pb::ExpressionKind expr_kind() const override;

  const std::string& type_name() const;
  const TypeSpec* defined_type_spec() const;

  pb::ExpressionSpec ToProto() const override;
  std::string DebugString() const override;
  std::unique_ptr<Expression> Clone(
      const CloneOverride& clone_override) const override;

 protected:
  absl::StatusOr<const TypeSpec*> NegotiateType(
      absl::optional<const TypeSpec*> type_hint) override;
  std::string type_name_;
  const TypeSpec* const defined_type_spec_;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_EXPRESSION_H__
