#include "nudl/status/status.h"

#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, status_annotate_joiner, "; ",
          "How composed status is joined.");

namespace nudl {
namespace status {

absl::Status Annotate(const absl::Status& status, absl::string_view message) {
  if (message.empty()) {
    return status;
  }
  absl::Status result(
      status.code(),
      status.message().empty()
          ? message
          : absl::StrCat(status.message(),
                         absl::GetFlag(FLAGS_status_annotate_joiner), message));
  status.ForEachPayload(
      [&result](absl::string_view name, const absl::Cord& payload) {
        result.SetPayload(name, payload);
      });
  return result;
}

absl::Status& UpdateOrAnnotate(absl::Status& status,
                               const absl::Status& annotation) {
  if (status.ok()) {
    status.Update(annotation);
  } else {
    status = Annotate(status, annotation.message());
    annotation.ForEachPayload(
        [&status](absl::string_view name, const absl::Cord& payload) {
          status.SetPayload(name, payload);
        });
  }
  return status;
}

}  // namespace status
}  // namespace nudl
