#include "nudl/analysis/type_spec.h"

#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "glog/logging.h"
#include "nudl/analysis/names.h"
#include "nudl/proto/analysis.pb.h"
#include "nudl/status/status.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);

namespace nudl {
namespace analysis {

TypeMemberStore::TypeMemberStore(const TypeSpec* spec,
                                 std::shared_ptr<NameStore> ancestor)
    : BaseNameStore(CHECK_NOTNULL(spec)->name()),
      type_spec_(spec),
      ancestor_(std::move(ancestor)) {}

const TypeSpec* TypeMemberStore::type_spec() const { return type_spec_; }

NameStore* TypeMemberStore::ancestor() const { return ancestor_.get(); }

pb::ObjectKind TypeMemberStore::kind() const {
  return pb::ObjectKind::OBJ_TYPE;
}

std::string TypeMemberStore::full_name() const {
  return absl::StrCat("Members of ", type_spec_->full_name());
}

absl::Status TypeMemberStore::CheckAddedObject(absl::string_view local_name,
                                               NamedObject* object) {
  pb::ObjectKind kind = CHECK_NOTNULL(object)->kind();
  if (kind != pb::ObjectKind::OBJ_FIELD && kind != pb::ObjectKind::OBJ_METHOD) {
    return status::InvalidArgumentErrorBuilder()
           << "Type members can only be fields or method. "
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

bool TypeMemberStore::HasName(absl::string_view local_name) const {
  if (BaseNameStore::HasName(local_name)) {
    return true;
  }
  if (ancestor_) {
    return ancestor_->HasName(local_name);
  }
  return false;
}

absl::StatusOr<NamedObject*> TypeMemberStore::GetName(
    absl::string_view local_name) {
  auto result = BaseNameStore::GetName(local_name);
  if (result.ok() || !ancestor_ || !ancestor_->HasName(local_name)) {
    return result;
  }
  return ancestor_->GetName(local_name);
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

size_t TypeSpec::NextTypeId() {
  static std::atomic<size_t> next_id{pb::TypeId::FIRST_CUSTOM_ID};
  return next_id.fetch_add(std::memory_order_relaxed);
}

TypeSpec::TypeSpec(pb::TypeId type_id, absl::string_view name,
                   std::shared_ptr<NameStore> type_member_store,
                   bool is_bound_type, const TypeSpec* ancestor,
                   std::vector<const TypeSpec*> parameters)
    : NamedObject(name),
      type_id_(type_id),
      type_member_store_(std::move(type_member_store)),
      is_bound_type_(is_bound_type),
      ancestor_(ancestor),
      parameters_(std::move(parameters)) {
  if (!type_member_store_) {
    type_member_store_ = std::make_shared<TypeMemberStore>(
        this, ancestor ? ancestor->type_member_store_ptr() : nullptr);
  }
}

pb::TypeId TypeSpec::type_id() const { return type_id_; }

bool TypeSpec::is_bound_type() const { return is_bound_type_; }

const TypeSpec* TypeSpec::ancestor() const { return ancestor_; }

const std::vector<const TypeSpec*>& TypeSpec::parameters() const {
  return parameters_;
}

NameStore* TypeSpec::type_member_store() const {
  return type_member_store_.get();
}

std::shared_ptr<NameStore> TypeSpec::type_member_store_ptr() const {
  return type_member_store_;
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
  proto.set_type_id(type_id());
  proto.set_name(name());
  for (auto param : parameters_) {
    *proto.add_parameter() = param->ToProto();
  }
  return proto;
}

const std::string& TypeSpec::local_name() const { return local_name_; }

void TypeSpec::set_local_name(absl::string_view local_name) {
  local_name_ = std::string(local_name);
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
  return {std::move(result)};
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
             << "Expecting an ancestor of type: " << parameters_[i]->full_name()
             << " for binding parameter " << i << " of type " << full_name()
             << ". Got: " << types.back()->full_name();
    }
  }
  if (check_params) {
    if (minimum_parameters.has_value()) {
      if (types.size() < minimum_parameters.value()) {
        return status::InvalidArgumentErrorBuilder()
               << "Expecting at least " << minimum_parameters.value()
               << " arguments "
                  " and at most: "
               << parameters_.size() << " for binding type " << full_name()
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
  return absl::OkStatus();
}

bool TypeSpec::IsBasicTypeId(pb::TypeId type_id) {
  static auto kBasicTypeIds = new absl::flat_hash_set<pb::TypeId>({
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
  } else if (!it->second->IsEqual(*type_spec)) {
    // TODO(catalin): tune the logic a bit here with tests.
    if (type_spec->IsBound() &&
        (!it->second->IsBound() || it->second->IsAncestorOf(*type_spec))) {
      it->second = type_spec;
    }
  } else if (!it->second->IsConvertibleFrom(*type_spec)) {
    return status::InvalidArgumentErrorBuilder()
           << "Named type: " << it->first
           << " is bound to "
              "two incompatible argument types: "
           << it->second->full_name() << " and " << type_spec->full_name();
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
  if (src_param->type_id() == pb::TypeId::FUNCTION_ID) {
    if (type_spec->type_id() != pb::TypeId::FUNCTION_ID ||
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
  if (src_param->type_id() == pb::TypeId::FUNCTION_ID) {
    if (type_spec->type_id() != pb::TypeId::FUNCTION_ID ||
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
  RET_CHECK(src_param->type_id() == pb::TypeId::FUNCTION_ID)
      << " Got a: " << src_param->full_name();
  RET_CHECK(type_specs.size() == src_param->parameters().size())
      << "Invalid number of types: " << type_specs.size() << " vs. "
      << src_param->parameters().size();
  for (size_t i = 0; i < type_specs.size(); ++i) {
    const TypeSpec* param_type = src_param->parameters()[i];
    const TypeSpec* param_type_spec = type_specs[i];
    ASSIGN_OR_RETURN(auto new_type, RebuildType(param_type, param_type_spec));
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

const absl::flat_hash_map<std::string, const TypeSpec*>&
LocalNamesRebinder::local_types() const {
  return local_types_;
}
}  // namespace analysis
}  // namespace nudl
