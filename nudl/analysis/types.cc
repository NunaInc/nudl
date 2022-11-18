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

#include "nudl/analysis/types.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "glog/logging.h"
#include "nudl/analysis/type_utils.h"
#include "nudl/analysis/vars.h"
#include "nudl/status/status.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);
ABSL_FLAG(bool, nudl_non_null_for_nullable_value_default, false,
          "If true we do not emit nulls as default values of "
          "nullable type. Temporary, until we introduce inline macros");

namespace nudl {
namespace analysis {

StoredTypeSpec::StoredTypeSpec(
    TypeStore* type_store, int type_id, absl::string_view name,
    std::shared_ptr<TypeMemberStore> type_member_store, bool is_bound_type,
    const TypeSpec* ancestor, std::vector<const TypeSpec*> parameters,
    absl::optional<const TypeSpec*> original_bind)
    : TypeSpec(type_id, name, std::move(type_member_store), is_bound_type,
               ancestor, std::move(parameters), original_bind),
      type_store_(CHECK_NOTNULL(type_store)),
      object_type_spec_(
          type_id == pb::TypeId::TYPE_ID
              ? this
              : TypeUtils::EnsureType(type_store, kTypeNameType)) {}

std::unique_ptr<TypeSpec> StoredTypeSpec::Clone() const {
  return std::make_unique<StoredTypeSpec>(type_store_, type_id_, name_,
                                          type_member_store_, is_bound_type_,
                                          ancestor_, parameters_);
}

TypeStore* StoredTypeSpec::type_store() const { return type_store_; }

const TypeSpec* StoredTypeSpec::type_spec() const { return object_type_spec_; }

const ScopeName& StoredTypeSpec::scope_name() const {
  if (scope_name_.has_value()) {
    return scope_name_.value();
  }
  return type_store_->scope_name();
}

TypeType::TypeType(TypeStore* type_store,
                   std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::TYPE_ID, kTypeNameType,
                     std::move(type_member_store)) {}

std::unique_ptr<TypeSpec> TypeType::Clone() const {
  return CloneType<TypeType>();
}

TypeAny::TypeAny(TypeStore* type_store,
                 std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::ANY_ID, kTypeNameAny,
                     std::move(type_member_store)) {}
std::unique_ptr<TypeSpec> TypeAny::Clone() const {
  return CloneType<TypeAny>();
}

TypeModule::TypeModule(TypeStore* type_store, absl::string_view module_name,
                       NameStore* module)
    : StoredTypeSpec(type_store, pb::TypeId::MODULE_ID, kTypeNameModule,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)
                         ->type_member_store_ptr()),
      module_name_(module_name),
      module_(module) {
  type_member_store_ = std::make_shared<TypeMemberStore>(
      this, std::make_shared<WrappedNameStore>(module_name, module));
}

std::unique_ptr<TypeSpec> TypeModule::Clone() const {
  return std::make_unique<TypeModule>(type_store_, module_name_, module_);
}

TypeNull::TypeNull(TypeStore* type_store,
                   std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::NULL_ID, kTypeNameNull,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeNull::Clone() const {
  return CloneType<TypeNull>();
}
absl::StatusOr<pb::Expression> TypeNull::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->set_null_value(pb::NullType::NULL_VALUE);
  return expression;
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeNull::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (bindings.size() != 1) {
    return status::InvalidArgumentErrorBuilder()
           << "Binding on Null type requires 1 argument. Got: "
           << bindings.size();
  }
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false));
  if (TypeUtils::IsNullableType(*types.front())) {
    return types.front()->Clone();
  }
  return TypeUtils::EnsureType(type_store_, kTypeNameNullable)->Bind(bindings);
}

TypeNumeric::TypeNumeric(TypeStore* type_store,
                         std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::NUMERIC_ID, kTypeNameNumeric,
                     std::move(type_member_store), false,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeNumeric::Clone() const {
  return CloneType<TypeNumeric>();
}

TypeIntegral::TypeIntegral(TypeStore* type_store,
                           std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INTEGRAL_ID, kTypeNameIntegral,
                     std::move(type_member_store), false,
                     TypeUtils::EnsureType(type_store, kTypeNameNumeric)) {}
std::unique_ptr<TypeSpec> TypeIntegral::Clone() const {
  return CloneType<TypeIntegral>();
}

TypeInt::TypeInt(TypeStore* type_store,
                 std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INT_ID, kTypeNameInt,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameIntegral)) {}
std::unique_ptr<TypeSpec> TypeInt::Clone() const {
  return CloneType<TypeInt>();
}
bool TypeInt::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsIntType(type_spec);
}
absl::StatusOr<pb::Expression> TypeInt::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->set_int_value(0);
  return expression;
}

TypeInt8::TypeInt8(TypeStore* type_store,
                   std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INT8_ID, kTypeNameInt8,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameInt)) {}
std::unique_ptr<TypeSpec> TypeInt8::Clone() const {
  return CloneType<TypeInt8>();
}
bool TypeInt8::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsIntType(type_spec);
}

TypeInt16::TypeInt16(TypeStore* type_store,
                     std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INT16_ID, kTypeNameInt16,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameInt)) {}
std::unique_ptr<TypeSpec> TypeInt16::Clone() const {
  return CloneType<TypeInt16>();
}
bool TypeInt16::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsIntType(type_spec);
}

TypeInt32::TypeInt32(TypeStore* type_store,
                     std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INT32_ID, kTypeNameInt32,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameInt)) {}
std::unique_ptr<TypeSpec> TypeInt32::Clone() const {
  return CloneType<TypeInt32>();
}
bool TypeInt32::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsIntType(type_spec);
}

TypeUInt::TypeUInt(TypeStore* type_store,
                   std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::UINT_ID, kTypeNameUInt,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameIntegral)) {}
std::unique_ptr<TypeSpec> TypeUInt::Clone() const {
  return CloneType<TypeUInt>();
}
bool TypeUInt::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsUIntType(type_spec);
}
absl::StatusOr<pb::Expression> TypeUInt::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->set_uint_value(0u);
  return expression;
}

TypeUInt8::TypeUInt8(TypeStore* type_store,
                     std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::UINT8_ID, kTypeNameUInt8,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameUInt)) {}
std::unique_ptr<TypeSpec> TypeUInt8::Clone() const {
  return CloneType<TypeUInt8>();
}
bool TypeUInt8::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsUIntType(type_spec);
}

TypeUInt16::TypeUInt16(TypeStore* type_store,
                       std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::UINT16_ID, kTypeNameUInt16,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameUInt)) {}
std::unique_ptr<TypeSpec> TypeUInt16::Clone() const {
  return CloneType<TypeUInt16>();
}
bool TypeUInt16::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsUIntType(type_spec);
}

TypeUInt32::TypeUInt32(TypeStore* type_store,
                       std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::UINT32_ID, kTypeNameUInt32,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameUInt)) {}
std::unique_ptr<TypeSpec> TypeUInt32::Clone() const {
  return CloneType<TypeUInt32>();
}
bool TypeUInt32::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsUIntType(type_spec);
}

TypeFloat64::TypeFloat64(TypeStore* type_store,
                         std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::FLOAT64_ID, kTypeNameFloat64,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameNumeric)) {}
std::unique_ptr<TypeSpec> TypeFloat64::Clone() const {
  return CloneType<TypeFloat64>();
}
bool TypeFloat64::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return (TypeUtils::IsFloatType(type_spec) ||
          TypeUtils::IsIntType(type_spec) || TypeUtils::IsUIntType(type_spec));
}
absl::StatusOr<pb::Expression> TypeFloat64::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->set_double_value(0.0);
  return expression;
}

TypeFloat32::TypeFloat32(TypeStore* type_store,
                         std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::FLOAT32_ID, kTypeNameFloat32,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameFloat64)) {}
std::unique_ptr<TypeSpec> TypeFloat32::Clone() const {
  return CloneType<TypeFloat32>();
}
bool TypeFloat32::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return (TypeUtils::IsFloatType(type_spec) ||
          TypeUtils::IsIntType(type_spec) || TypeUtils::IsUIntType(type_spec));
}
absl::StatusOr<pb::Expression> TypeFloat32::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->set_float_value(0.0f);
  return expression;
}

TypeString::TypeString(TypeStore* type_store,
                       std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::STRING_ID, kTypeNameString,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeString::Clone() const {
  return CloneType<TypeString>();
}
absl::StatusOr<pb::Expression> TypeString::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->set_str_value("");
  return expression;
}

TypeBytes::TypeBytes(TypeStore* type_store,
                     std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::BYTES_ID, kTypeNameBytes,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeBytes::Clone() const {
  return CloneType<TypeBytes>();
}
absl::StatusOr<pb::Expression> TypeBytes::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->set_bytes_value("");
  return expression;
}

TypeBool::TypeBool(TypeStore* type_store,
                   std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::BOOL_ID, kTypeNameBool,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeBool::Clone() const {
  return CloneType<TypeBool>();
}
absl::StatusOr<pb::Expression> TypeBool::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->set_bool_value("");
  return expression;
}

