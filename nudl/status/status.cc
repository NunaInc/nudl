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

absl::Status JoinStatus(const std::vector<absl::Status>& statuses) {
  absl::Status status;
  for (auto crt : statuses) {
    UpdateOrAnnotate(status, crt);
  }
  return status;
}

size_t GetNumPayloads(const absl::Status& status) {
  size_t num_payloads = 0;
  status.ForEachPayload(
      [&num_payloads](absl::string_view name, const absl::Cord& payload) {
        ++num_payloads;
      });
  return num_payloads;
}

}  // namespace status
}  // namespace nudl
