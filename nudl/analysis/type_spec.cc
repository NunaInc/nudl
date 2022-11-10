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

#include "nudl/analysis/type_spec.h"

#include <deque>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "glog/logging.h"
#include "nudl/analysis/function.h"
#include "nudl/analysis/names.h"
#include "nudl/proto/analysis.pb.h"
#include "nudl/status/status.h"
#include "nudl/testing/stacktrace.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);

namespace nudl {
namespace analysis {

TypeMemberStore::TypeMemberStore(const TypeSpec* spec,
                                 std::shared_ptr<NameStore> ancestor)
    : BaseNameStore(CHECK_NOTNULL(spec)->name()),
      type_spec_(spec),
      ancestor_(std::move(ancestor)) {
  AddMemberType(spec);
}

TypeMemberStore::~TypeMemberStore() {
  for (const auto& it : bound_children_) {
    CHECK(it.second->binding_parent().has_value() &&
          it.second->binding_parent().value() == this);
    it.second->RemoveBindingParent();
  }
}

const TypeSpec* TypeMemberStore::type_spec() const {
  // We need to return a value here:
  if (type_spec_.has_value()) {
    return type_spec_.value();  // most of the time this holds:
  }
  if (binding_parent_.has_value()) {
    return binding_parent_.value()->type_spec();
  }
  if (ancestor_) {
    return ancestor_->type_spec();
  }
  return TypeUnknown::Instance();
}

NameStore* TypeMemberStore::ancestor() const { return ancestor_.get(); }

absl::optional<TypeMemberStore*> TypeMemberStore::binding_parent() const {
  return binding_parent_;
}

const std::string& TypeMemberStore::binding_signature() const {
  return binding_signature_;
}

std::shared_ptr<NameStore> TypeMemberStore::ancestor_ptr() const {
  return ancestor_;
}

const absl::flat_hash_map<std::string, std::shared_ptr<TypeMemberStore>>&
TypeMemberStore::bound_children() const {
  return bound_children_;
}

pb::ObjectKind TypeMemberStore::kind() const {
  return pb::ObjectKind::OBJ_TYPE_MEMBER_STORE;
}

std::string TypeMemberStore::full_name() const {
  return absl::StrCat("Members of ", type_spec()->full_name());
}

std::vector<const NameStore*> TypeMemberStore::FindConstBindingOrder() const {
  auto result = const_cast<TypeMemberStore*>(this)->FindBindingOrder();
  return std::vector<const NameStore*>(result.begin(), result.end());
}

std::vector<NameStore*> TypeMemberStore::FindBindingOrder() {
  const TypeSpec* spec = type_spec();
  std::vector<NameStore*> result;
  absl::flat_hash_set<const NameStore*> traversed;
  std::deque<TypeMemberStore*> stack;
  stack.push_back(this);
  traversed.emplace(this);
  while (!stack.empty()) {
    TypeMemberStore* crt = stack.front();
    stack.pop_front();
    for (const auto& it : crt->bound_children_) {
      if (it.second->type_spec()->IsAncestorOf(*spec)) {
        result.emplace_back(it.second.get());
      }
    }
    if (crt->type_spec()->IsAncestorOf(*spec)) {
      result.emplace_back(crt);
    }
    if (crt->binding_parent_.has_value() &&
        crt->binding_parent_.value()->type_spec()->IsAncestorOf(*spec) &&
        !traversed.contains(crt->binding_parent_.value())) {
      stack.emplace_back(crt->binding_parent_.value());
      traversed.emplace(crt->binding_parent_.value());
    }
    if (crt->ancestor_ && crt->ancestor_->type_spec()->IsAncestorOf(*spec) &&
        !traversed.contains(crt->ancestor_.get())) {
      traversed.emplace(crt->ancestor_.get());
      if (crt->ancestor_->kind() == pb::ObjectKind::OBJ_TYPE_MEMBER_STORE) {
        auto store_ancestor =
            static_cast<TypeMemberStore*>(crt->ancestor_.get());
        stack.emplace_back(store_ancestor);
      } else {
        result.emplace_back(crt->ancestor_.get());
      }
    }
  }
  return result;
}

std::shared_ptr<TypeMemberStore> TypeMemberStore::AddBinding(
    absl::string_view signature, const TypeSpec* type_spec) {
  if (binding_parent_.has_value()) {
    return binding_parent_.value()->AddBinding(std::move(signature), type_spec);
  }
  auto it = bound_children_.find(signature);
  if (it != bound_children_.end()) {
    it->second->AddMemberType(type_spec);
    return it->second;
  }
  auto new_child = std::make_shared<TypeMemberStore>(type_spec, ancestor_);
  new_child->SetupBindingParent(signature, this);
  bound_children_.emplace(signature, new_child);
  return new_child;
}

void TypeMemberStore::RemoveBinding(absl::string_view signature) {
  auto it = bound_children_.find(signature);
  if (it != bound_children_.end()) {
    it->second->RemoveBindingParent();
    bound_children_.erase(it);
  }
}

void TypeMemberStore::SetupBindingParent(absl::string_view signature,
                                         TypeMemberStore* binding_parent) {
  CHECK(!binding_parent_.has_value());
  binding_parent_ = binding_parent;
  binding_signature_ = std::string(signature);
}

void TypeMemberStore::RemoveBindingParent() {
  CHECK(binding_parent_.has_value());
  binding_parent_.reset();
  binding_signature_.clear();
}

void TypeMemberStore::AddMemberType(const TypeSpec* member_type) {
  member_types_.emplace(member_type);
  if (!type_spec_.has_value()) {
    type_spec_ = member_type;
  }
}

void TypeMemberStore::RemoveMemberType(const TypeSpec* member_type) {
  member_types_.erase(member_type);
  if (type_spec_.has_value() && member_type == type_spec_.value()) {
    if (member_types_.empty()) {
      type_spec_.reset();
    } else {
      type_spec_ = *member_types_.begin();
    }
  }
}

absl::Status TypeMemberStore::CheckAddedObject(absl::string_view local_name,
                                               NamedObject* object) {
  pb::ObjectKind kind = CHECK_NOTNULL(object)->kind();
  if (!Function::IsMethodKind(*object) && kind != pb::ObjectKind::OBJ_FIELD) {
    return status::InvalidArgumentErrorBuilder()
           << "Type members can only be fields or methods. "
           << "Got: " << object->full_name() << " to be added "
           << " to store: " << full_name();
  }
  if (!NameUtil::IsValidName(NormalizeLocalName(local_name))) {
    return status::InvalidArgumentErrorBuilder()
           << "Type member store: " << full_name()
           << " requires valid local names. Got: `" << local_name
           << "` for: " << object->full_name();
  }
  return absl::OkStatus();
}

bool TypeMemberStore::HasName(absl::string_view local_name,
                              bool in_self_only) const {
  if (BaseNameStore::HasName(local_name, in_self_only)) {
    return true;
  }
  if (!in_self_only) {
    for (auto store : FindConstBindingOrder()) {
      if (store != this && store->HasName(local_name, true)) {
        return true;
      }
    }
  }
  return false;
}

absl::StatusOr<NamedObject*> TypeMemberStore::GetName(
    absl::string_view local_name, bool in_self_only) {
  auto result = BaseNameStore::GetName(local_name, in_self_only);
  if (result.ok() || in_self_only) {
    return result;
  }
  std::vector<absl::Status> result_status;
  result_status.emplace_back(result.status());
  for (auto store : FindBindingOrder()) {
    if (store != this) {
      result = store->GetName(local_name, true);
      if (result.ok()) {
        return result;
      }
      result_status.emplace_back(result.status());
    }
  }
  return status::JoinStatus(result_status);
}

absl::Status TypeMemberStore::AddName(absl::string_view local_name,
                                      NamedObject* object) {
  RETURN_IF_ERROR(CheckAddedObject(local_name, object));
  return BaseNameStore::AddName(local_name, object);
}

absl::Status TypeMemberStore::AddChildStore(absl::string_view local_name,
                                            NameStore* store) {
  RETURN_IF_ERROR(CheckAddedObject(local_name, store));
  return BaseNameStore::AddChildStore(local_name, store);
}

int TypeSpec::NextTypeId() {
  static std::atomic<int> next_id{pb::TypeId::FIRST_CUSTOM_ID};
  return next_id.fetch_add(1, std::memory_order_relaxed);
}

TypeSpec::TypeSpec(int type_id, absl::string_view name,
                   std::shared_ptr<TypeMemberStore> type_member_store,
                   bool is_bound_type, const TypeSpec* ancestor,
                   std::vector<const TypeSpec*> parameters,
                   absl::optional<const TypeSpec*> original_bind)
    : NamedObject(name),
      type_id_(type_id),
      type_member_store_(std::move(type_member_store)),
      is_bound_type_(is_bound_type),
      ancestor_(ancestor),
      parameters_(std::move(parameters)),
      original_bind_(original_bind) {
  if (!type_member_store_) {
    type_member_store_ = std::make_shared<TypeMemberStore>(
        this, ancestor ? ancestor->type_member_store_ptr() : nullptr);
  } else {
    type_member_store_->AddMemberType(this);
  }
}

TypeSpec::~TypeSpec() { type_member_store_->RemoveMemberType(this); }

int TypeSpec::type_id() const { return type_id_; }

bool TypeSpec::is_bound_type() const { return is_bound_type_; }

const TypeSpec* TypeSpec::ancestor() const { return ancestor_; }

const std::vector<const TypeSpec*>& TypeSpec::parameters() const {
  return parameters_;
}

TypeMemberStore* TypeSpec::type_member_store() const {
  return type_member_store_.get();
}

std::shared_ptr<TypeMemberStore> TypeSpec::type_member_store_ptr() const {
  return type_member_store_;
}

absl::optional<const TypeSpec*> TypeSpec::original_bind() const {
  return original_bind_;
}

pb::ObjectKind TypeSpec::kind() const { return pb::ObjectKind::OBJ_TYPE; }

absl::optional<NameStore*> TypeSpec::name_store() {
  return CHECK_NOTNULL(type_member_store_.get());
}

std::string TypeSpec::wrap_local_name(std::string s) const {
  if (local_name_.empty()) {
    return s;
  }
  return absl::StrCat("{ ", local_name_, " : ", s, " }");
}

std::string TypeSpec::full_name() const {
  std::string s;
  absl::StrAppend(&s, name());
  if (!parameters_.empty()) {
    absl::StrAppend(&s, "<", parameters_.front()->full_name());
    for (auto it = parameters_.begin() + 1; it != parameters_.end(); ++it) {
      absl::StrAppend(&s, ", ", (*it)->full_name());
    }
    absl::StrAppend(&s, ">");
  }
  return wrap_local_name(std::move(s));
}

pb::ExpressionTypeSpec TypeSpec::ToProto() const {
  pb::ExpressionTypeSpec proto;
  if (absl::GetFlag(FLAGS_nudl_short_analysis_proto)) {
    // Succint type specification.
    proto.set_name(full_name());
    return proto;
  }
  if (type_id() < 0 || type_id() >= pb::TypeId::FIRST_CUSTOM_ID) {
    proto.set_custom_type_id(type_id());
  } else {
    proto.set_type_id(static_cast<pb::TypeId>(type_id()));
  }
  proto.set_name(name());
  for (auto param : parameters_) {
    *proto.add_parameter() = param->ToProto();
  }
  if (!scope_name().empty()) {
    *proto.mutable_scope_name() = scope_name().ToProto();
  }
  return proto;
}

pb::TypeSpec TypeSpec::ToTypeSpecProto() const {
  pb::TypeSpec proto;
  proto.mutable_identifier()->add_name(name());
  for (auto param : parameters_) {
    *proto.add_argument()->mutable_type_spec() = param->ToTypeSpecProto();
  }
  return proto;
}

const std::string& TypeSpec::local_name() const { return local_name_; }

void TypeSpec::set_local_name(absl::string_view local_name) {
  CHECK(local_name.empty() || NameUtil::IsValidName(local_name))
      << " --> `" << local_name << "`";
  local_name_ = std::string(local_name);
}

absl::optional<NameStore*> TypeSpec::parent_store() const {
  return definition_scope_;
}

absl::optional<NameStore*> TypeSpec::definition_scope() const {
  return definition_scope_;
}

void TypeSpec::set_definition_scope(absl::optional<NameStore*> obj) {
  CHECK(!definition_scope_.has_value() ||
        (obj.has_value() && definition_scope_.value() == obj.value()));
  definition_scope_ = obj;
}

const ScopeName& TypeSpec::scope_name() const {
  static const auto kEmptyScope = new ScopeName();
  if (scope_name_.has_value()) {
    return scope_name_.value();
  }
  return *kEmptyScope;
}

void TypeSpec::set_scope_name(ScopeName scope_name) {
  scope_name_ = std::move(scope_name);
}

bool TypeSpec::IsBound() const {
  if (!is_bound_type_) {
    return false;
  }
  if (parameters_.empty()) {
    return true;
  }
  for (auto param : parameters_) {
    if (!param->IsBound()) {
      return false;
    }
  }
  return true;
}

bool TypeSpec::IsAncestorOf(const TypeSpec& type_spec) const {
  const TypeSpec* crt_type_spec = &type_spec;
  while (crt_type_spec) {
    if (type_id_ == crt_type_spec->type_id()) {
      return HasAncestorParameters(type_spec);
    }
    crt_type_spec = crt_type_spec->ancestor();
  }
  return false;
}

bool TypeSpec::HasAncestorParameters(const TypeSpec& type_spec) const {
  if (parameters_.empty()) {
    return true;
  }
  if (parameters_.size() != type_spec.parameters().size()) {
    if (IsResultTypeComparable(type_spec)) {
      return parameters_.front()->IsAncestorOf(*type_spec.ResultType());
    }
    return false;
  }
  for (size_t i = 0; i < parameters_.size(); ++i) {
    if (!parameters_[i]->IsAncestorOf(*type_spec.parameters()[i])) {
      return false;
    }
  }
  return true;
}

bool TypeSpec::IsEqual(const TypeSpec& type_spec) const {
  if (type_id_ != type_spec.type_id() ||
      parameters_.size() != type_spec.parameters().size()) {
    return false;
  }
  for (size_t i = 0; i < parameters_.size(); ++i) {
    if (!parameters_[i]->IsEqual(*type_spec.parameters()[i])) {
      return false;
    }
  }
  return true;
}

bool TypeSpec::IsConvertibleFrom(const TypeSpec& type_spec) const {
  const TypeSpec* crt_type_spec = &type_spec;
  while (crt_type_spec) {
    if (type_id_ == crt_type_spec->type_id()) {
      return HasConvertibleParameters(type_spec);
    }
    crt_type_spec = crt_type_spec->ancestor();
  }
  return false;
}

bool TypeSpec::HasConvertibleParameters(const TypeSpec& type_spec) const {
  if (parameters_.empty()) {
    return true;
  }
  if (parameters_.size() != type_spec.parameters().size()) {
    if (IsResultTypeComparable(type_spec)) {
      return parameters_.front()->IsConvertibleFrom(*type_spec.ResultType());
    }
    return false;
  }
  for (size_t i = 0; i < parameters_.size(); ++i) {
    if (parameters_[i]->IsBound()) {
      if (!type_spec.parameters()[i]->IsEqual(*parameters_[i])) {
        return false;
      }
    } else if (!parameters_[i]->IsConvertibleFrom(*type_spec.parameters()[i])) {
      return false;
    }
  }
  return true;
}

bool TypeSpec::IsResultTypeComparable(const TypeSpec& type_spec) const {
  return (parameters_.size() == 1 && type_spec.ResultType() && IsIterable() &&
          type_spec.IsIterable());
}

bool TypeSpec::IsGeneratedByThis(const TypeSpec& type_spec) const {
  return (type_spec.original_bind().has_value() &&
          type_spec.original_bind().value() == this);
}

const TypeSpec* TypeSpec::ResultType() const {
  if (IsIterable() && !parameters_.empty()) {
    return parameters_.back();
  }
  return nullptr;
}

const TypeSpec* TypeSpec::IndexType() const { return nullptr; }

const TypeSpec* TypeSpec::IndexedType() const { return nullptr; }

bool TypeSpec::IsIterable() const {
  if (ancestor_) {
    return ancestor_->IsIterable();
  }
  return false;
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeSpec::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings));
  auto result = Clone();
  result->parameters_ = types;
  RETURN_IF_ERROR(result->UpdateBindingStore(bindings));
  return {std::move(result)};
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeSpec::Build(
    const std::vector<TypeBindingArg>& bindings) const {
  return Bind(bindings);
}

absl::Status TypeSpec::UpdateBindingStore(
    const std::vector<TypeBindingArg>& bindings) {
  size_t num_non_any = 0;
  for (const auto& binding : bindings) {
    if (std::holds_alternative<const TypeSpec*>(binding) &&
        std::get<const TypeSpec*>(binding)->type_id() != pb::TypeId::ANY_ID) {
      ++num_non_any;
    }
  }
  if (!num_non_any) {
    return absl::OkStatus();
  }
  std::string signature = TypeBindingSignature(bindings);
  auto bound_store = type_member_store_->AddBinding(signature, this);
  type_member_store_->RemoveMemberType(this);
  type_member_store_ = std::move(bound_store);
  return absl::OkStatus();
}

std::string TypeSpec::TypeSignature() const {
  std::string s(name());  // absl::StrCat("T", type_id()));
  // if (!local_name().empty()) {
  //  absl::StrAppend(&s, "_L_", local_name(), "_");
  // }
  if (parameters_.empty()) {
    return s;
  }
  absl::StrAppend(&s, "__");
  for (size_t i = 0; i < parameters_.size(); ++i) {
    if (i) {
      absl::StrAppend(&s, "_");
    }
    absl::StrAppend(&s, parameters_[i]->TypeSignature());
  }
  absl::StrAppend(&s, "__");
  return s;
}

namespace {
std::string TypeBindingSignatureJoin(
    const absl::Span<const std::string> components) {
  return absl::StrCat("TS_", absl::StrJoin(components, "_s_"), "_");
}
}  // namespace

std::string TypeSpec::TypeBindingSignature(
    const absl::Span<const TypeSpec* const> type_arguments) {
  std::vector<std::string> components;
  components.reserve(type_arguments.size());
  for (const auto& ta : type_arguments) {
    components.emplace_back(ta->TypeSignature());
  }
  return TypeBindingSignatureJoin(components);
}

std::string TypeSpec::TypeBindingSignature(
    const std::vector<TypeBindingArg>& type_arguments) {
  std::vector<std::string> components;
  components.reserve(type_arguments.size());
  for (const auto& ta : type_arguments) {
    if (std::holds_alternative<const TypeSpec*>(ta)) {
      components.emplace_back(std::get<const TypeSpec*>(ta)->TypeSignature());
    } else {
      components.emplace_back(absl::StrCat("_i_", std::get<int>(ta)));
    }
  }
  return TypeBindingSignatureJoin(components);
}

absl::StatusOr<std::vector<const TypeSpec*>> TypeSpec::TypesFromBindings(
    const std::vector<TypeBindingArg>& bindings, bool check_params,
    absl::optional<size_t> minimum_parameters) const {
  if (check_params && bindings.size() > parameters_.size()) {
    return status::InvalidArgumentErrorBuilder()
           << "Expecting " << parameters_.size() << " arguments "
           << "for binding " << full_name() << " - got: " << bindings.size();
  }
  std::vector<const TypeSpec*> types;
  types.reserve(bindings.size());
  for (size_t i = 0; i < bindings.size(); ++i) {
    if (absl::holds_alternative<const TypeSpec*>(bindings[i])) {
      types.emplace_back(absl::get<const TypeSpec*>(bindings[i]));
    } else {
      return status::InvalidArgumentErrorBuilder()
             << "Expecting only types for binding arguments of " << full_name()
             << " - missed one at index: " << i;
    }
    if (check_params &&
        !parameters_[i]->IsAncestorOf(*CHECK_NOTNULL(types.back()))) {
      return status::InvalidArgumentErrorBuilder()
             << "Expecting an argument of type: " << parameters_[i]->full_name()
             << " for binding parameter " << i
             << ". Got: " << types.back()->full_name()
             << ". In type binding of: " << full_name();
    }
  }
  if (check_params) {
    if (minimum_parameters.has_value()) {
      if (types.size() < minimum_parameters.value()) {
        return status::InvalidArgumentErrorBuilder()
               << "Expecting at least " << minimum_parameters.value()
               << " arguments and at most: " << parameters_.size()
               << " for binding type " << full_name()
               << " - got: " << bindings.size();
      }
    } else if (types.size() < parameters_.size()) {
      return status::InvalidArgumentErrorBuilder()
             << "Expecting " << parameters_.size() << " arguments "
             << "for binding " << full_name() << " - got: " << bindings.size();
    }
  }
  return {std::move(types)};
}

absl::Status TypeSpec::set_name(absl::string_view name) {
  if (is_name_set_) {
    return absl::FailedPreconditionError("Name already set for TypeSpec.");
  }
  ASSIGN_OR_RETURN(name_, NameUtil::ValidatedName(std::string(name)),
                   _ << "Setting name of : " << full_name());
  is_name_set_ = true;
  return absl::OkStatus();
}

bool TypeSpec::IsBasicTypeId(int type_id) {
  static auto kBasicTypeIds = new absl::flat_hash_set<int>({
      pb::TypeId::NUMERIC_ID,
      pb::TypeId::INT_ID,
      pb::TypeId::INT8_ID,
      pb::TypeId::INT16_ID,
      pb::TypeId::INT32_ID,
      pb::TypeId::UINT_ID,
      pb::TypeId::UINT8_ID,
      pb::TypeId::UINT16_ID,
      pb::TypeId::UINT32_ID,
      pb::TypeId::BOOL_ID,
      pb::TypeId::FLOAT32_ID,
      pb::TypeId::FLOAT64_ID,
  });
  return kBasicTypeIds->contains(type_id);
}

bool TypeSpec::IsBasicType() const {
  if (IsBasicTypeId(type_id())) {
    return true;
  }
  if (type_id() == pb::TypeId::NULLABLE_ID && ResultType()) {
    return ResultType()->IsBasicType();
  }
  return false;
}

absl::StatusOr<pb::Expression> TypeSpec::DefaultValueExpression() const {
  if (ancestor_) {
    return ancestor_->DefaultValueExpression();
  }
  return status::UnimplementedErrorBuilder()
         << "Cannot build default value expression for: " << full_name();
}

namespace {
const TypeSpec* FindUnionMatch(const TypeSpec* src_param,
                               const TypeSpec* type_spec) {
  if (src_param->type_id() != pb::TypeId::UNION_ID) {
    return src_param;
  }
  const TypeSpec* best_match = src_param;
  for (auto param : src_param->parameters()) {
    if (param->IsAncestorOf(*type_spec) &&
        (best_match == src_param || best_match->IsAncestorOf(*param))) {
      best_match = param;
    }
  }
  return best_match;
}
}  // namespace

absl::Status LocalNamesRebinder::RecordLocalName(const TypeSpec* src_param,
                                                 const TypeSpec* type_spec) {
  if (src_param->local_name().empty()) {
    return absl::OkStatus();
  }
  auto it = local_types_.find(src_param->local_name());
  if (it == local_types_.end()) {
    local_types_.emplace(src_param->local_name(), type_spec);
    return absl::OkStatus();
  } else if (it->second->IsEqual(*type_spec)) {
    return absl::OkStatus();
  }

  // Function that determines if we should swap t2 in place of t1:
  auto swap_types = [&it](const TypeSpec* t1,
                          const TypeSpec* t2) -> absl::StatusOr<bool> {
    if (t2->IsBound() && (!t1->IsBound() || t1->IsAncestorOf(*t2))) {
      return true;
    } else if (!t1->IsConvertibleFrom(*t2) && !t2->IsConvertibleFrom(*t1)) {
      return status::InvalidArgumentErrorBuilder()
             << "Named type: " << it->first
             << " is bound to two incompatible (sub)argument types: "
             << t1->full_name() << " and " << t2->full_name();
    }
    return false;
  };
  // Helper to allocate a type and set it in the iterator:
  auto set_new_type =
      [this, &it](absl::StatusOr<std::unique_ptr<TypeSpec>> t) -> absl::Status {
    RETURN_IF_ERROR(t.status());
    it->second = t.value().get();
    allocated_types.emplace_back(std::move(t).value());
    return absl::OkStatus();
  };
  const TypeSpec* t1 = it->second;
  const TypeSpec* t2 = type_spec;

  //////////////////////////////////////////////////////////////////////////////
  //
  // We may need to update the existing type t1 with the new type t2.
  // Here is what we do:
  //
  //     t1: Existing     t2: New         => Action
  // ------------------------------------------------------------------
  // 1:  Null          <- Any             => Null
  // 2:  Null          <- Nullable<Any>   => Nullable<Any> ???
  // 3:  Null          <- Nullable<X>     => Nullable<X>
  // 4:  Null          <- X               => Nullable<X>
  // 5:  Nullable<A>   <- Null            => unchanged
  // 6:  Nullable<Y>   <- Nullable<X>     => typecheck & Nullable<X or Y>
  // 7:  Nullable<Y>   <- X               => typecheck & Nullable<X or Y>
  // 8:  Any           <- Null / X        => Null / X
  // 9:  X             <- Null            => Nullable<X>
  // 10: X             <- Nullable<Y>     => Nullable<X or Y>
  // 11: X             <- Y               => swap_types
  //
  if (TypeUtils::IsNullType(*t1)) {
    if (TypeUtils::IsAnyType(*t2)) {  // 1:
      return absl::OkStatus();
    } else if (TypeUtils::IsNullableType(*t2)) {  // 2: 3:
      it->second = t2;
    } else {  // 4:
      return set_new_type(t1->Bind({TypeBindingArg{t2}}));
    }
  } else if (TypeUtils::IsNullableType(*t1)) {
    if (TypeUtils::IsNullType(*t2)) {  // 5:
      return absl::OkStatus();
    } else if (TypeUtils::IsNullableType(*t2)) {  // 6:
      ASSIGN_OR_RETURN(auto do_swap, swap_types(t1, t2));
      if (do_swap) {
        it->second = t2;
      }
    } else {  // 7:
      ASSIGN_OR_RETURN(
          auto do_swap, swap_types(t1->parameters().back(), t2),
          _ << " Checking subtype of source type: " << t1->full_name());
      if (do_swap) {
        return set_new_type(t1->Bind({TypeBindingArg{t2}}));
      }
    }
  } else if (TypeUtils::IsAnyType(*t1)) {  // 8:
    ASSIGN_OR_RETURN(auto do_swap, swap_types(t1, t2));
    if (do_swap) {
      it->second = t2;
    }
  } else if (TypeUtils::IsNullType(*t2)) {
    return set_new_type(t2->Bind({TypeBindingArg{t1}}));
  } else if (TypeUtils::IsNullableType(*t2)) {
    ASSIGN_OR_RETURN(
        auto do_swap, swap_types(t1, t2->parameters().back()),
        _ << " Checking subtype of call type: " << t1->full_name());
    if (do_swap) {
      it->second = t2;
    } else {
      return set_new_type(t2->Bind({TypeBindingArg{t1}}));
    }
  } else {
    ASSIGN_OR_RETURN(auto do_swap, swap_types(t1, t2));
    if (do_swap) {
      it->second = t1;
    }
  }
  return absl::OkStatus();
}

absl::Status LocalNamesRebinder::ProcessType(const TypeSpec* src_param,
                                             const TypeSpec* type_spec) {
  RETURN_IF_ERROR(RecordLocalName(src_param, type_spec));
  const TypeSpec* original_type = src_param;
  src_param = FindUnionMatch(src_param, type_spec);
  if (original_type != src_param) {
    RETURN_IF_ERROR(RecordLocalName(src_param, type_spec));
  }
  if (TypeUtils::IsFunctionType(*src_param)) {
    if (!TypeUtils::IsFunctionType(*type_spec) ||
        src_param->parameters().empty()) {
      return status::InvalidArgumentErrorBuilder()
             << "Cannot process type for rebinding: " << src_param->full_name()
             << " with non-function or unbound type hint: "
             << type_spec->full_name();
    }
    if (type_spec->parameters().empty()) {
      return absl::OkStatus();  // Not yet bound
    }
    const size_t num_type_params = type_spec->parameters().size() - 1;
    const size_t num_src_params = src_param->parameters().size() - 1;
    for (size_t i = 0; i < num_src_params && i < num_type_params; ++i) {
      RETURN_IF_ERROR(
          ProcessType(src_param->parameters()[i], type_spec->parameters()[i]))
          << "In subtype " << i << " of " << src_param->full_name() << " and "
          << type_spec->full_name();
    }
    RETURN_IF_ERROR(ProcessType(src_param->parameters().back(),
                                type_spec->parameters().back()))
        << "In return type of function types " << src_param->full_name()
        << " and " << type_spec->full_name();
  } else if (type_spec->parameters().size() == src_param->parameters().size()) {
    for (size_t i = 0; i < type_spec->parameters().size(); ++i) {
      RETURN_IF_ERROR(
          ProcessType(src_param->parameters()[i], type_spec->parameters()[i]))
          << "In subtype " << i << " of " << src_param->full_name() << " and "
          << type_spec->full_name();
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<const TypeSpec*> LocalNamesRebinder::RebuildType(
    const TypeSpec* src_param, const TypeSpec* type_spec) {
  src_param = FindUnionMatch(src_param, type_spec);
  std::vector<TypeBindingArg> args;
  bool needs_rebinding = false;
  args.reserve(src_param->parameters().size());
  bool is_function = false;
  size_t num_type_params = type_spec->parameters().size();
  size_t num_src_params = src_param->parameters().size();

  // This is to avert rebinding a type_spec that was created as a parametrized
  // type from src_param (e.g. a Tuple from a TupleJoin or similar).
  if (type_spec->original_bind().has_value() &&
      (type_spec->original_bind().value() == src_param)) {
    return type_spec;
  }

  if (TypeUtils::IsFunctionType(*src_param)) {
    if (!TypeUtils::IsFunctionType(*type_spec) ||
        src_param->parameters().empty() || type_spec->parameters().empty()) {
      return status::InvalidArgumentErrorBuilder()
             << "Cannot rebuilt type: " << src_param->full_name()
             << " with non-function or unbound type hint: "
             << type_spec->full_name();
    }
    is_function = true;
    num_type_params -= 1;
    num_src_params -= 1;
  }
  for (size_t i = 0; i < num_src_params; ++i) {
    const TypeSpec* param_type = src_param->parameters()[i];
    const TypeSpec* param_type_spec = param_type;
    if (i < num_type_params) {
      param_type_spec = type_spec->parameters()[i];
    }
    ASSIGN_OR_RETURN(auto new_type, RebuildType(param_type, param_type_spec));
    if (new_type != param_type) {
      needs_rebinding = true;
    }
    args.emplace_back(TypeBindingArg{new_type});
  }
  for (size_t i = num_src_params; i < num_type_params; ++i) {
    args.emplace_back(TypeBindingArg{type_spec->parameters()[i]});
  }
  if (is_function) {
    ASSIGN_OR_RETURN(auto new_type,
                     RebuildType(src_param->parameters().back(),
                                 type_spec->parameters().back()));
    if (new_type != src_param->parameters().back()) {
      needs_rebinding = true;
    }
    args.emplace_back(TypeBindingArg{new_type});
  }
  auto it = local_types_.end();
  if (!src_param->local_name().empty()) {
    it = local_types_.find(src_param->local_name());
    if (it != local_types_.end() && !needs_rebinding) {
      return it->second;
    }
  }
  if (!needs_rebinding) {
    return type_spec;
  }
  ASSIGN_OR_RETURN(auto new_allocated_type, type_spec->Bind(args),
                   _ << "Binding type dependent of changed local type names: "
                     << src_param->full_name()
                     << " binding: " << type_spec->full_name());
  if (TypeUtils::IsTupleType(*new_allocated_type)) {
    auto new_tuple_type = static_cast<TypeTuple*>(new_allocated_type.get());
    new_tuple_type->UpdateNames(type_spec);
    new_tuple_type->UpdateNames(src_param);
  }
  allocated_types.emplace_back(std::move(new_allocated_type));
  if (it != local_types_.end()) {
    it->second = allocated_types.back().get();
  }
  return allocated_types.back().get();
}

absl::StatusOr<const TypeSpec*>
LocalNamesRebinder::RebuildFunctionWithComponents(
    const TypeSpec* src_param, const std::vector<const TypeSpec*>& type_specs) {
  std::vector<TypeBindingArg> args;
  bool needs_rebinding = false;
  args.reserve(src_param->parameters().size());
  RET_CHECK(TypeUtils::IsFunctionType(*src_param))
      << " Got a: " << src_param->full_name();
  RET_CHECK(type_specs.size() == src_param->parameters().size())
      << "Invalid number of types: " << type_specs.size() << " vs. "
      << src_param->parameters().size();
  for (size_t i = 0; i < type_specs.size(); ++i) {
    const TypeSpec* param_type = CHECK_NOTNULL(src_param->parameters()[i]);
    const TypeSpec* param_type_spec = CHECK_NOTNULL(type_specs[i]);
    ASSIGN_OR_RETURN(auto new_type, RebuildType(param_type, param_type_spec),
                     _ << "Rebuilding function argument: " << i
                       << " from: " << param_type->full_name()
                       << " with: " << param_type_spec->full_name());
    if (new_type != param_type) {
      needs_rebinding = true;
    }
    args.emplace_back(TypeBindingArg{new_type});
  }
  if (!needs_rebinding) {
    return src_param;
  }
  ASSIGN_OR_RETURN(auto new_allocated_type, src_param->Bind(args),
                   _ << "Binding function type of changed local type names: "
                     << src_param->full_name());
  allocated_types.emplace_back(std::move(new_allocated_type));
  return allocated_types.back().get();
}
}  // namespace analysis
}  // namespace nudl
