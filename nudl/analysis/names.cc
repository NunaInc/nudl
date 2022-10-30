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

#include "nudl/analysis/names.h"

#include <algorithm>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "nudl/status/status.h"
#include "re2/re2.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);

namespace nudl {
namespace analysis {

ScopeName::ScopeName() : hash_(std::hash<std::string>()(std::string())) {}

ScopeName::ScopeName(std::string name, std::vector<std::string> module_names,
                     std::vector<std::string> function_names)
    : name_(std::move(name)),
      module_names_(std::move(module_names)),
      function_names_(std::move(function_names)),
      hash_(std::hash<std::string>()(name_)) {}

const std::vector<std::string>& ScopeName::module_names() const {
  return module_names_;
}
const std::vector<std::string>& ScopeName::function_names() const {
  return function_names_;
}
const std::string& ScopeName::name() const { return name_; }

std::string ScopeName::module_name() const {
  return absl::StrJoin(module_names_, ".");
}

std::string ScopeName::function_name() const {
  return absl::StrJoin(function_names_, "::");
}

bool ScopeName::empty() const {
  return module_names_.empty() && function_names_.empty();
}

size_t ScopeName::size() const {
  return module_names_.size() + function_names_.size();
}

std::size_t ScopeName::hash() const { return hash_; }

absl::StatusOr<ScopeName> ScopeName::Parse(absl::string_view name) {
  if (name.empty()) {
    return ScopeName();
  }
  std::vector<std::string> pieces =
      absl::StrSplit(name, absl::MaxSplits("::", 1));
  RET_CHECK(!pieces.empty());
  std::vector<std::string> module_names, function_names;
  if (!pieces[0].empty()) {
    for (absl::string_view crt_name : absl::StrSplit(pieces[0], '.')) {
      ASSIGN_OR_RETURN(std::string module_name,
                       NameUtil::ValidatedName(std::string(crt_name)),
                       _ << "Invalid module name: `" << crt_name
                         << "`, "
                            "in scope name: `"
                         << name << "`");
      module_names.emplace_back(std::move(module_name));
    }
  }
  if (pieces.size() > 1) {
    for (absl::string_view crt_name : absl::StrSplit(pieces[1], "::")) {
      ASSIGN_OR_RETURN(std::string function_name,
                       NameUtil::ValidatedName(std::string(crt_name)),
                       _ << "Invalid function name: `" << crt_name
                         << "`, "
                            "in scope name: `"
                         << name << "`");
      function_names.emplace_back(std::move(function_name));
    }
  }
  return ScopeName(std::string(name), std::move(module_names),
                   std::move(function_names));
}

absl::StatusOr<ScopeName> ScopeName::FromProto(const pb::ScopeName& proto) {
  std::vector<std::string> module_names, function_names;
  module_names.reserve(proto.module_name().size());
  for (const auto& name : proto.module_name()) {
    ASSIGN_OR_RETURN(std::string mname, NameUtil::ValidatedName(name),
                     _ << "For module name in ScopeName proto");
    module_names.emplace_back(std::move(mname));
  }
  function_names.reserve(proto.function_name().size());
  for (const auto& name : proto.function_name()) {
    ASSIGN_OR_RETURN(std::string fname, NameUtil::ValidatedName(name),
                     _ << "For function name in ScopeName proto");
    function_names.emplace_back(std::move(fname));
  }
  std::string full_name(absl::StrJoin(module_names, "."));
  if (!function_names.empty()) {
    absl::StrAppend(&full_name, "::", absl::StrJoin(function_names, "::"));
  }
  return ScopeName(std::move(full_name), std::move(module_names),
                   std::move(function_names));
}

pb::ScopeName ScopeName::ToProto() const {
  pb::ScopeName proto;
  if (absl::GetFlag(FLAGS_nudl_short_analysis_proto)) {
    proto.set_name(name());
    return proto;
  }
  for (const auto& name : module_names_) {
    proto.add_module_name(name);
  }
  for (const auto& name : function_names_) {
    proto.add_function_name(name);
  }
  return proto;
}

pb::Identifier ScopeName::ToIdentifier() const {
  pb::Identifier identifier;
  identifier.mutable_name()->Reserve(size() + 1);
  for (const auto& name : module_names_) {
    identifier.add_name(name);
  }
  for (const auto& name : function_names_) {
    identifier.add_name(name);
  }
  return identifier;
}

absl::StatusOr<ScopeName> ScopeName::Submodule(absl::string_view name) const {
  if (!NameUtil::IsValidName(name)) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid submodule name: `" << name << "` to append to: `"
           << name_ << "`";
  }
  std::vector<std::string> module_names(module_names_);
  module_names.emplace_back(std::string(name));
  // Order or evaluation in parameters is compiler dependent.
  // Compute this here first, not as a function parameter.
  std::string full_name(Recompose(module_names, function_names_));
  return ScopeName(std::move(full_name), std::move(module_names),
                   function_names_);
}

absl::StatusOr<ScopeName> ScopeName::Subfunction(absl::string_view name) const {
  name = absl::StripPrefix(name, "::");
  if (!NameUtil::IsValidName(name)) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid subfunction name: `" << name << "` to append to: `"
           << name_ << "`";
  }
  std::vector<std::string> function_names(function_names_);
  function_names.emplace_back(std::string(name));
  std::string full_name(Recompose(module_names_, function_names));
  return ScopeName(std::move(full_name), module_names_,
                   std::move(function_names));
}