TypeTimestamp::TypeTimestamp(TypeStore* type_store,
                             std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::TIMESTAMP_ID, kTypeNameTimestamp,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeTimestamp::Clone() const {
  return CloneType<TypeTimestamp>();
}
absl::StatusOr<pb::Expression> TypeTimestamp::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_function_call()->mutable_identifier()->add_name(
      "default_timestamp");
  return expression;
}

TypeDate::TypeDate(TypeStore* type_store,
                   std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::DATE_ID, kTypeNameDate,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameTimestamp)) {}
std::unique_ptr<TypeSpec> TypeDate::Clone() const {
  return CloneType<TypeDate>();
}
absl::StatusOr<pb::Expression> TypeDate::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_function_call()->mutable_identifier()->add_name(
      "default_date");
  return expression;
}

TypeDateTime::TypeDateTime(TypeStore* type_store,
                           std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::DATETIME_ID, kTypeNameDateTime,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameTimestamp)) {}
std::unique_ptr<TypeSpec> TypeDateTime::Clone() const {
  return CloneType<TypeDateTime>();
}
absl::StatusOr<pb::Expression> TypeDateTime::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_function_call()->mutable_identifier()->add_name(
      "default_datetime");
  return expression;
}

TypeTimeInterval::TypeTimeInterval(
    TypeStore* type_store, std::shared_ptr<TypeMemberStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::TIMEINTERVAL_ID,
                     kTypeNameTimeInterval, std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeTimeInterval::Clone() const {
  return CloneType<TypeTimeInterval>();
}
absl::StatusOr<pb::Expression> TypeTimeInterval::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_literal()->mutable_time_range()->set_seconds(0);
  return expression;
}

TypeDecimal::TypeDecimal(TypeStore* type_store,
                         std::shared_ptr<TypeMemberStore> type_member_store,
                         int precision, int scale)
    : StoredTypeSpec(type_store, pb::TypeId::DECIMAL_ID, kTypeNameDecimal,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameNumeric)),
      precision_(precision),
      scale_(scale) {}

std::unique_ptr<TypeSpec> TypeDecimal::Clone() const {
  return std::make_unique<TypeDecimal>(type_store_, type_member_store_,
                                       precision_, scale_);
}

absl::StatusOr<pb::Expression> TypeDecimal::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  expression.mutable_function_call()->mutable_identifier()->add_name(
      "default_decimal");
  return expression;
}

std::string TypeDecimal::full_name() const {
  if (precision_ > 0) {
    return absl::StrCat(name(), "<", precision_, ", ", scale_, ">");
  }
  return name();
}

pb::ExpressionTypeSpec TypeDecimal::ToProto() const {
  auto proto = TypeSpec::ToProto();
  if (absl::GetFlag(FLAGS_nudl_short_analysis_proto)) {
    return proto;
  }
  if (precision_ > 0) {
    proto.add_parameter_value(precision_);
    proto.add_parameter_value(scale_);
  }
  return proto;
}

pb::TypeSpec TypeDecimal::ToTypeSpecProto(
    const ScopeName& call_scope_name) const {
  pb::TypeSpec proto;
  proto.mutable_identifier()->add_name(name());
  if (precision_ > 0) {
    proto.add_argument()->set_int_value(precision_);
    proto.add_argument()->set_int_value(scale_);
  }
  return proto;
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeDecimal::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (precision_ > 0) {
    return status::InvalidArgumentErrorBuilder()
           << "Decimal type " << full_name() << " cannot be re-bind";
  }
  if (bindings.size() != 2 || !absl::holds_alternative<int>(bindings[0]) ||
      !absl::holds_alternative<int>(bindings[1])) {
    return absl::InvalidArgumentError(
        "Decimal type requires two integer argument to bind");
  }
  const int precision = absl::get<int>(bindings[0]);
  if (precision < 1 || precision > kMaxPrecision) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid precision for binding Decimal type: " << precision
           << " - it must be between 1 and " << kMaxPrecision;
  }
  const int scale = absl::get<int>(bindings[1]);
  if (scale < 0 || scale > precision) {
    return status::InvalidArgumentErrorBuilder()
           << "Invalid scale for binding Decimal type: " << scale
           << " - it must be between 0 and " << precision;
  }
  return std::make_unique<TypeDecimal>(type_store_, type_member_store_,
                                       precision, scale);
}

TypeIterable::TypeIterable(TypeStore* type_store,
                           std::shared_ptr<TypeMemberStore> type_member_store,
                           const TypeSpec* param)
    : StoredTypeSpec(type_store, pb::TypeId::ITERABLE_ID, kTypeNameIterable,
                     std::move(type_member_store), false,
                     TypeUtils::EnsureType(type_store, kTypeNameAny),
                     {TypeUtils::EnsureType(type_store, kTypeNameAny, param)}) {
}

std::unique_ptr<TypeSpec> TypeIterable::Clone() const {
  CHECK(!parameters_.empty());
  return std::make_unique<TypeIterable>(type_store_, type_member_store_,
                                        parameters_.front());
}

bool TypeIterable::IsIterable() const { return true; }

TypeContainer::TypeContainer(TypeStore* type_store,
                             std::shared_ptr<TypeMemberStore> type_member_store,
                             const TypeSpec* param)
    : StoredTypeSpec(type_store, pb::TypeId::CONTAINER_ID, kTypeNameContainer,
                     std::move(type_member_store), false,
                     TypeUtils::EnsureType(type_store, kTypeNameIterable),
                     {TypeUtils::EnsureType(type_store, kTypeNameAny, param)}) {
}

std::unique_ptr<TypeSpec> TypeContainer::Clone() const {
  CHECK(!parameters_.empty());
  return std::make_unique<TypeContainer>(type_store_, type_member_store_,
                                         parameters_.front());
}

TypeGenerator::TypeGenerator(TypeStore* type_store,
                             std::shared_ptr<TypeMemberStore> type_member_store,
                             const TypeSpec* param)
    : StoredTypeSpec(type_store, pb::TypeId::GENERATOR_ID, kTypeNameGenerator,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameIterable),
                     {TypeUtils::EnsureType(type_store, kTypeNameAny, param)}) {
}

std::unique_ptr<TypeSpec> TypeGenerator::Clone() const {
  CHECK(!parameters_.empty());
  return std::make_unique<TypeGenerator>(type_store_, type_member_store_,
                                         parameters_.front());
}

TypeArray::TypeArray(TypeStore* type_store,
                     std::shared_ptr<TypeMemberStore> type_member_store,
                     const TypeSpec* param)
    : StoredTypeSpec(type_store, pb::TypeId::ARRAY_ID, kTypeNameArray,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameContainer),
                     {TypeUtils::EnsureType(type_store, kTypeNameAny, param)}) {
}

const TypeSpec* TypeArray::IndexType() const {
  if (!index_type_) {
    index_type_ = TypeUtils::IntIndexType(type_store_);
  }
  return index_type_.get();
}

const TypeSpec* TypeArray::IndexedType() const {
  if (!indexed_type_) {
    indexed_type_ = TypeUtils::NullableType(type_store_, parameters_.front());
  }
  return indexed_type_.get();
}

std::unique_ptr<TypeSpec> TypeArray::Clone() const {
  CHECK(!parameters_.empty());
  return std::make_unique<TypeArray>(type_store_, type_member_store_,
                                     parameters_.front());
}
absl::StatusOr<pb::Expression> TypeArray::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  auto fun = expression.mutable_function_call();
  fun->mutable_identifier()->add_name("Array");
  ASSIGN_OR_RETURN(
      *fun->add_argument()->mutable_value(),
      parameters_.front()->DefaultValueExpression(call_scope_name));
  // expression.set_empty_struct(pb::NullType::NULL_VALUE);
  return expression;
}

TypeSet::TypeSet(TypeStore* type_store,
                 std::shared_ptr<TypeMemberStore> type_member_store,
                 const TypeSpec* param)
    : StoredTypeSpec(type_store, pb::TypeId::SET_ID, kTypeNameSet,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameContainer),
                     {TypeUtils::EnsureType(type_store, kTypeNameAny, param)}),
      bool_type_(TypeUtils::EnsureType(type_store, kTypeNameBool)) {}

std::unique_ptr<TypeSpec> TypeSet::Clone() const {
  CHECK(!parameters_.empty());
  return std::make_unique<TypeSet>(type_store_, type_member_store_,
                                   parameters_.front());
}

const TypeSpec* TypeSet::IndexType() const {
  CHECK(!parameters_.empty());
  return parameters_.front();
}

const TypeSpec* TypeSet::IndexedType() const { return bool_type_; }

absl::StatusOr<pb::Expression> TypeSet::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  auto fun = expression.mutable_function_call();
  fun->mutable_identifier()->add_name("Set");
  ASSIGN_OR_RETURN(
      *fun->add_argument()->mutable_value(),
      parameters_.front()->DefaultValueExpression(call_scope_name));
  // expression.set_empty_struct(pb::NullType::NULL_VALUE);
  return expression;
}

