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
#ifndef NUDL_ANALYSIS_PRAGMA_H__
#define NUDL_ANALYSIS_PRAGMA_H__

#include <memory>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "nudl/proto/dsl.pb.h"

namespace nudl {
namespace analysis {

class Expression;
class Module;
class Scope;

//
// Possible pragmas:
//
// Logs the debug string of the expression that follows.
inline constexpr absl::string_view kPragmaLogExpression = "log_expression";
// Logs the proto representation of the expression that follows.
inline constexpr absl::string_view kPragmaLogProto = "log_proto";
// Logs the type specification of the expression that follows.
inline constexpr absl::string_view kPragmaLogType = "log_type";

// Turns on/off the logging for function binding internals.
inline constexpr absl::string_view kPragmaLogBindingsOn = "log_bindings_on";
inline constexpr absl::string_view kPragmaLogBindingsOff = "log_bindings_off";

// Logs all the known names in this module / scope:
inline constexpr absl::string_view kPragmaLogModuleNames = "log_module_names";
inline constexpr absl::string_view kPragmaLogScopeNames = "log_scope_names";

class PragmaHandler {
 public:
  explicit PragmaHandler(Module* module);

  absl::StatusOr<std::unique_ptr<Expression>> HandlePragma(
      Scope* scope, const pb::PragmaExpression& expression);

  Module* module() const;

  // Flags that can be enabled via pragmas:
  bool log_bindings() const;

 private:
  Module* const module_;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_PRAGMA_H__
