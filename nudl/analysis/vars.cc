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

#include "nudl/analysis/vars.h"

#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_set.h"
#include "glog/logging.h"
#include "nudl/status/status.h"

namespace nudl {
namespace analysis {

VarBase::VarBase(absl::string_view name, const TypeSpec* type_spec,
                 absl::optional<NameStore*> parent_store)
    : WrappedNameStore(
          name, CHECK_NOTNULL(CHECK_NOTNULL(type_spec)->type_member_store())),
      original_type_(type_spec),
      type_spec_(type_spec),
      parent_store_(parent_store) {
  if (parent_store_.has_value()) {
    CHECK_NOTNULL(parent_store_.value());
  }
}

VarBase::~VarBase() {
  while (!local_fields_.empty()) {
    local_fields_.pop_back();
  }
}

const TypeSpec* VarBase::type_spec() const { return type_spec_; }

const TypeSpec* VarBase::original_type() const { return original_type_; }

const TypeSpec* VarBase::converted_type() const {
  if ((!original_type_->IsBound() ||
       original_type_->type_id() == pb::UNION_ID) &&
      (type_spec_->type_id() != pb::NULL_ID)) {
    return type_spec_;
  }
  return type_spec_;
}

absl::optional<NameStore*> VarBase::parent_store() const {
  return parent_store_;
}

const std::vector<Expression*> VarBase::assignments() const {
  return assignments_;
}

const std::vector<const TypeSpec*> VarBase::assign_types() const {
  return assign_types_;
}

absl::StatusOr<std::unique_ptr<Expression>> VarBase::Assign(
    std::unique_ptr<Expression> expression) {
  absl::Cleanup mark_failed = [this, &expression]() {
    failed_assignments_.emplace_back(std::move(expression));
  };
  ASSIGN_OR_RETURN(auto type_spec, expression->type_spec(type_spec_));
  if (!original_type_->IsAncestorOf(*type_spec)) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot assign an expression of type: " << type_spec->full_name()
           << " to " << full_name()
           << " originally declared as: " << original_type_->full_name();
  }
  if (!type_spec_->IsAncestorOf(*type_spec)) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot assign an expression of type: " << type_spec->full_name()
           << " to " << full_name()
           << " that was las assigned to: " << type_spec_->full_name();
  }
  std::move(mark_failed).Cancel();
  // TODO(catalin): wrap this in type conversion or something.
  std::unique_ptr<Expression> converted_expression(std::move(expression));
  if (((!type_spec_->IsBound() && type_spec_->type_id() != pb::FUNCTION_ID) ||
       type_spec_->type_id() == pb::UNION_ID) &&
      (type_spec->type_id() != pb::NULL_ID)) {
    type_spec_ = type_spec;
  }
  assign_types_.push_back(type_spec);
  assignments_.push_back(converted_expression.get());
  return {std::move(converted_expression)};
}

bool VarBase::IsVarKind(const NamedObject& object) {
  static const auto* const kVarKinds = new absl::flat_hash_set<pb::ObjectKind>({
      pb::ObjectKind::OBJ_FIELD,
      pb::ObjectKind::OBJ_VARIABLE,
      pb::ObjectKind::OBJ_PARAMETER,
      pb::ObjectKind::OBJ_ARGUMENT,
  });
  return kVarKinds->contains(object.kind());
}

absl::StatusOr<NamedObject*> VarBase::GetName(absl::string_view local_name,
                                              bool in_self_only) {
  auto it = local_fields_map_.find(local_name);
  if (it != local_fields_map_.end()) {
    return it->second;
  }
  ASSIGN_OR_RETURN(auto base_object,
                   WrappedNameStore::GetName(local_name, in_self_only));
  if (!IsVarKind(*(CHECK_NOTNULL(base_object)))) {
    return base_object;
  }
  auto local_var = static_cast<VarBase*>(base_object)->Clone(this);
  local_fields_map_.emplace(std::string(local_name),
                            CHECK_NOTNULL(local_var.get()));
  local_fields_.emplace_back(std::move(local_var));
  return local_fields_.back().get();
}

// Override these: we don't support - they should be added to types.
absl::Status VarBase::AddName(absl::string_view local_name,
                              NamedObject* object) {
  return status::UnimplementedErrorBuilder()
         << "Cannot add a name: " << local_name
         << " to a variable typed name object: " << full_name() << kBugNotice;
}

absl::Status VarBase::AddChildStore(absl::string_view local_name,
                                    NameStore* store) {
  return status::UnimplementedErrorBuilder()
         << "Cannot add a name: " << local_name
         << " to a variable typed name object: " << full_name() << kBugNotice;
}

VarBase* VarBase::GetRootVar() {
  VarBase* root_var = this;
  auto parent = parent_store_;
  while (parent.has_value() && IsVarKind(*parent.value())) {
    root_var = static_cast<VarBase*>(parent.value());
    parent = root_var->parent_store();
  }
  return root_var;
}

Field::Field(absl::string_view name, const TypeSpec* type_spec,
             const TypeSpec* parent_type,
             absl::optional<NameStore*> parent_store)
    : VarBase(name, type_spec, parent_store),
      parent_type_(CHECK_NOTNULL(parent_type)) {
  CHECK(parent_store.has_value());
}

pb::ObjectKind Field::kind() const { return pb::ObjectKind::OBJ_FIELD; }

std::string Field::full_name() const {
  return absl::StrCat(VarBase::full_name(), " of ", parent_type_->full_name());
}

const TypeSpec* Field::parent_type() const { return parent_type_; }

std::unique_ptr<VarBase> Field::Clone(
    absl::optional<NameStore*> parent_store) const {
  return absl::make_unique<Field>(name_, original_type_, parent_type_,
                                  parent_store);
}

Var::Var(absl::string_view name, const TypeSpec* type_spec,
         absl::optional<NameStore*> parent_store)
    : VarBase(name, type_spec, parent_store) {}

pb::ObjectKind Var::kind() const { return pb::ObjectKind::OBJ_VARIABLE; }

std::unique_ptr<VarBase> Var::Clone(
    absl::optional<NameStore*> parent_store) const {
  return absl::make_unique<Var>(name_, original_type_, parent_store);
}

Parameter::Parameter(absl::string_view name, const TypeSpec* type_spec,
                     absl::optional<NameStore*> parent_store)
    : Var(name, type_spec, parent_store) {}

pb::ObjectKind Parameter::kind() const { return pb::ObjectKind::OBJ_PARAMETER; }

std::unique_ptr<VarBase> Parameter::Clone(
    absl::optional<NameStore*> parent_store) const {
  return absl::make_unique<Parameter>(name_, original_type_, parent_store);
}

Argument::Argument(absl::string_view name, const TypeSpec* type_spec,
                   absl::optional<NameStore*> parent_store)
    : Var(name, type_spec, parent_store) {}

pb::ObjectKind Argument::kind() const { return pb::ObjectKind::OBJ_ARGUMENT; }

std::unique_ptr<VarBase> Argument::Clone(
    absl::optional<NameStore*> parent_store) const {
  return std::make_unique<Argument>(name_, original_type_, parent_store);
}

}  // namespace analysis
}  // namespace nudl