TypeStruct::TypeStruct(TypeStore* type_store,
                       std::shared_ptr<StructMemberStore> type_member_store,
                       absl::string_view name, std::vector<Field> fields,
                       bool is_abstract_struct)
    : StoredTypeSpec(type_store, pb::TypeId::STRUCT_ID, name,
                     CHECK_NOTNULL(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)),
      fields_(std::move(fields)),
      struct_member_store_(std::move(type_member_store)),
      is_abstract_struct_(is_abstract_struct) {
  for (auto field : fields_) {
    parameters_.push_back(CHECK_NOTNULL(field.type_spec));
  }
  struct_member_store_->set_type_spec(this);
}

absl::StatusOr<pb::Expression> TypeStruct::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  if (is_abstract_struct_) {
    return absl::UnimplementedError(
        "No default value for abstract struct type");
  }
  pb::Expression expression;
  *expression.mutable_function_call()->mutable_type_spec() =
      ToTypeSpecProto(call_scope_name);
  return expression;
}

pb::TypeSpec TypeStruct::ToTypeSpecProto(
    const ScopeName& call_scope_name) const {
  pb::TypeSpec type_spec;
  auto identifier = type_spec.mutable_identifier();
  if (call_scope_name.name() != scope_name().name()) {
    *identifier = scope_name().ToIdentifier();
  }
  if (local_name().empty()) {
    identifier->add_name(name());
  } else {
    identifier->add_name(local_name());
  }
  return type_spec;
}

pb::ExpressionTypeSpec TypeStruct::ToProto() const {
  auto proto = TypeSpec::ToProto();
  if (absl::GetFlag(FLAGS_nudl_short_analysis_proto)) {
    return proto;
  }
  for (const auto& field : fields_) {
    proto.add_parameter_name(field.name);
  }
  return proto;
}

std::string TypeStruct::TypeSignature() const {
  std::vector<std::string> comp;
  if (!scope_name().empty()) {
    if (!scope_name().module_names().empty()) {
      comp.emplace_back(absl::StrJoin(scope_name().module_names(), "_d_"));
    }
    if (!scope_name().function_names().empty()) {
      comp.emplace_back(absl::StrJoin(scope_name().module_names(), "_f_"));
    }
  }
  if (local_name().empty()) {
    comp.emplace_back(name());
  } else {
    comp.emplace_back(local_name());
  }
  return absl::StrCat("S_", absl::StrJoin(comp, "_x_"));
}

std::string TypeStruct::full_name() const {
  if (name() == kTypeNameStruct) {
    return TypeSpec::full_name();
  }
  return name();
}

absl::StatusOr<std::unique_ptr<TypeStruct>> TypeStruct::CreateTypeStruct(
    TypeStore* base_store, TypeStore* registration_store,
    absl::string_view name, std::vector<Field> fields) {
  ASSIGN_OR_RETURN(auto struct_spec,
                   base_store->FindTypeByName(kTypeNameStruct),
                   _ << "Probably a setup error: base type is not registered");
  auto struct_member_store = std::make_shared<StructMemberStore>(
      struct_spec, struct_spec->type_member_store_ptr());
  auto struct_type = absl::make_unique<TypeStruct>(
      registration_store, std::move(struct_member_store), name,
      std::move(fields), false);
  RETURN_IF_ERROR(
      struct_type->struct_member_store()->AddFields(struct_type->fields()))
      << "Adding fields to: " << struct_type->full_name();
  return {std::move(struct_type)};
}

absl::StatusOr<TypeStruct*> TypeStruct::AddTypeStruct(
    const ScopeName& scope_name, TypeStore* base_store,
    TypeStore* registration_store, absl::string_view name,
    std::vector<Field> fields) {
  ASSIGN_OR_RETURN(auto struct_type,
                   CreateTypeStruct(base_store, registration_store, name,
                                    std::move(fields)));
  auto struct_type_ptr = struct_type.get();
  RETURN_IF_ERROR(
      registration_store->DeclareType(scope_name, "", std::move(struct_type))
          .status());
  return struct_type_ptr;
}

std::unique_ptr<TypeSpec> TypeStruct::Clone() const {
  return absl::make_unique<TypeStruct>(type_store_, struct_member_store_, name_,
                                       fields_, is_abstract_struct_);
}

const std::vector<TypeStruct::Field>& TypeStruct::fields() const {
  return fields_;
}

StructMemberStore* TypeStruct::struct_member_store() const {
  return struct_member_store_.get();
}

bool TypeStruct::CheckStruct(
    const TypeSpec& type_spec,
    const std::function<bool(const TypeSpec*, const TypeSpec*)>& checker)
    const {
  if (type_spec.type_id() != type_id()) {
    return false;
  }
  auto struct_spec = static_cast<const TypeStruct*>(&type_spec);
  if (fields_.size() != struct_spec->fields_.size()) {
    return false;
  }
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (fields_[i].name != struct_spec->fields()[i].name ||
        !checker(fields_[i].type_spec, struct_spec->fields()[i].type_spec)) {
      return false;
    }
  }
  return true;
}

bool TypeStruct::IsAncestorOf(const TypeSpec& type_spec) const {
  if (fields_.empty()) {
    return type_spec.type_id() == type_id();
  }
  return CheckStruct(type_spec,
                     [](const TypeSpec* self, const TypeSpec* other) {
                       return self->IsAncestorOf(*other);
                     });
}

bool TypeStruct::IsEqual(const TypeSpec& type_spec) const {
  return CheckStruct(type_spec,
                     [](const TypeSpec* self, const TypeSpec* other) {
                       return self->IsEqual(*other);
                     });
}

bool TypeStruct::IsConvertibleFrom(const TypeSpec& type_spec) const {
  if (fields_.empty()) {
    return type_spec.type_id() == type_id();
  }
  return CheckStruct(type_spec,
                     [](const TypeSpec* self, const TypeSpec* other) {
                       return self->IsConvertibleFrom(*other);
                     });
}

bool TypeStruct::IsBound() const {
  if (fields_.empty()) {
    return false;
  }
  return StoredTypeSpec::IsBound();
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeStruct::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  std::vector<Field> new_fields;
  if (!fields_.empty()) {
    ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings));
    CHECK_EQ(types.size(), fields_.size());
    new_fields = fields_;
    for (size_t i = 0; i < new_fields.size(); ++i) {
      new_fields[i].type_spec = types[i];
    }
  } else {
    if (bindings.empty()) {
      return absl::InvalidArgumentError("Empty bindings arguments for Struct");
    }
    new_fields.reserve(bindings.size());
    ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false));
    CHECK_EQ(bindings.size(), types.size());
    for (size_t i = 0; i < bindings.size(); ++i) {
      new_fields.emplace_back(Field{absl::StrCat("field_", i), types[i]});
    }
  }
  auto struct_member_store =
      std::make_shared<StructMemberStore>(this, type_member_store_ptr());
  auto struct_type =
      absl::make_unique<TypeStruct>(type_store_, std::move(struct_member_store),
                                    name(), std::move(new_fields));
  RETURN_IF_ERROR(
      struct_type->struct_member_store()->AddFields(struct_type->fields()))
      << "Binding new structure fields for " << full_name();
  return {std::move(struct_type)};
}

StructMemberStore::StructMemberStore(const TypeSpec* type_spec,
                                     std::shared_ptr<TypeMemberStore> ancestor)
    : TypeMemberStore(type_spec, std::move(ancestor)) {
  CHECK(type_spec->type_id() == pb::TypeId::STRUCT_ID   // normally
        || type_spec->type_id() == pb::TypeId::ANY_ID)  // first build
      << "Got: " << type_spec->type_id();
}

StructMemberStore::~StructMemberStore() {
  while (!field_vars_.empty()) {
    field_vars_.pop_back();
  }
}

absl::Status StructMemberStore::AddFields(
    const std::vector<TypeStruct::Field>& fields) {
  RET_CHECK(fields_.empty())
      << "Should not add twice the fields to a struct member store";
  fields_.reserve(fields.size());
  field_vars_.reserve(fields.size());
  for (const auto& field : fields) {
    if (!NameUtil::IsValidName(field.name)) {
      return status::InvalidArgumentErrorBuilder()
             << "Invalid field name: " << field.name
             << " in type: " << type_spec()->full_name();
    }
    auto field_var = std::make_unique<Field>(
        field.name, CHECK_NOTNULL(field.type_spec), type_spec(), this);
    RETURN_IF_ERROR(AddChildStore(field.name, field_var.get()))
        << "Adding field: " << field.name
        << " to type: " << type_spec()->full_name();
    field_vars_.emplace_back(std::move(field_var));
    fields_.emplace_back(field);
  }
  return absl::OkStatus();
}

void StructMemberStore::set_type_spec(const TypeSpec* type_spec) {
  CHECK_EQ(type_spec->type_id(), pb::TypeId::STRUCT_ID);
  type_spec_ = type_spec;
  name_ = type_spec->name();
}

