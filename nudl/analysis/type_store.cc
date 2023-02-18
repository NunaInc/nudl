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

#include "nudl/analysis/type_store.h"

#include <utility>

#include "glog/logging.h"
#include "nudl/analysis/type_utils.h"
#include "nudl/grammar/dsl.h"
#include "nudl/status/status.h"

namespace nudl {
namespace analysis {

GlobalTypeStore::GlobalTypeStore(std::unique_ptr<TypeStore> base_store)
    : TypeStore() {
  if (base_store) {
    base_store_ = std::move(base_store);
  } else {
    base_store_ = std::make_unique<BaseTypesStore>(this);
  }
}

const TypeStore& GlobalTypeStore::base_store() const { return *base_store_; }

TypeStore* GlobalTypeStore::mutable_base_store() { return base_store_.get(); }

TypeStore* GlobalTypeStore::GlobalStore() { return this; }

absl::optional<ScopeTypeStore*> GlobalTypeStore::FindStore(
    absl::string_view name) const {
  const auto it = scopes_.find(name);
  if (it != scopes_.end()) {
    return it->second;
  }
  return {};
}

std::string GlobalTypeStore::DebugNames() const {
  std::string s(
      absl::StrCat("Global store with: ", scopes_.size(), " subscopes"));
  std::cout << "---> S: " << s << "\n";
  for (const auto& it : scopes_) {
    std::cout << "---> X: " << it.first << " -> " << it.second << "\n";
    absl::StrAppend(&s, "> Substore: ", it.first, "\n",
                    it.second->DebugNames());
  }
  std::cout << "---> Base: " << base_store_ << "\n";
  absl::StrAppend(&s, "Base store:\n", base_store_->DebugNames());
  std::cout << "---> Done\n";
  return s;
}

absl::StatusOr<const TypeSpec*> GlobalTypeStore::FindType(
    const ScopeName& lookup_scope, const pb::TypeSpec& type_spec) {
  if (type_spec.is_local_type()) {
    auto store = FindStore(lookup_scope.name());
    if (!store.has_value()) {
      return status::NotFoundErrorBuilder()
             << "Scope for lookup: " << lookup_scope.name() << " not created.";
    }
    return store.value()->FindType(lookup_scope, type_spec);
  }
  ASSIGN_OR_RETURN(auto type_name,
                   ScopedName::FromIdentifier(type_spec.identifier()),
                   _ << "Obtaining type specification");
  for (size_t i = lookup_scope.size() + 1; i > 0; --i) {
    const ScopeName crt_name(
        lookup_scope.PrefixScopeName(i - 1).Subscope(type_name.scope_name()));
    auto store = FindStore(crt_name.name());
    if (store.has_value()) {
      if (store.value()->HasType(type_name.name())) {
        return store.value()->FindType(lookup_scope, type_spec);
      }
    }
  }
  if (type_name.scope_name().empty()) {
    return base_store_->FindType(lookup_scope, type_spec);
  }
  return status::NotFoundErrorBuilder()
         << "Cannot find type named `" << type_name.full_name() << "`, "
         << "`, from module: `" << lookup_scope.name() << "`";
}

absl::StatusOr<const TypeSpec*> GlobalTypeStore::FindTypeByName(
    absl::string_view name) const {
  return base_store_->FindTypeByName(name);
}

absl::Status GlobalTypeStore::AddScope(std::shared_ptr<ScopeName> scope_name) {
  if (scopes_.contains(scope_name->name())) {
    return status::AlreadyExistsErrorBuilder()
           << "Cannot overwrite module " << scope_name->name();
  }
  std::string key(scope_name->name());
  scopes_store_.emplace_back(
      std::make_unique<ScopeTypeStore>(std::move(scope_name), this));
  RET_CHECK(scopes_store_.back().get() != nullptr);
  scopes_.emplace(std::move(key), scopes_store_.back().get());
  return absl::OkStatus();
}

absl::Status GlobalTypeStore::AddAlias(const ScopeName& scope_name,
                                       const ScopeName& alias_name) {
  if (scopes_.contains(alias_name.name())) {
    return status::AlreadyExistsErrorBuilder()
           << "A type scope named: " << alias_name.name() << " already exists";
  }
  auto it = scopes_.find(scope_name.name());
  if (it == scopes_.end()) {
    return status::NotFoundErrorBuilder()
           << "Cannot file a type scoped names: " << scope_name.name()
           << " for adding an alias to it";
  }
  // Make a copy for safety:
  auto store_ptr = it->second;
  scopes_.emplace(alias_name.name(), store_ptr);
  return absl::OkStatus();
}

absl::StatusOr<const TypeSpec*> GlobalTypeStore::DeclareType(
    const ScopeName& scope_name, absl::string_view name,
    std::unique_ptr<TypeSpec> type_spec) {
  auto it = scopes_.find(scope_name.name());
  if (it == scopes_.end()) {
    std::string key(scope_name.name());
    scopes_store_.emplace_back(std::make_unique<ScopeTypeStore>(
        absl::make_unique<ScopeName>(scope_name), this));
    RET_CHECK(scopes_store_.back().get() != nullptr);
    std::tie(it, std::ignore) =
        scopes_.emplace(scope_name.name(), scopes_store_.back().get());
  }
  return it->second->DeclareType(scope_name, name, std::move(type_spec));
}

absl::Status GlobalTypeStore::CallRegistrationCallback(
    const ScopeName& scope_name, TypeSpec* type_spec) const {
  auto callback_it = callbacks_.find(scope_name.name());
  if (callback_it != callbacks_.end()) {
    RETURN_IF_ERROR(callback_it->second(type_spec));
  }
  return absl::OkStatus();
}

const ScopeName& GlobalTypeStore::scope_name() const {
  static const auto kEmptyScope = new ScopeName();
  return *kEmptyScope;
}

void GlobalTypeStore::AddRegistrationCallback(const ScopeName& scope_name,
                                              RegistrationCallback callback) {
  callbacks_[scope_name.name()] = std::move(callback);
}

void GlobalTypeStore::RemoveRegistrationCallback(const ScopeName& scope_name) {
  callbacks_.erase(scope_name.name());
}

ScopeTypeStore::ScopeTypeStore(std::shared_ptr<ScopeName> scope_name,
                               GlobalTypeStore* global_store)
    : TypeStore(),
      scope_name_(std::move(scope_name)),
      global_store_(CHECK_NOTNULL(global_store)) {}

const ScopeName& ScopeTypeStore::scope_name() const { return *scope_name_; }

TypeStore* ScopeTypeStore::GlobalStore() { return global_store_; }

bool ScopeTypeStore::HasType(absl::string_view type_name) const {
  return types_.contains(type_name);
}

std::string ScopeTypeStore::DebugNames() const {
  std::string s(absl::StrCat("Scope Type Store: ", scope_name_->name(), "\n"));
  for (const auto& it : types_) {
    absl::StrAppend(&s, "Type: ", it.first, ": ", it.second->full_name(), "\n");
  }
  return s;
}

absl::StatusOr<const TypeSpec*> ScopeTypeStore::FindType(
    const ScopeName& lookup_scope, const pb::TypeSpec& type_spec) {
  if (type_spec.is_local_type()) {
    return FindTypeLocal(lookup_scope, type_spec);
  }
  ASSIGN_OR_RETURN(const std::string type_name,
                   NameUtil::GetObjectName(type_spec.identifier()),
                   _ << "Obtaining type name.");
  auto type_it = types_.find(type_name);
  if (type_it == types_.end()) {
    return status::NotFoundErrorBuilder()
           << "Cannot find type `" << type_name << "` in scope `"
           << scope_name_->name() << "`";
  }
  const TypeSpec* spec = type_it->second.get();
  if (type_spec.argument().empty()) {
    return spec;
  }
  std::vector<TypeBindingArg> bind_arguments;
  for (auto argument : type_spec.argument()) {
    if (argument.has_int_value()) {
      bind_arguments.emplace_back(
          TypeBindingArg(static_cast<int>(argument.int_value())));
    } else if (argument.has_type_spec()) {
      ASSIGN_OR_RETURN(
          auto subtype,
          global_store_->FindType(lookup_scope, argument.type_spec()),
          _ << "Finding subtype `" << grammar::ToDsl(argument.type_spec()));
      bind_arguments.emplace_back(TypeBindingArg(std::move(subtype)));
    }
  }
  ASSIGN_OR_RETURN(auto bound_type, spec->Build(bind_arguments),
                   _ << "Binding type: " << spec->name());
  bound_type->set_scope_name(lookup_scope);
  bound_types_.emplace_back(std::move(bound_type));
  return bound_types_.back().get();
}

absl::StatusOr<const TypeSpec*> ScopeTypeStore::FindTypeLocal(
    const ScopeName& lookup_scope, const pb::TypeSpec& type_spec) {
  RET_CHECK(type_spec.is_local_type());
  RET_CHECK(lookup_scope.name() == scope_name_->name())
      << "Declaring local type in a wrong scope: " << lookup_scope.name()
      << " vs. " << scope_name_->name();
  ASSIGN_OR_RETURN(const std::string module_name,
                   NameUtil::GetModuleName(type_spec.identifier()),
                   _ << "Obtaining module name.");
  if (!module_name.empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "Local type name should not contain a module specifier "
              "for local type: "
           << grammar::ToDsl(type_spec);
  }
  ASSIGN_OR_RETURN(const std::string type_name,
                   NameUtil::GetObjectName(type_spec.identifier()),
                   _ << "Obtaining type name.");
  const TypeSpec* spec = nullptr;
  auto type_it = types_.find(type_name);
  if (type_it != types_.end()) {
    spec = type_it->second.get();
  }
  if (type_spec.argument().empty()) {
    if (spec) {
      return spec;
    }
    return DeclareLocalAnyType(type_name);
  }
  if (type_spec.argument().size() != 1 ||
      !type_spec.argument(0).has_type_spec()) {
    return status::InvalidArgumentErrorBuilder()
           << "Local type declaration expecting just one type spec argument "
              "for: "
           << type_name << " in: " << grammar::ToDsl(type_spec);
  }
  if (spec) {
    return status::AlreadyExistsErrorBuilder()
           << "Cannot redefine local type: " << type_name;
  }
  const pb::TypeSpec& arg_type_spec = type_spec.argument(0).type_spec();
  ASSIGN_OR_RETURN(auto subtype,
                   global_store_->FindType(lookup_scope, arg_type_spec),
                   _ << "Finding subtype `" << grammar::ToDsl(arg_type_spec)
                     << " for registering local type: " << type_name);
  return DeclareType(lookup_scope, type_name, subtype->Clone());
}

absl::StatusOr<const TypeSpec*> ScopeTypeStore::DeclareLocalAnyType(
    absl::string_view name) {
  pb::TypeSpec any_spec;
  any_spec.mutable_identifier()->add_name(std::string(kTypeNameAny));
  ASSIGN_OR_RETURN(auto subtype, global_store_->FindType(ScopeName(), any_spec),
                   _ << "Cannot find type Any for named local type "
                        "registration of: "
                     << name);
  return DeclareType(*scope_name_, name, subtype->Clone());
}

absl::StatusOr<const TypeSpec*> ScopeTypeStore::FindTypeByName(
    absl::string_view name) const {
  auto type_it = types_.find(name);
  if (type_it == types_.end()) {
    return status::NotFoundErrorBuilder()
           << "Cannot find type `" << name << "` in scope `"
           << scope_name_->name() << "`";
  }
  return type_it->second.get();
}

absl::StatusOr<const TypeSpec*> ScopeTypeStore::DeclareType(
    const ScopeName& scope_name, absl::string_view name,
    std::unique_ptr<TypeSpec> type_spec) {
  if (name.empty()) {
    name = type_spec->name();
  } else {
    type_spec->set_local_name(name);
  }
  if (types_.contains(name)) {
    return status::AlreadyExistsErrorBuilder()
           << "Cannot redeclare existing type `" << name << "`"
           << " in scope: `" << scope_name_->name() << "`";
  }
  type_spec->set_scope_name(*scope_name_);
  TypeSpec* added_type = types_.emplace(std::string(name), std::move(type_spec))
                             .first->second.get();
  if (global_store_) {
    RETURN_IF_ERROR(
        global_store_->CallRegistrationCallback(*scope_name_, added_type));
  }
  return added_type;
}

}  // namespace analysis
}  // namespace nudl
