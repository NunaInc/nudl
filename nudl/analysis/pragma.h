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