TypeMap::TypeMap(TypeStore* type_store,
                 std::shared_ptr<TypeMemberStore> type_member_store,
                 const TypeSpec* key, const TypeSpec* val)
    : StoredTypeSpec(type_store, pb::TypeId::MAP_ID, kTypeNameMap,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameContainer),
                     {TypeUtils::EnsureType(type_store, kTypeNameAny, key),
                      TypeUtils::EnsureType(type_store, kTypeNameAny, val)}),
      result_type_(
          std::make_unique<TypeTuple>(type_store_, nullptr, parameters_)) {}

const TypeSpec* TypeMap::IndexType() const {
  CHECK_EQ(parameters_.size(), 2ul);
  return parameters_.front();
}

const TypeSpec* TypeMap::IndexedType() const {
  CHECK_EQ(parameters_.size(), 2ul);
  if (!indexed_type_) {
    indexed_type_ = TypeUtils::NullableType(type_store_, parameters_.back());
  }
  return indexed_type_.get();
}

std::unique_ptr<TypeSpec> TypeMap::Clone() const {
  CHECK_EQ(parameters_.size(), 2ul);
  return absl::make_unique<TypeMap>(type_store_, type_member_store_,
                                    parameters_.front(), parameters_.back());
}

const TypeSpec* TypeMap::ResultType() const { return result_type_.get(); }

absl::StatusOr<pb::Expression> TypeMap::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  pb::Expression expression;
  auto fun = expression.mutable_function_call();
  fun->mutable_identifier()->add_name("Map");
  ASSIGN_OR_RETURN(
      *fun->add_argument()->mutable_value(),
      parameters_.front()->DefaultValueExpression(call_scope_name));
  ASSIGN_OR_RETURN(*fun->add_argument()->mutable_value(),
                   parameters_.back()->DefaultValueExpression(call_scope_name));
  // expression.set_empty_struct(pb::NullType::NULL_VALUE);
  return expression;
}

// TODO(catalin): at some point should probably restrict the key types.
absl::StatusOr<std::unique_ptr<TypeSpec>> TypeMap::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings));
  CHECK_EQ(types.size(), 2ul);
  auto new_map = std::make_unique<TypeMap>(type_store_, type_member_store_,
                                           types.front(), types.back());
  RETURN_IF_ERROR(new_map->UpdateBindingStore(bindings));
  return {std::move(new_map)};
}

TypeTuple::TypeTuple(TypeStore* type_store,
                     std::shared_ptr<TypeMemberStore> type_member_store,
                     std::vector<const TypeSpec*> parameters,
                     std::vector<std::string> names,
                     absl::optional<const TypeSpec*> original_bind)
    : StoredTypeSpec(type_store, pb::TypeId::TUPLE_ID, kTypeNameTuple,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny),
                     std::move(parameters), original_bind),
      names_(std::move(names)),
      is_named_(ComputeIsNamed()) {
  names_.resize(parameters_.size());
}

const std::vector<std::string>& TypeTuple::names() const { return names_; }

bool TypeTuple::ComputeIsNamed() const {
  for (const auto& name : names_) {
    if (!name.empty()) {
      return true;
    }
  }
  return false;
}

bool TypeTuple::is_named() const { return is_named_; }

std::string TypeTuple::full_name() const {
  if (!is_named_) {
    return TypeSpec::full_name();
  }
  std::string s(absl::StrCat(name(), "<"));
  CHECK_EQ(parameters_.size(), names_.size());
  for (size_t i = 0; i < parameters_.size(); ++i) {
    if (i) {
      absl::StrAppend(&s, ", ");
    }
    if (!names_[i].empty()) {
      absl::StrAppend(&s, names_[i], ": ");
    }
    absl::StrAppend(&s, parameters_[i]->full_name());
  }
  absl::StrAppend(&s, ">");
  return s;
}

bool TypeTuple::IsBound() const {
  if (parameters_.empty()) {
    return false;
  }
  return TypeSpec::IsBound();
}

const TypeSpec* TypeTuple::IndexType() const {
  if (!index_type_) {
    index_type_ = TypeUtils::IntIndexType(type_store_);
  }
  return index_type_.get();
}

std::unique_ptr<TypeSpec> TypeTuple::Clone() const {
  return std::make_unique<TypeTuple>(type_store_, type_member_store_,
                                     parameters_, names_, original_bind_);
}

bool TypeTuple::IsAncestorOf(const TypeSpec& type_spec) const {
  if (type_spec.type_id() == pb::TypeId::TUPLE_ID && parameters_.empty()) {
    return true;
  }
  return TypeSpec::IsAncestorOf(type_spec);
}

bool TypeTuple::IsConvertibleFrom(const TypeSpec& type_spec) const {
  if (type_spec.type_id() == pb::TypeId::TUPLE_ID && parameters_.empty()) {
    return true;
  }
  return TypeSpec::IsConvertibleFrom(type_spec);
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeTuple::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (bindings.empty() && parameters_.empty()) {
    return Clone();
  }
  if (!parameters_.empty() || bindings.empty()) {
    ASSIGN_OR_RETURN(auto binding, TypeSpec::Bind(bindings),
                     _ << "Binding tuple type: " << full_name());
    return {std::move(binding)};
  }
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                   _ << "Extracting types from bindings " << full_name());
  CHECK_EQ(bindings.size(), types.size());
  auto new_tuple =
      std::make_unique<TypeTuple>(type_store_, type_member_store_,
                                  std::move(types), names_, original_bind_);
  RETURN_IF_ERROR(new_tuple->UpdateBindingStore(bindings));
  return {std::move(new_tuple)};
}

void TypeTuple::UpdateNames(const TypeSpec* type_spec) {
  if (!TypeUtils::IsTupleType(*type_spec)) {
    return;
  }
  auto type_tuple = static_cast<const TypeTuple*>(type_spec);
  if (type_tuple->names().size() != names().size()) {
    return;
  }
  for (size_t i = 0; i < names_.size(); ++i) {
    if (names_[i].empty()) {
      names_[i] = type_tuple->names()[i];
    }
  }
}

TypeTupleJoin::TypeTupleJoin(TypeStore* type_store,
                             std::shared_ptr<TypeMemberStore> type_member_store,
                             std::vector<const TypeSpec*> parameters)
    : TypeTuple(type_store, std::move(type_member_store),
                std::move(parameters)) {
  name_ = kTypeNameTupleJoin;
}

std::unique_ptr<TypeSpec> TypeTupleJoin::Clone() const {
  return std::make_unique<TypeTupleJoin>(type_store_, type_member_store_ptr(),
                                         parameters_);
}

bool TypeTupleJoin::IsAncestorOf(const TypeSpec& type_spec) const {
  return (IsGeneratedByThis(type_spec) || TypeTuple::IsAncestorOf(type_spec));
}

bool TypeTupleJoin::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return (IsGeneratedByThis(type_spec) ||
          TypeTuple::IsConvertibleFrom(type_spec));
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeTupleJoin::Build(
    const std::vector<TypeBindingArg>& bindings) const {
  if (bindings.empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot build empty joined tuple";
  }
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                   _ << "Extracting types from bindings for " << full_name());
  return std::make_unique<TypeTupleJoin>(type_store_, type_member_store_,
                                         std::move(types));
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeTupleJoin::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (bindings.empty()) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot bind empty joined tuple";
  }
  std::vector<const TypeSpec*> parameter_types;
  std::vector<std::string> names;
  ASSIGN_OR_RETURN(auto types,
                   TypesFromBindings(bindings, !parameters_.empty()),
                   _ << "Extracting types from bindings for " << full_name());
  for (const TypeSpec* t : types) {
    if (!TypeUtils::IsTupleType(*t)) {
      names.emplace_back(std::string());
      parameter_types.emplace_back(t);
    } else {
      auto tuple_type = static_cast<const TypeTuple*>(t);
      RET_CHECK(tuple_type->parameters().size() == tuple_type->names().size())
          << kBugNotice;
      std::copy(tuple_type->parameters().begin(),
                tuple_type->parameters().end(),
                std::back_inserter(parameter_types));
      std::copy(tuple_type->names().begin(), tuple_type->names().end(),
                std::back_inserter(names));
    }
  }
  return std::make_unique<TypeTuple>(type_store_, type_member_store_,
                                     std::move(parameter_types),
                                     std::move(names), this);
}

TypeFunction::TypeFunction(
    TypeStore* type_store, std::shared_ptr<TypeMemberStore> type_member_store,
    absl::string_view name, std::vector<Argument> arguments,
    const TypeSpec* result, absl::optional<const TypeSpec*> original_bind,
    absl::optional<size_t> first_default_value_index,
    std::shared_ptr<absl::flat_hash_set<Function*>> function_instances)
    : StoredTypeSpec(type_store, pb::TypeId::FUNCTION_ID, name,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny), {},
                     original_bind),
      arguments_(std::move(arguments)),
      result_(result),
      first_default_value_index_(first_default_value_index),
      function_instances_(std::move(function_instances)) {
  parameters_.reserve(arguments_.size() + 1);
  for (const auto& argument : arguments_) {
    parameters_.push_back(CHECK_NOTNULL(argument.type_spec));
  }
  if (result_) {
    parameters_.push_back(result_);
  } else {
    CHECK(arguments_.empty());
  }
  if (first_default_value_index_.has_value()) {
    CHECK_LT(first_default_value_index.value(), arguments_.size());
  }
  if (!function_instances_) {
    function_instances_ = std::make_shared<absl::flat_hash_set<Function*>>();
  }
}

