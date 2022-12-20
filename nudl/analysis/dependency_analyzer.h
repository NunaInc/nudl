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
#ifndef NUDL_ANALYSIS_DEPENDENCY_ANALYZER_H__
#define NUDL_ANALYSIS_DEPENDENCY_ANALYZER_H__

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "nudl/analysis/expression.h"
#include "nudl/analysis/type_spec.h"
#include "nudl/analysis/vars.h"

namespace nudl {
namespace analysis {

using FieldUsageSet = absl::flat_hash_set<std::string>;
using FieldUsageMap =
    absl::flat_hash_map<const TypeSpec*, std::unique_ptr<FieldUsageSet>>;

class FieldUsageVisitor : public ExpressionVisitor {
 public:
  FieldUsageVisitor();

  bool Visit(Expression* expression) override;
  const FieldUsageMap& usage_map() const;

 private:
  bool ProcessIdentifier(Identifier* expression);
  bool ProcessDotAccess(DotAccessExpression* expression);
  bool ProcessFunctionCall(FunctionCallExpression* expression);
  void RecordField(const Field* field);

  FieldUsageMap usage_map_;
};

// If the provided expression returns a function, or a function type,
// it visits the underlying function expressions with the provided visitor.
void VisitFunctionExpressions(Expression* expression,
                              ExpressionVisitor* visitor);

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_DEPENDENCY_ANALYZER_H__
