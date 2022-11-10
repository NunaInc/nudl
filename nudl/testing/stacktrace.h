#ifndef NUDL_TESTING_PRINT_STACK_TRACE_H_
#define NUDL_TESTING_PRINT_STACK_TRACE_H_

#include <string>
#include <vector>

// Note: Your program must call absl::InstallFailureSignalHandler()
// and  absl::InitializeSymbolizer() when starting.
namespace nudl {
namespace stacktrace {

constexpr inline size_t kDefaultStackTraceDepth = 40;
constexpr inline size_t kMaxStackTraceDepth = 2048;
constexpr inline size_t kMaxSymbolSize = 2048;

// These are for the convenience of debugging only, not for failure
// handling or so.

// Simple function that wraps absl::GetStackFrames and absl::Symbolize.
std::vector<std::string> GetTrace(size_t max_depth = kDefaultStackTraceDepth);

// Gets the stack, than prints it to stderr.
std::string ToString(size_t max_depth = kDefaultStackTraceDepth);

}  // namespace stacktrace
}  // namespace nudl

#endif  // NUDL_TESTING_PRINT_STACK_TRACE_H_
