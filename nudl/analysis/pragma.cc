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

#include "nudl/analysis/pragma.h"

#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "glog/logging.h"
#include "nudl/analysis/expression.h"
#include "nudl/analysis/module.h"
#include "nudl/analysis/scope.h"
#include "nudl/status/status.h"

ABSL_FLAG(bool, analyze_log_bindings, false,
          "Turns on detailed logging of the function bindings");

namespace nudl {
namespace analysis {

PragmaHandler::PragmaHandler(Module* module) : module_(module) {}

Module* PragmaHandler::module() const { return module_; }

bool PragmaHandler::log_bindings() const {
  // This flag needs to be global.
  return absl::GetFlag(FLAGS_analyze_log_bindings);
}

absl::StatusOr<std::unique_ptr<Expression>> PragmaHandler::HandlePragma(
    Scope* scope, const pb::PragmaExpression& expression) {
  static const auto kPragmasWithExpression =
      new absl::flat_hash_set<std::string>({
          std::string(kPragmaLogExpression),
          std::string(kPragmaLogProto),
          std::string(kPragmaLogType),
      });
  absl::optional<std::unique_ptr<Expression>> child;
  const TypeSpec* type_spec = nullptr;
  if (expression.has_value()) {
    ASSIGN_OR_RETURN(
        child, scope->BuildExpression(expression.value()),
        _ << "Building pragma " << expression.name() << " child expression");
    ASSIGN_OR_RETURN(type_spec, child.value()->type_spec(),
                     _ << "Determining pragma " << expression.name()
                       << " child expression type");
  }
  if (kPragmasWithExpression->contains(expression.name())) {
    if (!child.has_value()) {
      return status::InvalidArgumentErrorBuilder()
             << "Pragma " << expression.name()
             << " requires an expression to be specified as argument.";
    }
  } else if (child.has_value()) {
    return status::InvalidArgumentErrorBuilder()
           << "Pragma " << expression.name()
           << " is either unknown, or does not require an expression";
  }
  if (expression.name() == kPragmaLogBindingsOn) {
    absl::SetFlag(&FLAGS_analyze_log_bindings, true);
  } else if (expression.name() == kPragmaLogBindingsOff) {
    absl::SetFlag(&FLAGS_analyze_log_bindings, false);
  } else if (expression.name() == kPragmaLogModuleNames) {
    LOG(INFO) << "Names for module: " << module_->full_name() << "\n"
              << module_->ToProtoObject().DebugString();
  } else if (expression.name() == kPragmaLogScopeNames) {
    LOG(INFO) << "Names for module: " << scope->full_name() << "\n"
              << scope->ToProtoObject().DebugString();
  } else if (expression.name() == kPragmaLogExpression) {
    LOG(INFO) << "Pragma expression for: `" << expression.value().code()
              << "`:\n"
              << child.value()->DebugString();
  } else if (expression.name() == kPragmaLogProto) {
    LOG(INFO) << "Pragma expression proto for: `" << expression.value().code()
              << "`:\n"
              << child.value()->ToProto().DebugString();
  } else if (expression.name() == kPragmaLogType) {
    LOG(INFO) << "Pragma type for: `" << expression.value().code() << "`:\n"
              << type_spec->full_name();
  } else {
    return status::InvalidArgumentErrorBuilder()
           << "Unknown pragma " << expression.name();
  }
  return {std::make_unique<NopExpression>(scope, std::move(child))};
}

}  // namespace analysis
}  // namespace nudl
