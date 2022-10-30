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

#include "nudl/analysis/named_object.h"

#include <algorithm>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "nudl/status/status.h"

ABSL_FLAG(bool, nudl_short_analysis_proto, false,
          "This will output slightly succinct proto of analyzed code, "
          "so they can be parsed easily by humans.");

namespace nudl {
namespace analysis {

absl::string_view ObjectKindName(pb::ObjectKind kind) {
  switch (kind) {
    case pb::ObjectKind::OBJ_UNKNOWN:
      return "Unknown";
    case pb::ObjectKind::OBJ_VARIABLE:
      return "Variable";
    case pb::ObjectKind::OBJ_PARAMETER:
      return "Parameter";
    case pb::ObjectKind::OBJ_ARGUMENT:
      return "Argument";
    case pb::ObjectKind::OBJ_FIELD:
      return "Field";
    case pb::ObjectKind::OBJ_SCOPE:
      return "Scope";
    case pb::ObjectKind::OBJ_FUNCTION:
      return "Function";
    case pb::ObjectKind::OBJ_METHOD:
      return "Method";
    case pb::ObjectKind::OBJ_CONSTRUCTOR:
      return "Constructor";
    case pb::ObjectKind::OBJ_LAMBDA:
      return "Lambda";
    case pb::ObjectKind::OBJ_MODULE:
      return "Module";
    case pb::ObjectKind::OBJ_TYPE:
      return "Type";
    case pb::ObjectKind::OBJ_FUNCTION_GROUP:
      return "FunctionGroup";
    case pb::ObjectKind::OBJ_METHOD_GROUP:
      return "MethodGroup";
    case pb::ObjectKind::OBJ_TYPE_MEMBER_STORE:
      return "TypeMemberStore";
  }
  return "Unknown";
}

NamedObject::NamedObject(absl::string_view name) : name_(std::string(name)) {}

NamedObject::~NamedObject() {}

const std::string& NamedObject::name() const { return name_; }

absl::string_view NamedObject::kind_name() const {
  return ObjectKindName(kind());
}

absl::optional<NameStore*> NamedObject::name_store() { return {}; }

absl::optional<NameStore*> NamedObject::parent_store() const { return {}; }

std::string NamedObject::full_name() const {
  return absl::StrCat(kind_name(), " named: `", name(), "`");
}

bool NamedObject::IsAncestorOf(const NamedObject* object) const {
  while (object) {
    if (object == this) {
      return true;
    }
    object = object->parent_store().value_or(nullptr);
  }
  return false;
}

pb::NamedObjectRef NamedObject::ToProtoRef() const {
  pb::NamedObjectRef proto;
  proto.set_name(name());
  proto.set_kind(kind());
  return proto;
}

pb::NamedObject NamedObject::ToProtoObject() const {
  pb::NamedObject proto;
  proto.set_name(name());
  proto.set_kind(kind());
  return proto;
}

NameStore::NameStore(absl::string_view name) : NamedObject(name) {}

absl::optional<NameStore*> NameStore::name_store() { return this; }

absl::string_view NameStore::NormalizeLocalName(
    absl::string_view local_name) const {
  return absl::StripPrefix(local_name, "::");
}

BaseNameStore::BaseNameStore(absl::string_view name) : NameStore(name) {}

absl::StatusOr<NamedObject*> BaseNameStore::FindName(
    const ScopeName& lookup_scope, const ScopedName& scoped_name) {
  ASSIGN_OR_RETURN(auto store, FindChildStore(scoped_name.scope_name()),
                   _ << "Finding in :" << full_name());
  return store->GetName(scoped_name.name(), false);
}

absl::Status BaseNameStore::AddName(absl::string_view local_name,
                                    NamedObject* object) {
  CHECK_NOTNULL(object);
  RET_CHECK(!object->IsAncestorOf(this))
      << "Don't create object chains: " << object->name() << " => " << name();
  const auto it = named_objects_.find(NormalizeLocalName(local_name));
  if (it != named_objects_.end()) {
    return status::AlreadyExistsErrorBuilder()
           << full_name()
           << " already contains local object: " << it->second->full_name()
           << " under local name: " << local_name
           << ", while adding: " << object->full_name();
  }
  named_objects_.emplace(std::string(NormalizeLocalName(local_name)), object);
  return absl::OkStatus();
}

std::string BaseNameStore::DebugString() const { return DebugNames(); }

std::vector<std::string> BaseNameStore::DefinedNames() const {
  std::vector<std::string> names;
  names.reserve(named_objects_.size());
  for (const auto& it : named_objects_) {
    names.emplace_back(it.first);
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::string BaseNameStore::DebugNames() const {
  std::vector<std::string> names;
  names.reserve(named_objects_.size());
  for (const auto& it : named_objects_) {
    names.emplace_back(
        absl::StrCat("  ", it.first, " : ", it.second->full_name()));
  }
  return absl::StrCat("Name Store: ", name(), " / ", full_name(), "\n",
                      absl::StrJoin(names, "\n"));
}

pb::NamedObject BaseNameStore::ToProtoObject() const {
  pb::NamedObject proto = NameStore::ToProtoObject();
  for (const auto& it : named_objects_) {
    auto child = proto.add_child();
    *child = it.second->ToProtoObject();
    child->set_local_name(it.first);
  }
  return proto;
}

bool BaseNameStore::HasName(absl::string_view local_name,
                            bool in_self_only) const {
  return local_name.empty() ||
         named_objects_.contains(NormalizeLocalName(local_name));
}

absl::StatusOr<NamedObject*> BaseNameStore::GetName(
    absl::string_view local_name, bool in_self_only) {
  if (local_name.empty()) {
    return this;
  }
  auto it = named_objects_.find(NormalizeLocalName(local_name));
  if (it == named_objects_.end()) {
    return status::NotFoundErrorBuilder()
           << "Cannot find local name: `" << local_name << "` in "
           << full_name();
  }
  return it->second;
}

absl::Status BaseNameStore::AddChildStore(absl::string_view local_name,
                                          NameStore* store) {
  CHECK_NOTNULL(store);
  auto it = child_name_stores_.find(NormalizeLocalName(local_name));
  if (it != child_name_stores_.end()) {
    return status::AlreadyExistsErrorBuilder()
           << full_name()
           << " already contains child name stored: " << it->second->full_name()
           << " registered under local name: " << local_name
           << "; while adding child store: " << store->full_name();
  }
  child_name_stores_.emplace(std::string(NormalizeLocalName(local_name)),
                             store);
  return AddName(NormalizeLocalName(local_name), store);
}

absl::Status BaseNameStore::AddOwnedChildStore(
    absl::string_view local_name, std::unique_ptr<NameStore> store) {
  RETURN_IF_ERROR(AddChildStore(local_name, store.get()));
  owned_stores_.emplace_back(std::move(store));
  return absl::OkStatus();
}

absl::StatusOr<NameStore*> BaseNameStore::FindChildStore(
    const ScopeName& lookup_scope) {
  if (lookup_scope.empty()) {
    return this;
  }
  for (size_t i = 1; i <= lookup_scope.size(); ++i) {
    const std::string prefix(NormalizeLocalName(lookup_scope.PrefixName(i)));
    auto it = child_name_stores_.find(prefix);
    if (it != child_name_stores_.end()) {
      auto result = it->second->FindChildStore(lookup_scope.SuffixScopeName(i));
      if (result.ok()) {
        return result;
      }
    }
  }
  return status::NotFoundErrorBuilder()
         << "Cannot find `" << lookup_scope.name()
         << "` "
            "in: "
         << full_name();
}

WrappedNameStore::WrappedNameStore(absl::string_view name,
                                   NameStore* wrapped_store)
    : NameStore(name), wrapped_store_(CHECK_NOTNULL(wrapped_store)) {}

pb::ObjectKind WrappedNameStore::kind() const { return wrapped_store_->kind(); }

const TypeSpec* WrappedNameStore::type_spec() const {
  return wrapped_store_->type_spec();
}

std::string WrappedNameStore::DebugString() const { return DebugNames(); }

std::vector<std::string> WrappedNameStore::DefinedNames() const {
  return wrapped_store_->DefinedNames();
}

pb::NamedObject WrappedNameStore::ToProtoObject() const {
  pb::NamedObject proto = NameStore::ToProtoObject();
  proto.set_wrapped_store(wrapped_store_->full_name());
  return proto;
}

std::string WrappedNameStore::DebugNames() const {
  return absl::StrCat("Wrapped Name Store: ", name(),
                      wrapped_store_->DebugString());
}

absl::StatusOr<NamedObject*> WrappedNameStore::FindName(
    const ScopeName& lookup_scope, const ScopedName& scoped_name) {
  ASSIGN_OR_RETURN(auto object,
                   wrapped_store_->FindName(lookup_scope, scoped_name),
                   _ << "Finding in: " << full_name());
  return object;
}

absl::Status WrappedNameStore::AddName(absl::string_view local_name,
                                       NamedObject* object) {
  RETURN_IF_ERROR(wrapped_store_->AddName(local_name, object))
      << "Adding name to: " << full_name();
  return absl::OkStatus();
}

bool WrappedNameStore::HasName(absl::string_view local_name,
                               bool in_self_only) const {
  return wrapped_store_->HasName(local_name, in_self_only);
}

absl::StatusOr<NamedObject*> WrappedNameStore::GetName(
    absl::string_view local_name, bool in_self_only) {
  ASSIGN_OR_RETURN(auto object,
                   wrapped_store_->GetName(local_name, in_self_only),
                   _ << "Finding in: " << full_name());
  return object;
}

absl::Status WrappedNameStore::AddChildStore(absl::string_view local_name,
                                             NameStore* store) {
  RETURN_IF_ERROR(wrapped_store_->AddChildStore(local_name, store))
      << "Adding child to: " << full_name();
  return absl::OkStatus();
}

absl::Status WrappedNameStore::AddOwnedChildStore(
    absl::string_view local_name, std::unique_ptr<NameStore> store) {
  RETURN_IF_ERROR(
      wrapped_store_->AddOwnedChildStore(local_name, std::move(store)))
      << "Adding child to: " << full_name();
  return absl::OkStatus();
}

absl::StatusOr<NameStore*> WrappedNameStore::FindChildStore(
    const ScopeName& lookup_scope) {
  ASSIGN_OR_RETURN(auto store, wrapped_store_->FindChildStore(lookup_scope),
                   _ << "Finding child in: " << full_name());
  if (store == wrapped_store_) {
    return this;
  }
  return store;
}

}  // namespace analysis
}  // namespace nudl
