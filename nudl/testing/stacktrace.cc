#include "nudl/testing/stacktrace.h"

#include <string.h>

#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace nudl {
namespace stacktrace {

std::vector<std::string> GetTrace(size_t max_depth) {
  if (max_depth > kMaxStackTraceDepth) {
    max_depth = kMaxStackTraceDepth;
  }
  std::vector<void*> results(max_depth, nullptr);
  std::vector<int> sizes(max_depth, 0);
  const int depth = absl::GetStackFrames(&results[0], &sizes[0], max_depth, 1);
  std::vector<std::string> symbols;
  if (depth < 0) {
    return symbols;
  }
  size_t result_size = static_cast<size_t>(depth);
  if (result_size > results.size()) {
    result_size = results.size();
  }
  symbols.reserve(result_size);
  char buffer[kMaxSymbolSize];
  for (size_t i = 1; i < result_size; ++i) {
    if (absl::Symbolize(results[i], buffer, sizeof(buffer))) {
      symbols.emplace_back(absl::StrFormat(
          "@%p - %s", results[i],
          absl::string_view(buffer, strnlen(buffer, sizeof(buffer)))));
    } else {
      symbols.emplace_back(
          absl::StrFormat("@%p - [ no symbols found ]", results[i]));
    }
  }
  return symbols;
}

// Gets the stack, than prints it to stderr.
std::string ToString(size_t max_depth) {
  const std::vector<std::string> trace = GetTrace(max_depth);
  return absl::StrJoin(GetTrace(max_depth), "\n",
                       [](std::string* out, const std::string& s) {
                         absl::StrAppend(out, "    ", s);
                       });
}

}  // namespace stacktrace
}  // namespace nudl