absl::StatusOr<ScopeName> ScopeName::Subname(absl::string_view name) const {
  if (!NameUtil::IsValidName(name)) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid name: `" << name << "` to append to: `" << name_ << "`";
  }
  if (function_names_.empty()) {
    std::vector<std::string> module_names(module_names_);
    module_names.emplace_back(std::string(name));
    std::string full_name(Recompose(module_names, {}));
    return ScopeName(std::move(full_name), std::move(module_names), {});
  } else {
    std::vector<std::string> function_names(function_names_);
    function_names.emplace_back(std::string(name));
    std::string full_name(Recompose(module_names_, function_names));
    return ScopeName(std::move(full_name), module_names_,
                     std::move(function_names));
  }
}

std::string ScopeName::Recompose(
    const absl::Span<const std::string>& module_names,
    const absl::Span<const std::string>& function_names) {
  std::string s(absl::StrJoin(module_names, "."));
  if (!function_names.empty()) {
    absl::StrAppend(&s, "::", absl::StrJoin(function_names, "::"));
  }
  return s;
}

std::string ScopeName::PrefixName(size_t position) const {
  if (position >= size()) {
    return name_;
  }
  if (position <= module_names_.size()) {
    return Recompose(absl::Span(module_names_.data(), position), {});
  }
  position -= module_names_.size();
  CHECK_LT(position, function_names_.size());
  return Recompose(module_names_, absl::Span(function_names_.data(), position));
}

ScopeName ScopeName::PrefixScopeName(size_t position) const {
  if (position >= size()) {
    return *this;
  }
  std::string name(PrefixName(position));
  if (position <= module_names_.size()) {
    return ScopeName(std::move(name),
                     {module_names_.begin(), module_names_.begin() + position},
                     {});
  }
  position -= module_names_.size();
  CHECK_LT(position, function_names_.size());
  return ScopeName(
      std::move(name), module_names_,
      {function_names_.begin(), function_names_.begin() + position});
}

std::string ScopeName::SuffixName(size_t position) const {
  if (position >= size()) {
    return std::string();
  }
  if (position < module_names_.size()) {
    return Recompose(absl::Span(module_names_.data() + position,
                                module_names_.size() - position),
                     function_names_);
  }
  position -= module_names_.size();
  CHECK_LT(position, function_names_.size());
  return Recompose({}, absl::Span(function_names_.data() + position,
                                  function_names_.size() - position));
}

ScopeName ScopeName::SuffixScopeName(size_t position) const {
  if (position >= size()) {
    return ScopeName();
  }
  std::string name(SuffixName(position));
  if (position < module_names_.size()) {
    return ScopeName(std::move(name),
                     {module_names_.begin() + position, module_names_.end()},
                     function_names_);
  }
  position -= module_names_.size();
  CHECK_LT(position, function_names_.size());
  return ScopeName(std::move(name), {},
                   {function_names_.begin() + position, function_names_.end()});
}

ScopeName ScopeName::Subscope(const ScopeName& scope_name) const {
  if (scope_name.empty()) {
    return *this;
  }
  if (!function_names_.empty() && (!scope_name.module_names().empty() ||
                                   scope_name.function_names().empty())) {
    return *this;
  }
  std::vector<std::string> module_names(module_names_);
  std::copy(scope_name.module_names().begin(), scope_name.module_names().end(),
            std::back_inserter(module_names));
  std::vector<std::string> function_names(function_names_);
  std::copy(scope_name.function_names().begin(),
            scope_name.function_names().end(),
            std::back_inserter(function_names));
  std::string subname(Recompose(module_names, function_names));
  return ScopeName(std::move(subname), std::move(module_names),
                   std::move(function_names));
}

bool ScopeName::IsPrefixScope(const ScopeName& scope_name) const {
  if (name().empty()) {
    return true;
  }
  if (!absl::StartsWith(scope_name.name(), name())) {
    return false;
  }
  const absl::string_view suffix(
      absl::string_view(scope_name.name()).substr(name().size()));
  return (suffix.empty() || absl::StartsWith(suffix, ".") ||
          absl::StartsWith(suffix, "::"));
}

ScopedName::ScopedName(std::shared_ptr<ScopeName> scope_name,
                       absl::string_view name)
    : scope_name_(std::move(CHECK_NOTNULL(scope_name))), name_(name) {}