std::string TypeFunction::full_name() const {
  if (!result_) {
    return name();
  }
  std::string s(absl::StrCat(name(), "<"));
  absl::StrAppend(&s, result_->full_name(), "(");
  for (size_t index = 0; index < arguments_.size(); ++index) {
    if (index) {
      absl::StrAppend(&s, ", ");
    }
    absl::StrAppend(&s, arguments_[index].ToString());
    if (first_default_value_index_.has_value() &&
        index >= first_default_value_index_.value()) {
      absl::StrAppend(&s, "*");
    }
  }

  absl::StrAppend(&s, ")>");

  return wrap_local_name(std::move(s));
}

pb::ExpressionTypeSpec TypeFunction::ToProto() const {
  auto proto = TypeSpec::ToProto();
  if (absl::GetFlag(FLAGS_nudl_short_analysis_proto)) {
    return proto;
  }
  for (const auto& arg : arguments_) {
    proto.add_parameter_name(arg.name);
  }
  if (first_default_value_index_.has_value()) {
    proto.add_parameter_value(first_default_value_index_.value());
  }
  return proto;
}

const TypeSpec* TypeFunction::ResultType() const { return result_; }

const std::vector<TypeFunction::Argument>& TypeFunction::arguments() const {
  return arguments_;
}

void TypeFunction::set_argument_name(size_t index, std::string name) {
  CHECK_LT(index, arguments_.size());
  arguments_[index].name = std::move(name);
}

const absl::flat_hash_set<Function*>& TypeFunction::function_instances() const {
  return *function_instances_;
}

void TypeFunction::add_function_instance(Function* instance) {
  function_instances_->insert(instance);
}

bool TypeFunction::IsBound() const {
  if (!result_) {
    return false;
  }
  // We are bound if all arguments are bound, and if either the
  // return value is bound or is a function type.
  for (size_t i = 0; i + 1 < parameters_.size(); ++i) {
    if (!parameters_[i]->IsBound()) {
      return false;
    }
  }
  // TODO(catalin): we may need to tune this logic a bit:
  return result_->IsBound() || result_->type_id() == pb::TypeId::FUNCTION_ID;
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeFunction::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  std::vector<Argument> arguments;
  const TypeSpec* result_type = nullptr;
  absl::optional<const TypeSpec*> original_bind;
  if (result_) {
    ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings),
                     _ << "Extracting types from bindings " << full_name());
    CHECK_EQ(types.size(), arguments_.size() + 1);
    arguments = arguments_;
    for (size_t i = 0; i < arguments.size(); ++i) {
      arguments[i].type_spec = types[i];
    }
    result_type = types.back();
    original_bind = original_bind_.has_value() ? original_bind_.value() : this;
  } else {
    if (bindings.empty()) {
      return absl::InvalidArgumentError("Empty binding arguments for Function");
    }
    arguments.reserve(bindings.size() - 1);
    ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                     _ << "Extracting types from bindings " << full_name());
    CHECK_EQ(types.size(), bindings.size());
    for (size_t i = 0; i + 1 < bindings.size(); ++i) {
      arguments.push_back(Argument{absl::StrCat("arg_", i + 1), "", types[i]});
    }
    result_type = types.back();
  }
  // For functions we keep the same store:
  return {std::make_unique<TypeFunction>(type_store_, type_member_store_,
                                         name(), std::move(arguments),
                                         result_type, original_bind)};
}

absl::StatusOr<pb::Expression> TypeFunction::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  if (!result_) {
    return TypeSpec::DefaultValueExpression(call_scope_name);
  }
  pb::Expression expression;
  auto fun = expression.mutable_lambda_def();
  for (auto arg : arguments_) {
    RET_CHECK(arg.type_spec != nullptr);
    auto fparam = fun->add_param();
    fparam->set_name(arg.name);
    *fparam->mutable_type_spec() =
        arg.type_spec->ToTypeSpecProto(call_scope_name);
  }
  *fun->mutable_result_type() = result_->ToTypeSpecProto(call_scope_name);
  ASSIGN_OR_RETURN(
      *fun->mutable_expression_block()->add_expression(),
      result_->DefaultValueExpression(call_scope_name),
      _ << "Producing default result expression for: " << full_name());
  return expression;
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeFunction::BindWithFunction(
    const TypeFunction& fun) const {
  if (!fun.ResultType()) {
    return absl::InvalidArgumentError("Cannot bind abstract function type");
  }
  return BindWithComponents(fun.arguments(), fun.ResultType(),
                            fun.first_default_value_index());
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeFunction::BindWithComponents(
    const std::vector<Argument>& arguments, const TypeSpec* result_type,
    absl::optional<size_t> first_default_index) const {
  RET_CHECK(!first_default_index.has_value() ||
            first_default_index.value() < arguments.size());
  if (!result_) {
    return {std::make_unique<TypeFunction>(
        type_store_, type_member_store_, name_, arguments, result_type,
        original_bind_, first_default_index)};
  }
  if (!result_->IsConvertibleFrom(*result_type)) {
    return status::InvalidArgumentErrorBuilder()
           << "Result type: " << result_type->full_name()
           << " is not compatible with function result type "
           << result_->full_name();
  }
  if (arguments.size() < arguments_.size()) {
    return status::InvalidArgumentErrorBuilder()
           << "Not enough arguments to bind function type: " << arguments.size()
           << " provided, expecting " << arguments_.size();
  }
  if (arguments.size() > arguments_.size()) {
    if (!first_default_index.has_value()) {
      return status::InvalidArgumentErrorBuilder()
             << "Too many arguments to bind function type: " << arguments.size()
             << " provided, expecting " << arguments_.size();
    }
    if (first_default_index.value() > arguments_.size()) {
      return status::InvalidArgumentErrorBuilder()
             << "Too many arguments to bind function type: " << arguments.size()
             << " provided, and only the last "
             << (arguments.size() - first_default_index.value())
             << " have default values. Expecting " << arguments_.size()
             << " arguments";
    }
  }
  if (first_default_value_index_.has_value() &&
      (!first_default_index.has_value() ||
       (first_default_value_index_.value() < first_default_index.value()))) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot bind function with unavailable default values "
              "for arguments. Expected default values to start at index: "
           << first_default_value_index_.value() << " but "
           << (!first_default_index.has_value()
                   ? "None"
                   : absl::StrCat(first_default_index.value()))
           << " found";
  }
  std::vector<Argument> bind_args;
  bind_args.reserve(arguments_.size());
  for (size_t i = 0; i < arguments_.size(); ++i) {
    if (!CHECK_NOTNULL(arguments[i].type_spec)
             ->IsConvertibleFrom(*CHECK_NOTNULL(arguments_[i].type_spec))) {
      return status::InvalidArgumentErrorBuilder()
             << "Bind argument at index: " << i << ", "
             << arguments[i].type_spec->full_name()
             << " is not convertible from possible function argument: "
             << arguments_[i].type_spec->full_name();
    }
    bind_args.emplace_back(arguments[i]);
  }
  return {std::make_unique<TypeFunction>(
      type_store_, type_member_store_, name_, std::move(bind_args), result_type,
      original_bind_, first_default_value_index_)};
}

std::unique_ptr<TypeSpec> TypeFunction::Clone() const {
  return std::make_unique<TypeFunction>(
      type_store_, type_member_store_, name_, arguments_, result_,
      original_bind_, first_default_value_index_, function_instances_);
}

absl::optional<size_t> TypeFunction::first_default_value_index() const {
  return first_default_value_index_;
}

void TypeFunction::set_first_default_value_index(absl::optional<size_t> value) {
  if (value.has_value()) {
    CHECK_LT(value.value(), arguments_.size());
    CHECK(!first_default_value_index_.has_value());
    first_default_value_index_ = value;
  }
}

std::string TypeFunction::Argument::ToString() const {
  std::string s(absl::StrCat(name, ": "));
  if (type_name.empty()) {
    absl::StrAppend(&s, type_spec->full_name());
  } else {
    absl::StrAppend(&s, "{", type_name, " : ", type_spec->full_name(), "}");
  }
  return s;
}

namespace {
std::vector<const TypeSpec*> UnionSortTypes(
    std::vector<const TypeSpec*> parameters) {
  auto results(TypeUtils::DedupTypes(absl::Span<const TypeSpec*>(parameters)));
  std::stable_sort(results.begin(), results.end(),
                   [](const TypeSpec* a, const TypeSpec* b) {
                     if (a->type_id() == pb::TypeId::NULL_ID) {
                       return true;
                     }
                     if (b->type_id() == pb::TypeId::NULL_ID) {
                       return false;
                     }
                     return a->full_name() < b->full_name();
                   });
  return results;
}
}  // namespace

