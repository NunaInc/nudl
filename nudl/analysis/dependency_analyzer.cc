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

#include "nudl/analysis/dependency_analyzer.h"

#include "nudl/analysis/function.h"

namespace nudl {
namespace analysis {

FieldUsageVisitor::FieldUsageVisitor() {}

const FieldUsageMap& FieldUsageVisitor::usage_map() const { return usage_map_; }

bool FieldUsageVisitor::Visit(Expression* expression) {
  switch (expression->expr_kind()) {
    case pb::ExpressionKind::EXPR_IDENTIFIER:
      return ProcessIdentifier(static_cast<Identifier*>(expression));
    case pb::ExpressionKind::EXPR_DOT_ACCESS:
      return ProcessDotAccess(static_cast<DotAccessExpression*>(expression));
    case pb::ExpressionKind::EXPR_FUNCTION_CALL:
      return ProcessFunctionCall(
          static_cast<FunctionCallExpression*>(expression));
    default:
      break;
  }
  VisitFunctionExpressions(expression, this);
  return true;
}

void FieldUsageVisitor::RecordField(const Field* field) {
  auto it = usage_map_.find(field->parent_type());
  if (it == usage_map_.end()) {
    it = usage_map_
             .emplace(field->parent_type(), std::make_unique<FieldUsageSet>())
             .first;
  }
  it->second->insert(field->name());
  absl::optional<NameStore*> parent = field->parent_store();
  if (parent.has_value() &&
      parent.value()->kind() == pb::ObjectKind::OBJ_FIELD) {
    RecordField(static_cast<const Field*>(parent.value()));
  }
}

bool FieldUsageVisitor::ProcessIdentifier(Identifier* expression) {
  if (expression->object()->kind() == pb::ObjectKind::OBJ_FIELD) {
    RecordField(static_cast<const Field*>(expression->object()));
  }
  return true;
}

bool FieldUsageVisitor::ProcessDotAccess(DotAccessExpression* expression) {
  if (expression->object()->kind() == pb::ObjectKind::OBJ_FIELD) {
    RecordField(static_cast<const Field*>(expression->object()));
  }
  VisitFunctionExpressions(expression, this);
  return true;
}

namespace {
void VisitFunction(const Function* fun, ExpressionVisitor* visitor) {
  for (const auto& expr : fun->expressions()) {
    expr->VisitExpressions(visitor);
  }
}
}  // namespace

bool FieldUsageVisitor::ProcessFunctionCall(
    FunctionCallExpression* expression) {
  for (const Function* fun : expression->dependent_functions()) {
    VisitFunction(fun, this);
  }
  return true;
}

void VisitFunctionExpressions(Expression* expression,
                              ExpressionVisitor* visitor) {
  if (expression->named_object().has_value()) {
    auto obj = expression->named_object().value();
    if (FunctionGroup::IsFunctionGroup(*obj)) {
      for (const Function* fun :
           static_cast<const FunctionGroup*>(obj)->functions()) {
        VisitFunction(fun, visitor);
      }
    } else if (Function::IsFunctionKind(*obj)) {
      VisitFunction(static_cast<const Function*>(obj), visitor);
    }
  } else {
    auto type_spec = expression->stored_type_spec();
    if (type_spec.has_value() &&
        TypeUtils::IsFunctionType(*type_spec.value())) {
      auto function_type = static_cast<const TypeFunction*>(type_spec.value());
      for (const Function* fun : function_type->function_instances()) {
        VisitFunction(fun, visitor);
      }
    }
  }
  expression->VisitExpressions(visitor);
}

}  // namespace analysis
}  // namespace nudl