absl::StatusOr<ScopedName> ScopedName::Parse(absl::string_view name) {
  auto pos = name.rfind('.');
  absl::string_view name_part(name);
  ScopeName scope_name;
  if (pos != std::string::npos) {
    name_part = name.substr(pos + 1);
    ASSIGN_OR_RETURN(scope_name, ScopeName::Parse(name.substr(0, pos)),
                     _ << "Bad scope part in scoped name: `" << name << "`");
  }
  if (!NameUtil::IsValidName(name_part)) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid name for scoped name: `" << name_part << "`";
  }
  return ScopedName(std::make_shared<ScopeName>(std::move(scope_name)),
                    std::string(name_part));
}

absl::StatusOr<ScopedName> ScopedName::FromProto(const pb::ScopedName& proto) {
  ASSIGN_OR_RETURN(auto scope_name, ScopeName::FromProto(proto.scope_name()),
                   _ << "Building ScopedName from proto");
  ASSIGN_OR_RETURN(auto name, NameUtil::ValidatedName(proto.name()),
                   _ << "For name field in ScopedName proto");
  return ScopedName(std::make_shared<ScopeName>(std::move(scope_name)), name);
}

pb::ScopedName ScopedName::ToProto() const {
  pb::ScopedName proto;
  if (absl::GetFlag(FLAGS_nudl_short_analysis_proto)) {
    proto.set_full_name(full_name());
    return proto;
  }
  if (!scope_name_->empty()) {
    *proto.mutable_scope_name() = scope_name_->ToProto();
  }
  proto.set_name(name_);
  return proto;
}

pb::Identifier ScopedName::ToIdentifier() const {
  pb::Identifier identifier = scope_name_->ToIdentifier();
  identifier.add_name(name_);
  return identifier;
}

pb::TypeSpec ScopedName::ToTypeSpec() const {
  pb::TypeSpec type_spec;
  *type_spec.mutable_identifier() = ToIdentifier();
  return type_spec;
}

absl::StatusOr<ScopedName> ScopedName::FromIdentifier(
    const pb::Identifier& identifier) {
  if (identifier.name().empty()) {
    return absl::InvalidArgumentError("Empty identifier for scoped name");
  }
  return Parse(absl::StrJoin(
      absl::Span(identifier.name().data(), identifier.name().size()), "."));
}

const std::shared_ptr<ScopeName>& ScopedName::scope_name_ptr() const {
  return scope_name_;
}

const ScopeName& ScopedName::scope_name() const { return *scope_name_; }

const std::string& ScopedName::name() const { return name_; }

std::string ScopedName::full_name() const {
  if (scope_name_->name().empty()) {
    return name_;
  }
  if (name_.empty()) {
    return scope_name_->name();
  }
  return absl::StrCat(scope_name_->name(), ".", name_);
}

bool NameUtil::IsValidName(absl::string_view name) {
  return RE2::FullMatch(name, R"([a-zA-Z_][a-zA-Z0-9_]*)");
}

absl::StatusOr<std::string> NameUtil::ValidatedName(std::string name) {
  if (!IsValidName(name)) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid identifier name: `" << name << "`";
  }
  return {std::move(name)};
}

bool NameUtil::IsValidModuleName(absl::string_view name) {
  if (name.empty()) {
    return true;
  }
  auto splitter = absl::StrSplit(name, ".");
  return std::all_of(splitter.begin(), splitter.end(), IsValidName);
}

absl::StatusOr<std::string> NameUtil::ValidatedModuleName(std::string name) {
  if (!IsValidModuleName(name)) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid module name: `" << name << "`";
  }
  return {std::move(name)};
}

absl::StatusOr<std::string> NameUtil::GetModuleName(
    const pb::Identifier& identifier) {
  if (identifier.name().empty()) {
    return absl::InvalidArgumentError("Empty identifier provider");
  }
  return ValidatedModuleName(absl::StrJoin(
      absl::Span(identifier.name().data(), identifier.name().size() - 1), "."));
}

absl::StatusOr<std::string> NameUtil::GetFullModuleName(
    const pb::Identifier& identifier) {
  if (identifier.name().empty()) {
    return absl::InvalidArgumentError("Empty identifier provider");
  }
  return ValidatedModuleName(absl::StrJoin(identifier.name(), "."));
}

absl::StatusOr<std::string> NameUtil::GetObjectName(
    const pb::Identifier& identifier) {
  if (identifier.name().empty()) {
    return absl::InvalidArgumentError("Empty identifier provider");
  }
  return ValidatedName(*identifier.name().rbegin());
}

pb::Identifier NameUtil::IdentifierFromName(absl::string_view name) {
  pb::Identifier identifier;
  identifier.add_name(std::string(name));
  return identifier;
}

}  // namespace analysis
}  // namespace nudl