TypeUnion::TypeUnion(TypeStore* type_store,
                     std::shared_ptr<TypeMemberStore> type_member_store,
                     std::vector<const TypeSpec*> parameters)
    : StoredTypeSpec(
          type_store, pb::TypeId::UNION_ID, kTypeNameUnion,
          // TODO(catalin): there is a discussion here if we want
          // to make union base-bound.. for now we say true and
          // we see what issues we get :) else may be too restrictive.
          std::move(type_member_store), true,  // false,
          TypeUtils::EnsureType(type_store, kTypeNameAny),
          UnionSortTypes(std::move(parameters))) {}

bool TypeUnion::IsBound() const {
  return TypeSpec::IsBound();
  // Same here:
  // return parameters_.size() == 1 && parameters_.front()->IsBound();
}
std::unique_ptr<TypeSpec> TypeUnion::Clone() const {
  return std::make_unique<TypeUnion>(type_store_, type_member_store_,
                                     parameters_);
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeUnion::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (!parameters_.empty()) {
    if (bindings.size() != 1) {
      return StoredTypeSpec::Bind(bindings);
    }
    ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                     _ << "Creating bound Union from parameters");
    if (IsAncestorOf(*types.front())) {
      return types.front()->Clone();
    }
    return status::InvalidArgumentErrorBuilder()
           << "Cannot bind any of arguments of: " << full_name() << " to "
           << types.front()->full_name();
  }
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                   _ << "Creating bound Union from parameters");
  if (types.size() < 2) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot build a Union with less than two type parameters: "
           << types.size() << " vs: " << bindings.size();
  }
  // TODO(catalin): should probably sort types for binding..
  auto new_union = std::make_unique<TypeUnion>(type_store_, type_member_store_,
                                               std::move(types));
  RETURN_IF_ERROR(new_union->UpdateBindingStore(bindings));
  return {std::move(new_union)};
}

bool TypeUnion::IsAncestorOf(const TypeSpec& type_spec) const {
  if (type_spec.type_id() == pb::TypeId::UNION_ID) {
    return StoredTypeSpec::IsAncestorOf(type_spec);
  }
  for (auto parameter : parameters_) {
    if (parameter->IsAncestorOf(type_spec)) {
      return true;
    }
  }
  return false;
}

bool TypeUnion::IsConvertibleFrom(const TypeSpec& type_spec) const {
  if (type_spec.type_id() == pb::TypeId::UNION_ID) {
    return StoredTypeSpec::IsAncestorOf(type_spec);
  }
  for (auto parameter : parameters_) {
    if (parameter->IsConvertibleFrom(type_spec)) {
      return true;
    }
  }
  return false;
}

TypeNullable::TypeNullable(TypeStore* type_store,
                           std::shared_ptr<TypeMemberStore> type_member_store,
                           const TypeSpec* type_spec)
    : StoredTypeSpec(type_store, pb::TypeId::NULLABLE_ID, kTypeNameNullable,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameUnion)) {
  if (type_spec) {
    parameters_ = {TypeUtils::EnsureType(type_store, kTypeNameNull), type_spec};
  }
}

std::string TypeNullable::full_name() const {
  if (parameters_.empty()) {
    return wrap_local_name(name());
  }
  return wrap_local_name(
      absl::StrCat(name(), "<", parameters_.back()->full_name(), ">"));
}

std::string TypeNullable::TypeSignature() const {
  if (parameters_.empty()) {
    return TypeSpec::TypeSignature();
  }
  return absl::StrCat("N_", parameters_.back()->TypeSignature());
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeNullable::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                   _ << "Creating bound Nullable from parameters");
  const TypeSpec* nullable_bind = nullptr;
  if (types.size() == 2) {
    if (types.front()->type_id() == pb::TypeId::NULL_ID) {
      nullable_bind = types.back();
    } else if (types.back()->type_id() == pb::TypeId::NULL_ID) {
      nullable_bind = types.front();
    }
  } else if (types.size() == 1 &&
             types.front()->type_id() == pb::TypeId::NULLABLE_ID) {
    nullable_bind = types.front()->parameters().back();
  }
  if (types.size() != 1 && !nullable_bind) {
    return status::InvalidArgumentErrorBuilder()
           << "Nullable type requires exactly one parameter for binding. "
              " Provided: "
           << types.size();
  }
  const TypeSpec* arg_type = nullable_bind ? nullable_bind : types.front();
  if (parameters_.empty()) {
    if (arg_type->type_id() == pb::TypeId::NULL_ID) {
      return status::InvalidArgumentErrorBuilder()
             << "Cannot bind type Null as an argument to a Nullable type";
    }
    auto nullable_type = std::make_unique<TypeNullable>(
        type_store_, type_member_store_, arg_type);
    std::vector<TypeBindingArg> update_bindings;
    update_bindings.emplace_back(arg_type);
    RETURN_IF_ERROR(nullable_type->UpdateBindingStore(update_bindings));
    return {std::move(nullable_type)};
  }
  if (!IsAncestorOf(*arg_type)) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot bind type: " << full_name()
           << " to: " << arg_type->full_name();
  }
  if (!nullable_bind) {
    return arg_type->Clone();
  }
  if (nullable_bind->type_id() == pb::TypeId::NULL_ID) {
    return nullable_bind->Clone();
  }
  std::vector<TypeBindingArg> update_bindings;
  update_bindings.emplace_back(nullable_bind);
  auto nullable_type = std::make_unique<TypeNullable>(
      type_store_, type_member_store_, nullable_bind);
  RETURN_IF_ERROR(nullable_type->UpdateBindingStore(update_bindings));
  return {std::move(nullable_type)};
}

std::unique_ptr<TypeSpec> TypeNullable::Clone() const {
  return std::make_unique<TypeNullable>(
      type_store_, type_member_store_,
      parameters_.empty() ? nullptr : parameters_.back());
}

absl::StatusOr<pb::Expression> TypeNullable::DefaultValueExpression(
    const ScopeName& call_scope_name) const {
  if (!parameters_.empty() &&
      absl::GetFlag(FLAGS_nudl_non_null_for_nullable_value_default)) {
    return parameters_.back()->DefaultValueExpression(call_scope_name);
  }
  pb::Expression expression;
  expression.mutable_literal()->set_null_value(pb::NullType::NULL_VALUE);
  return expression;
}

bool TypeNullable::IsAncestorOf(const TypeSpec& type_spec) const {
  if (type_spec.type_id() == pb::TypeId::NULLABLE_ID) {
    return StoredTypeSpec::IsAncestorOf(type_spec);
  }
  if (parameters_.empty()) {
    return false;
  }
  return (parameters_.front()->IsAncestorOf(type_spec) ||
          parameters_.back()->IsAncestorOf(type_spec));
}

bool TypeNullable::IsConvertibleFrom(const TypeSpec& type_spec) const {
  if (type_spec.type_id() == pb::TypeId::NULLABLE_ID) {
    return StoredTypeSpec::IsConvertibleFrom(type_spec);
  }
  if (parameters_.empty()) {
    return false;
  }
  return (parameters_.front()->IsConvertibleFrom(type_spec) ||
          parameters_.back()->IsConvertibleFrom(type_spec));
}

TypeDataset::TypeDataset(TypeStore* type_store,
                         std::shared_ptr<TypeMemberStore> type_member_store,
                         absl::optional<const TypeSpec*> original_bind,
                         absl::string_view name, const TypeSpec* type_spec)
    : StoredTypeSpec(
          type_store, pb::TypeId::DATASET_ID,
          name.empty() ? kTypeNameDataset : name, std::move(type_member_store),
          true, TypeUtils::EnsureType(type_store, kTypeNameAny),
          {TypeUtils::EnsureType(type_store, kTypeNameAny, type_spec)},
          original_bind) {}

std::unique_ptr<TypeSpec> TypeDataset::Clone() const {
  CHECK_EQ(parameters_.size(), 1ul);
  return std::make_unique<TypeDataset>(type_store_, type_member_store_,
                                       original_bind_, name_,
                                       parameters_.back());
}

const TypeSpec* TypeDataset::ResultType() const {
  CHECK_EQ(parameters_.size(), 1ul);
  return parameters_.front();
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeDataset::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                   _ << "Creating bound Dataset from parameters");
  if (types.size() != 1) {
    return status::InvalidArgumentErrorBuilder()
           << "Expecting exactly one argument for binding: " << full_name();
  }
  if (types.front()->type_id() == pb::TypeId::FUNCTION_ID) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot bind a function to " << full_name();
  }
  if (!parameters_.front()->IsAncestorOf(*types.front())) {
    return status::InvalidArgumentErrorBuilder()
           << "Cannot bind " << types.front()->full_name() << " to "
           << full_name();
  }
  auto new_dataset = std::make_unique<TypeDataset>(
      type_store_, type_member_store_, original_bind_, name_, types.front());
  RETURN_IF_ERROR(new_dataset->UpdateBindingStore(bindings));
  return {std::move(new_dataset)};
}

namespace {
std::vector<TypeStore*>* DatasetRegistrationStack() {
  static const auto kStack = new std::vector<TypeStore*>();
  return kStack;
}
}  // namespace

// Note we operate single thread, so we are fine with this.

