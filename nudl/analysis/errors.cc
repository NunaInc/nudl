#include "nudl/analysis/errors.h"

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "nudl/grammar/dsl.h"

namespace nudl {
namespace analysis {

pb::ErrorInfo CodeContext::ToErrorInfo(absl::string_view message) const {
  pb::ErrorInfo info;
  if (interval) {
    *info.mutable_location() = interval->begin();
  }
  if (code) {
    info.set_snippet(*code);
  }
  info.set_error_message(std::string(message));
  return info;
}

absl::Status& CodeContext::AppendErrorToStatus(
    absl::Status& status, absl::string_view message) const {
  if (!status.ok()) {
    status.SetPayload(
        absl::StrCat(grammar::kParseErrorUrl, "/", 0),
        absl::Cord(ToErrorInfo(absl::StrCat(message, ": ", status.message()))
                       .SerializeAsString()));
  }
  return status;
}

std::vector<std::string> ExtractErrorLines(const absl::Status& status) {
  std::string filename;
  std::string code;
  status.ForEachPayload(
      [&filename, &code](absl::string_view name, const absl::Cord& payload) {
        if (absl::StartsWith(name, grammar::kParseFileUrl)) {
          filename = std::string(payload);
        } else if (absl::StartsWith(name, grammar::kParseCodeUrl)) {
          code = std::string(payload);
        }
      });
  std::vector<std::string> code_lines = absl::StrSplit(code, '\n');

  std::vector<grammar::ErrorInfo> errors;
  status.ForEachPayload(
      [&errors](absl::string_view name, const absl::Cord& payload) {
        if (absl::StartsWith(name, grammar::kParseErrorUrl)) {
          pb::ErrorInfo err;
          if (err.ParseFromString(std::string(payload))) {
            errors.emplace_back(grammar::ErrorInfo::FromProto(err));
          }
        }
      });
  std::stable_sort(
      errors.begin(), errors.end(),
      [](const grammar::ErrorInfo& a, const grammar::ErrorInfo& b) {
        return a.location.line() < b.location.line();
      });
  std::vector<std::string> result;
  result.reserve(errors.size() * 3);
  for (const auto& error : errors) {
    result.emplace_back(error.ToCompileErrorString(filename));
    if (error.location.line() > 0 &&
        code_lines.size() > error.location.line() - 1) {
      result.emplace_back(code_lines[error.location.line() - 1]);
      result.emplace_back(
          absl::StrCat(std::string(error.location.column(), ' ') + "^"));
    }
  }
  return result;
}

}  // namespace analysis
}  // namespace nudl
