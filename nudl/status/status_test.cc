#include "nudl/status/status.h"

#include "nudl/status/testing.h"

absl::Status F(bool make_error, absl::string_view message = "") {
  if (make_error) {
    return absl::InvalidArgumentError(message);
  }
  return absl::OkStatus();
}

absl::StatusOr<int> FAssign(bool make_error, absl::string_view message = "") {
  if (make_error) {
    return absl::InvalidArgumentError(message);
  }
  return 1;
}

absl::Status FWrap(bool make_error, absl::string_view message = "") {
  RETURN_IF_ERROR(F(make_error, "FWrap")) << message;
  return absl::OkStatus();
}

absl::StatusOr<int> FAssignWrap(bool make_error,
                                absl::string_view message = "") {
  ASSIGN_OR_RETURN(int value, FAssign(make_error, "FAssignWrap"), _ << message);
  return value;
}

TEST(Status, Macros) {
  EXPECT_OK(FWrap(false));
  ASSERT_OK_AND_ASSIGN(int value, FAssignWrap(false));
  EXPECT_EQ(value, 1);
  EXPECT_RAISES_WITH_MESSAGE(FWrap(true, "A"), InvalidArgument,
                             "INVALID_ARGUMENT: FWrap; A");
  EXPECT_RAISES_WITH_MESSAGE(FAssignWrap(true, "A").status(), InvalidArgument,
                             "INVALID_ARGUMENT: FAssignWrap; A");
}

namespace nudl {
namespace status {

TEST(Status, Annotate) {
  EXPECT_RAISES_WITH_MESSAGE(Annotate(absl::NotFoundError("A"), "B"), NotFound,
                             "NOT_FOUND: A; B");
  {
    absl::Status status = absl::NotFoundError("A");
    status.SetPayload("Y", absl::Cord("X_Y"));
    absl::Status annotated = Annotate(status, "B");
    EXPECT_EQ(annotated.ToString(), "NOT_FOUND: A; B [Y='X_Y']");
  }
  {
    absl::Status ok_status = absl::OkStatus();
    EXPECT_OK(UpdateOrAnnotate(ok_status, absl::OkStatus()));
    EXPECT_RAISES_WITH_MESSAGE(
        UpdateOrAnnotate(ok_status, absl::InternalError("B")), Internal,
        "INTERNAL: B");
  }
  {
    absl::Status status = absl::NotFoundError("A");
    EXPECT_RAISES_WITH_MESSAGE(
        UpdateOrAnnotate(status, absl::InternalError("B")), NotFound,
        "NOT_FOUND: A; B");
  }
  {
    absl::Status status = absl::NotFoundError("A");
    status.SetPayload("Y", absl::Cord("X_Y"));
    absl::Status annotation = absl::InternalError("B");
    absl::Status annotated = UpdateOrAnnotate(status, annotation);
    EXPECT_EQ(annotated.ToString(), "NOT_FOUND: A; B [Y='X_Y']");
    EXPECT_EQ(status.ToString(), "NOT_FOUND: A; B [Y='X_Y']");
  }
  {
    absl::Status status = absl::NotFoundError("A");
    absl::Status annotation = absl::InternalError("B");
    annotation.SetPayload("Z", absl::Cord("X_Z"));
    absl::Status annotated = UpdateOrAnnotate(status, annotation);
    EXPECT_EQ(annotated.ToString(), "NOT_FOUND: A; B [Z='X_Z']");
    EXPECT_EQ(status.ToString(), "NOT_FOUND: A; B [Z='X_Z']");
  }
  {
    std::vector<absl::Status> statuses;
    statuses.emplace_back(absl::NotFoundError("A"));
    absl::Status annotation = absl::InternalError("B");
    annotation.SetPayload("Z", absl::Cord("X_Z"));
    statuses.emplace_back(std::move(annotation));
    absl::Status annotated = JoinStatus(statuses);
    EXPECT_EQ(annotated.ToString(), "NOT_FOUND: A; B [Z='X_Z']");
  }
  {
    std::vector<absl::Status> statuses;
    EXPECT_OK(JoinStatus(statuses));
  }
}

}  // namespace status
}  // namespace nudl