void TypeDataset::PushRegistrationStore(TypeStore* type_store) {
  DatasetRegistrationStack()->push_back(type_store);
}

void TypeDataset::PopRegistrationStore() {
  auto store = DatasetRegistrationStack();
  CHECK(!store->empty());
  store->pop_back();
}

TypeStore* TypeDataset::GetRegistrationStore(TypeStore* default_type_store) {
  auto store = DatasetRegistrationStack();
  if (store->empty()) {
    return default_type_store;
  }
  return store->back();
}

DatasetAggregate::DatasetAggregate(
    TypeStore* type_store, std::shared_ptr<TypeMemberStore> type_member_store,
    std::vector<const TypeSpec*> parameters)
    : TypeDataset(type_store, std::move(type_member_store), {},
                  kTypeNameDatasetAggregate) {
  parameters_ = std::move(parameters);
}

bool DatasetAggregate::IsAncestorOf(const TypeSpec& type_spec) const {
  return (IsGeneratedByThis(type_spec) || TypeDataset::IsAncestorOf(type_spec));
}

bool DatasetAggregate::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return (IsGeneratedByThis(type_spec) ||
          TypeDataset::IsConvertibleFrom(type_spec));
}

std::unique_ptr<TypeSpec> DatasetAggregate::Clone() const {
  return std::make_unique<DatasetAggregate>(type_store_, type_member_store_,
                                            parameters_);
}

absl::StatusOr<std::unique_ptr<TypeSpec>> DatasetAggregate::Build(
    const std::vector<TypeBindingArg>& bindings) const {
  if (bindings.size() != 1) {
    return status::InvalidArgumentErrorBuilder()
           << "Expecting exactly one argument to build an aggregate type";
  }
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                   _ << "Extracting types from bindings for " << full_name());
  return std::make_unique<DatasetAggregate>(type_store_, type_member_store_,
                                            std::move(types));
}

namespace {
struct NameKeeper {
  absl::StatusOr<std::string> field_name(absl::string_view name) {
    std::string result;
    ++index;
    if (name.empty() || name == "_unnamed") {
      size_t j = index;
      while (known_names.contains(absl::StrCat("arg_", j))) {
        ++j;
      }
      result = absl::StrCat("arg_", j);
    } else if (known_names.contains(name)) {
      return status::InvalidArgumentErrorBuilder()
             << "Duplicated field name found in aggregation: `" << name << "`";
    } else {
      ASSIGN_OR_RETURN(result, NameUtil::ValidatedName(std::string(name)),
                       _ << "Invalid aggregate field name: `" << name << "`");
    }
    known_names.insert(result);
    return result;
  }
  size_t index = 0;
  absl::flat_hash_set<std::string> known_names;
};
}  // namespace

absl::StatusOr<const TypeSpec*> DatasetAggregate::AggregateFieldType(
    absl::string_view aggregate_type, const TypeSpec* type_spec) const {
  // Mainly for variant 2, with functions instead of expressions:
  if (TypeUtils::IsFunctionType(*type_spec)) {
    if (!type_spec->ResultType()) {
      return status::InvalidArgumentErrorBuilder()
             << "Abstract function provided in aggregation "
                "specification: "
             << type_spec->full_name();
    }
    type_spec = type_spec->ResultType();
  }
  if (aggregate_type == "count") {
    return TypeUtils::EnsureType(type_store_, kTypeNameInt);
  }
  if (aggregate_type == "to_set") {
    ASSIGN_OR_RETURN(
        auto set_type,
        TypeUtils::EnsureType(type_store_, kTypeNameSet)->Bind({type_spec}));
    allocated_types_.emplace_back(std::move(set_type));
    return allocated_types_.back().get();
  }
  if (aggregate_type == "to_array") {
    ASSIGN_OR_RETURN(
        auto array_type,
        TypeUtils::EnsureType(type_store_, kTypeNameArray)->Bind({type_spec}));
    allocated_types_.emplace_back(std::move(array_type));
    return allocated_types_.back().get();
  }
  if (aggregate_type == "sum" || aggregate_type == "mean") {
    // This should normally be caught in .ndl file.. anyway:
    if (!TypeUtils::EnsureType(type_store_, kTypeNameNumeric)
             ->IsAncestorOf(*type_spec)) {
      return status::InvalidArgumentErrorBuilder()
             << "Aggregate type `" << aggregate_type
             << "` expects a "
                "numeric value to aggregate. Found: "
             << type_spec->full_name();
    }
  }
  return type_spec;  // whatever for now.
}

absl::StatusOr<std::unique_ptr<TypeSpec>> DatasetAggregate::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (bindings.size() != 1) {
    return status::InvalidArgumentErrorBuilder()
           << "Expecting exactly one arguments to build a dataset join type";
  }
  ASSIGN_OR_RETURN(auto types,
                   TypesFromBindings(bindings, !parameters_.empty()),
                   _ << "Extracting types from bindings for " << full_name());
  RET_CHECK(types.size() == 1);
  if (!TypeUtils::IsTupleType(*types.front()) ||
      types.front()->parameters().size() < 2) {
    return status::InvalidArgumentErrorBuilder()
           << "Type argument for building an aggregate is expected "
              "to be a tuple with two members or more. Found: "
           << types.front()->full_name();
  }

  const TypeTuple* const spec = static_cast<const TypeTuple*>(types.front());
  const TypeSpec* const base_type = spec->parameters().front();
  NameKeeper names;
  std::vector<TypeStruct::Field> struct_fields;
  for (size_t i = 1; i < spec->parameters().size(); ++i) {
    const std::string& aggregate_type = spec->names()[i];
    const TypeSpec* crt = spec->parameters()[i];
    if (!TypeUtils::IsTupleType(*crt) || crt->parameters().empty()) {
      return status::InvalidArgumentErrorBuilder()
             << "Aggregation specification is badly built at index: " << i
             << ", aggregate type: " << aggregate_type
             << ". Found: " << crt->full_name()
             << ". Bind type: " << types.front()->full_name();
    }
    auto field_spec = static_cast<const TypeTuple*>(crt);
    ASSIGN_OR_RETURN(const std::string field_name,
                     names.field_name(field_spec->names().front()),
                     _ << "In aggregation specification at index: " << i
                       << " from: " << spec->full_name());
    ASSIGN_OR_RETURN(
        const TypeSpec* field_type,
        AggregateFieldType(aggregate_type, field_spec->parameters().front()),
        _ << "Determining the field type for aggregate at index " << i
          << ", field name: " << field_name
          << " aggregate type: " << aggregate_type);
    struct_fields.emplace_back(
        TypeStruct::Field{std::move(field_name), field_type});
  }
  const std::string struct_name(
      absl::StrCat("_Aggregate_", base_type->name(), "_", NextTypeId()));
  auto registration_store = TypeDataset::GetRegistrationStore(type_store_);
  ASSIGN_OR_RETURN(
      auto struct_type,
      TypeStruct::CreateTypeStruct(type_store_, registration_store, struct_name,
                                   std::move(struct_fields)),
      _ << "Creating structure type for aggregation result");
  ASSIGN_OR_RETURN(
      auto struct_type_ptr,
      registration_store->DeclareType(registration_store->scope_name(), "",
                                      std::move(struct_type)),
      _ << "Declaring aggregation result type");
  return std::make_unique<TypeDataset>(type_store_, type_member_store_, this,
                                       absl::StrCat("_Dataset", struct_name),
                                       struct_type_ptr);
}

DatasetJoin::DatasetJoin(TypeStore* type_store,
                         std::shared_ptr<TypeMemberStore> type_member_store,
                         std::vector<const TypeSpec*> parameters)
    : TypeDataset(type_store, std::move(type_member_store), {},
                  kTypeNameDatasetJoin) {
  parameters_ = std::move(parameters);
}

bool DatasetJoin::IsAncestorOf(const TypeSpec& type_spec) const {
  return (IsGeneratedByThis(type_spec) || TypeDataset::IsAncestorOf(type_spec));
}

bool DatasetJoin::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return (IsGeneratedByThis(type_spec) ||
          TypeDataset::IsConvertibleFrom(type_spec));
}

std::unique_ptr<TypeSpec> DatasetJoin::Clone() const {
  return std::make_unique<DatasetJoin>(type_store_, type_member_store_,
                                       parameters_);
}

absl::StatusOr<std::unique_ptr<TypeSpec>> DatasetJoin::Build(
    const std::vector<TypeBindingArg>& bindings) const {
  if (bindings.size() != 3) {
    return status::InvalidArgumentErrorBuilder()
           << "Expecting exactly three argument to build an aggregate type";
  }
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                   _ << "Extracting types from bindings for " << full_name());
  return std::make_unique<DatasetJoin>(type_store_, type_member_store_,
                                       std::move(types));
}

namespace {

bool IsProperJoinType(const TypeSpec* crt) {
  if (!TypeUtils::IsTupleType(*crt) || crt->parameters().size() != 2 ||
      !TypeUtils::IsFunctionType(*crt->parameters().back())) {
    return false;
  }
  const TypeSpec* dataset = crt->parameters().front();
  if (TypeUtils::IsDatasetType(*dataset)) {
    return true;
  }
  if (TypeUtils::IsArrayType(*dataset) && dataset->ResultType() &&
      TypeUtils::IsDatasetType(*dataset->ResultType())) {
    return true;
  }
  return false;
}

struct JoinBuilder {
  explicit JoinBuilder(TypeStore* type_store) : type_store(type_store) {}

