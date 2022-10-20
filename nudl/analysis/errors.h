#ifndef NUDL_ANALYSIS_ERRORS_H__
#define NUDL_ANALYSIS_ERRORS_H__

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "nudl/proto/dsl.pb.h"

namespace nudl {
namespace analysis {

struct CodeContext {
  const pb::CodeInterval* interval = nullptr;
  const std::string* code = nullptr;

  template <class Proto>
  static CodeContext FromProto(const Proto& proto) {
    CodeContext context;
    if (proto.has_code_interval()) {
      context.interval = &proto.code_interval();
    }
    if (proto.has_code()) {
      context.code = &proto.code();
    }
    return context;
  }
  pb::ErrorInfo ToErrorInfo(absl::string_view message) const;
  absl::Status& AppendErrorToStatus(absl::Status& status,
                                    absl::string_view message) const;
};

std::vector<std::string> ExtractErrorLines(const absl::Status& status);

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_ERRORS_H__
