#ifndef NUDL_ANALYSIS_NAMES_H__
#define NUDL_ANALYSIS_NAMES_H__

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "nudl/proto/analysis.pb.h"
#include "nudl/proto/dsl.pb.h"

namespace nudl {
namespace analysis {

// This class names a scope, which is formed by joining
// a module name with a function name. The format is:
// <scope_name> = <module_name> <function_name>?
// <module_name> = <name> [ '.' <name> ] *
// <function_name> = [ '::' <name> ] +
// We reserve empty scope name for built-in scope, in which
// we place all built-in types and functions.
class ScopeName {
 public:
  // Builder function - use this to construct a ScopeName.
  //  - parsing it from a name with . and ::
  static absl::StatusOr<ScopeName> Parse(absl::string_view name);
  //  - reading it from a proto representation.
  static absl::StatusOr<ScopeName> FromProto(const pb::ScopeName& proto);

  // Prefer using the Parse.
  ScopeName();
  ScopeName(std::string name, std::vector<std::string> module_names,
            std::vector<std::string> function_names);

  const std::vector<std::string>& module_names() const;
  const std::vector<std::string>& function_names() const;

  // Full name of a scope.
  const std::string& name() const;
  // Full module name, concatenated with a '.'.
  std::string module_name() const;
  // Full function name, concatenated with a '::'.
  std::string function_name() const;

  // If this refers to the base scope.
  bool empty() const;

  // Total number of names (module + function) in this ScopeName.
  size_t size() const;

  // Hasing value for this object.
  std::size_t hash() const;

  // Recomposes a partial prefix name up to the provided positon.
  // Starts with module names, then advance to function names.
  std::string PrefixName(size_t position) const;

  // Recomposes a partial prefixname as above, but to a ScopeName.
  ScopeName PrefixScopeName(size_t position) const;

  // Recomposes a partial suffix name from the provided position,
  // included.
  std::string SuffixName(size_t position) const;

  // Recomposes a partial suffix name, but as a ScopeName.
  ScopeName SuffixScopeName(size_t position) const;

  // If this scope name is a prefix for the argument.
  bool IsPrefixScope(const ScopeName& scope_name) const;

  // For a module only scope name, builds a name that appends
  // on the module.  E.g. foo.bar + baz => foo.bar.baz
  absl::StatusOr<ScopeName> Submodule(absl::string_view name) const;
  // Builds a module that appends on a function.
  // E.g. foo.bar::baz + qux => foo.bar::baz::qux
  absl::StatusOr<ScopeName> Subfunction(absl::string_view name) const;
  // Appends the name to either module or function
  absl::StatusOr<ScopeName> Subname(absl::string_view name) const;

  // Returns a subscope of this scope name
  ScopeName Subscope(const ScopeName& scope_name) const;

  // Encodes this as a proto message.
  pb::ScopeName ToProto() const;

  // Recomposes a name from components:
  static std::string Recompose(
      const absl::Span<const std::string>& module_names,
      const absl::Span<const std::string>& function_names);

 private:
  std::string name_;
  std::vector<std::string> module_names_;
  std::vector<std::string> function_names_;
  std::size_t hash_ = 0;
};

// A name inside a scope.
class ScopedName {
 public:
  ScopedName(std::shared_ptr<ScopeName> scope_name, absl::string_view name);

  static absl::StatusOr<ScopedName> Parse(absl::string_view name);
  static absl::StatusOr<ScopedName> FromIdentifier(
      const pb::Identifier& identifier);
  static absl::StatusOr<ScopedName> FromProto(const pb::ScopedName& proto);

  const std::shared_ptr<ScopeName>& scope_name_ptr() const;
  const ScopeName& scope_name() const;
  const std::string& name() const;
  std::string full_name() const;
  pb::ScopedName ToProto() const;

 private:
  std::shared_ptr<ScopeName> scope_name_;
  std::string name_;
};

// Some utility functions for names:
class NameUtil {
 public:
  // If provided name is valid. I.e. letters + digits + _
  // and does not start w/ a digit.
  static bool IsValidName(absl::string_view name);
  // Invokes IsValidName to return a string or an error.
  static absl::StatusOr<std::string> ValidatedName(std::string name);
  // If provided name is a valid module name. I.e. <name> [ '.' <name> ]*
  static bool IsValidModuleName(absl::string_view name);
  // Invokes IsValidModuleName to return a string or an error.
  static absl::StatusOr<std::string> ValidatedModuleName(std::string name);
  // Build a module name from a identifier proto.
  static absl::StatusOr<std::string> GetModuleName(
      const pb::Identifier& identifier);
  // Same as above, but interprets the whole identifier as a module.
  static absl::StatusOr<std::string> GetFullModuleName(
      const pb::Identifier& identifier);
  // Returns the final name from an identifier proto.
  static absl::StatusOr<std::string> GetObjectName(
      const pb::Identifier& identifier);
  // Builds a simple identifier from a name:
  static pb::Identifier IdentifierFromName(absl::string_view name);
};

}  // namespace analysis
}  // namespace nudl

namespace std {

// Hashing for ScopeNames
template <>
struct hash<nudl::analysis::ScopeName> {
  std::size_t operator()(const nudl::analysis::ScopeName& name) {
    return name.hash();
  }
};

}  // namespace std

#endif  // NUDL_ANALYSIS_NAMES_H__