  absl::Status ProcessJoinComponent(const TypeSpec* crt,
                                    absl::string_view join_field) {
    if (!IsProperJoinType(crt)) {
      return status::InvalidArgumentErrorBuilder()
             << "Invalid tuple type argument for specification "
                "of right side of the join. We expect a tuple with "
                "a dataset or array of datasets and a key function. Got: "
             << crt->full_name();
    }
    auto crt_tuple = static_cast<const TypeTuple*>(crt);
    const TypeSpec* dtype = crt->parameters().front();
    const TypeDataset* dset_type = nullptr;
    bool is_composed_dataset = false;
    if (TypeUtils::IsDatasetType(*dtype)) {
      dset_type = static_cast<const TypeDataset*>(dtype);
    } else {
      RET_CHECK(TypeUtils::IsDatasetType(*dtype->ResultType()));
      dset_type = static_cast<const TypeDataset*>(dtype->ResultType());
      is_composed_dataset = true;
    }
    if (dset_type->parameters().empty() ||
        !TypeUtils::IsStructType(*dset_type->parameters().front())) {
      return status::InvalidArgumentErrorBuilder()
             << "Join dataset inner type not specified or not a structure: "
             << dset_type->full_name();
    }
    return ProcessRight(
        crt_tuple->names().front(), join_field,
        static_cast<const TypeStruct*>(dset_type->parameters().front()),
        crt->parameters().back(), is_composed_dataset);
  }
  absl::Status ProcessLeft(const TypeSpec* arg, const TypeSpec* key) {
    RET_CHECK(struct_fields.empty()) << "Multiple ProcessLeft calls.";
    if (!TypeUtils::IsStructType(*arg)) {
      return status::InvalidArgumentErrorBuilder()
             << "Expecting a dataset type binded to a struct as first "
                "join argument. Got: "
             << arg->full_name();
    }
    if (!TypeUtils::IsFunctionType(*key) || !key->ResultType()) {
      return status::InvalidArgumentErrorBuilder()
             << "Expecting a valid function type as the second argument in the "
                "join specification. Got: "
             << key->full_name();
    }
    auto arg_struct = static_cast<const TypeStruct*>(arg);
    for (const auto& field : arg_struct->fields()) {
      RETURN_IF_ERROR(name_keeper.field_name(field.name).status())
          << "For field in the left join structure: " << arg->full_name();
    }
    std::copy(arg_struct->fields().begin(), arg_struct->fields().end(),
              std::back_inserter(struct_fields));
    left_type = arg_struct;
    key_type = key->ResultType();
    return absl::OkStatus();
  }

  absl::Status ProcessRight(absl::string_view join_name,
                            absl::string_view join_field, const TypeStruct* arg,
                            const TypeSpec* key, bool is_composed_dataset) {
    RET_CHECK(left_type.has_value()) << "Need to call ProcessLeft first";
    RET_CHECK(key_type.has_value()) << "Need to call ProcessLeft first";
    if (!TypeUtils::IsFunctionType(*key) || !key->ResultType()) {
      return status::InvalidArgumentErrorBuilder()
             << "Expecting a valid function type as the second argument in the "
                "join specification. Got: "
             << key->full_name();
    }
    if (!key->ResultType()->IsEqual(*key_type.value())) {
      return status::InvalidArgumentErrorBuilder()
             << "Right side expression of a join differs from what was "
                "presented on the left side. Found: "
             << key->ResultType()->full_name()
             << " expecting: " << key_type.value()->full_name();
    }
    if (is_composed_dataset && join_name != "right_multi_array") {
      return status::InvalidArgumentErrorBuilder()
             << "Invalid join name: " << join_name
             << " for joining with "
                "dataset array.";
    }
    ASSIGN_OR_RETURN(const std::string field_name,
                     name_keeper.field_name(join_field),
                     _ << "For right join specification: " << key->full_name());
    std::unique_ptr<TypeSpec> join_type;
    if (join_name == "right") {
      ASSIGN_OR_RETURN(
          join_type,
          TypeUtils::EnsureType(type_store, kTypeNameNullable)->Bind({arg}),
          _ << "Building an array type for the multi right join field");
    } else if (join_name == "right_multi" || join_name == "right_multi_array") {
      ASSIGN_OR_RETURN(
          join_type,
          TypeUtils::EnsureType(type_store, kTypeNameArray)->Bind({arg}),
          _ << "Building an array type for the multi right join field");
    } else {
      return status::InvalidArgumentErrorBuilder()
             << "Invalid join name specification: " << join_name;
    }
    struct_fields.emplace_back(TypeStruct::Field{field_name, join_type.get()});
    allocated_types.emplace_back(std::move(join_type));
    if (is_composed_dataset) {
      ASSIGN_OR_RETURN(
          std::string index_field_name,
          name_keeper.field_name(absl::StrCat(field_name, "_index")),
          _ << "Adding an index field name to array-based "
               "join specification: "
            << key->full_name());
      ASSIGN_OR_RETURN(
          auto join_index_type,
          TypeUtils::EnsureType(type_store, kTypeNameArray)
              ->Bind({TypeUtils::EnsureType(type_store, kTypeNameInt)}),
          _ << "Building an array type for the multi right join index field");
      struct_fields.emplace_back(TypeStruct::Field{std::move(index_field_name),
                                                   join_index_type.get()});
      allocated_types.emplace_back(std::move(join_index_type));
    }
    return absl::OkStatus();
  }
  absl::StatusOr<const TypeSpec*> BuildResult(size_t type_id) {
    if (!left_type.has_value()) {
      return status::InvalidArgumentErrorBuilder()
             << "No left structure to join with was specified";
    }
    const std::string struct_name(
        absl::StrCat("_Join_", left_type.value()->name(), "_", type_id));
    auto registration_store = TypeDataset::GetRegistrationStore(type_store);
    ASSIGN_OR_RETURN(
        auto struct_type,
        TypeStruct::CreateTypeStruct(type_store, registration_store,
                                     struct_name, std::move(struct_fields)),
        _ << "Creating structure type for join result");
    return registration_store->DeclareType(registration_store->scope_name(), "",
                                           std::move(struct_type));
  }

  TypeStore* const type_store;
  absl::optional<const TypeSpec*> left_type;
  absl::optional<const TypeSpec*> key_type;
  std::vector<TypeStruct::Field> struct_fields;
  std::vector<std::unique_ptr<TypeSpec>> allocated_types;
  NameKeeper name_keeper;
};
}  // namespace

absl::StatusOr<std::unique_ptr<TypeSpec>> DatasetJoin::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (bindings.size() != 3) {
    return status::InvalidArgumentErrorBuilder()
           << "Expecting exactly three arguments to building an aggregate type";
  }
  ASSIGN_OR_RETURN(auto types,
                   TypesFromBindings(bindings, !parameters_.empty()),
                   _ << "Extracting types from bindings for " << full_name());
  RET_CHECK(types.size() == 3);
  if (!TypeUtils::IsTupleType(*types.back())) {
    return status::InvalidArgumentErrorBuilder()
           << "Expecting the third type argument for building a join "
              " to be a tuple. Got: "
           << types.back()->full_name();
  }
  JoinBuilder builder(type_store_);
  absl::Cleanup mark_error = [this, &builder]() {
    std::move(builder.allocated_types.begin(), builder.allocated_types.end(),
              std::back_inserter(allocated_types_));
  };
  RETURN_IF_ERROR(builder.ProcessLeft(types[0], types[1]));

  const TypeTuple* const spec = static_cast<const TypeTuple*>(types.back());
  for (size_t i = 0; i < spec->parameters().size(); ++i) {
    RETURN_IF_ERROR(
        builder.ProcessJoinComponent(spec->parameters()[i], spec->names()[i]))
        << "Processing right join specification at index: " << i;
  }
  ASSIGN_OR_RETURN(auto struct_type, builder.BuildResult(NextTypeId()),
                   _ << "Building join result type");
  return std::make_unique<TypeDataset>(
      type_store_, type_member_store_, this,
      absl::StrCat("_Dataset", struct_type->name()), struct_type);
}

std::unique_ptr<TypeSpec> TypeUnknown::Clone() const {
  return std::make_unique<TypeUnknown>(type_member_store_ptr());
}

TypeUnknown::TypeUnknown(std::shared_ptr<TypeMemberStore> type_member_store)
    : TypeSpec(pb::TypeId::UNKNOWN_ID, kTypeNameUnknown,
               std::move(type_member_store)) {}

const TypeUnknown* TypeUnknown::Instance() {
  static const TypeUnknown* const kUnknownType = new TypeUnknown(nullptr);
  return kUnknownType;
}

const TypeSpec* TypeUnknown::type_spec() const { return this; }

}  // namespace analysis
}  // namespace nudl
