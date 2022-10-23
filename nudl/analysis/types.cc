#include "nudl/analysis/types.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "glog/logging.h"
#include "nudl/analysis/vars.h"
#include "nudl/status/status.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);

namespace nudl {
namespace analysis {

const TypeSpec* TypeUtils::EnsureType(TypeStore* type_store,
                                      absl::string_view name,
                                      const TypeSpec* spec) {
  if (spec) {
    return spec;
  }
  auto result = type_store->FindTypeByName(name);
  CHECK_OK(result.status());
  return result.value();
}

absl::string_view TypeUtils::BaseTypeName(pb::TypeId type_id) {
  static const auto* const kTypeNames =
      new absl::flat_hash_map<pb::TypeId, std::string>({
          {pb::TypeId::ANY_ID, std::string(kTypeNameAny)},
          {pb::TypeId::NULL_ID, std::string(kTypeNameNull)},
          {pb::TypeId::NUMERIC_ID, std::string(kTypeNameNumeric)},
          {pb::TypeId::INT_ID, std::string(kTypeNameInt)},
          {pb::TypeId::INT8_ID, std::string(kTypeNameInt8)},
          {pb::TypeId::INT16_ID, std::string(kTypeNameInt16)},
          {pb::TypeId::INT32_ID, std::string(kTypeNameInt32)},
          {pb::TypeId::UINT_ID, std::string(kTypeNameUInt)},
          {pb::TypeId::UINT8_ID, std::string(kTypeNameUInt8)},
          {pb::TypeId::UINT16_ID, std::string(kTypeNameUInt16)},
          {pb::TypeId::UINT32_ID, std::string(kTypeNameUInt32)},
          {pb::TypeId::STRING_ID, std::string(kTypeNameString)},
          {pb::TypeId::BYTES_ID, std::string(kTypeNameBytes)},
          {pb::TypeId::BOOL_ID, std::string(kTypeNameBool)},
          {pb::TypeId::FLOAT32_ID, std::string(kTypeNameFloat32)},
          {pb::TypeId::FLOAT64_ID, std::string(kTypeNameFloat64)},
          {pb::TypeId::DATE_ID, std::string(kTypeNameDate)},
          {pb::TypeId::DATETIME_ID, std::string(kTypeNameDateTime)},
          {pb::TypeId::TIMEINTERVAL_ID, std::string(kTypeNameTimeInterval)},
          {pb::TypeId::TIMESTAMP_ID, std::string(kTypeNameTimestamp)},
          {pb::TypeId::DECIMAL_ID, std::string(kTypeNameDecimal)},
          {pb::TypeId::ITERABLE_ID, std::string(kTypeNameIterable)},
          {pb::TypeId::ARRAY_ID, std::string(kTypeNameArray)},
          {pb::TypeId::TUPLE_ID, std::string(kTypeNameTuple)},
          {pb::TypeId::SET_ID, std::string(kTypeNameSet)},
          {pb::TypeId::MAP_ID, std::string(kTypeNameMap)},
          {pb::TypeId::STRUCT_ID, std::string(kTypeNameStruct)},
          {pb::TypeId::FUNCTION_ID, std::string(kTypeNameFunction)},
          {pb::TypeId::UNION_ID, std::string(kTypeNameUnion)},
          {pb::TypeId::NULLABLE_ID, std::string(kTypeNameNullable)},
          {pb::TypeId::DATASET_ID, std::string(kTypeNameDataset)},
          {pb::TypeId::TYPE_ID, std::string(kTypeNameType)},
          {pb::TypeId::MODULE_ID, std::string(kTypeNameModule)},
      });
  auto it = kTypeNames->find(type_id);
  if (it == kTypeNames->end()) {
    return kTypeNameUnknown;
  }
  return it->second;
}

bool TypeUtils::IsUIntType(pb::TypeId type_id) {
  return (type_id == pb::TypeId::UINT_ID || type_id == pb::TypeId::UINT8_ID ||
          type_id == pb::TypeId::UINT16_ID || type_id == pb::TypeId::UINT32_ID);
}

bool TypeUtils::IsIntType(pb::TypeId type_id) {
  return (type_id == pb::TypeId::INT_ID || type_id == pb::TypeId::INT8_ID ||
          type_id == pb::TypeId::INT16_ID || type_id == pb::TypeId::INT32_ID);
}

bool TypeUtils::IsFloatType(pb::TypeId type_id) {
  return (type_id == pb::TypeId::FLOAT32_ID ||
          type_id == pb::TypeId::FLOAT64_ID);
}

std::unique_ptr<TypeSpec> TypeUtils::IntIndexType(TypeStore* type_store) {
  auto index_type =
      TypeUtils::EnsureType(type_store, kTypeNameUnion)
          ->Bind(
              {TypeBindingArg{TypeUtils::EnsureType(type_store, kTypeNameInt)},
               TypeBindingArg{
                   TypeUtils::EnsureType(type_store, kTypeNameUInt)}});
  CHECK_OK(index_type.status());
  return std::move(index_type).value();
}

std::unique_ptr<TypeSpec> TypeUtils::NullableType(TypeStore* type_store,
                                                  const TypeSpec* type_spec) {
  if (type_spec->type_id() == pb::TypeId::NULLABLE_ID ||
      type_spec->type_id() == pb::TypeId::NULL_ID) {
    return type_spec->Clone();
  }
  auto nullable_type = TypeUtils::EnsureType(type_store, kTypeNameNullable)
                           ->Bind({TypeBindingArg{type_spec}});
  CHECK_OK(nullable_type.status());
  return std::move(nullable_type).value();
}

std::vector<const TypeSpec*> TypeUtils::DedupTypes(
    absl::Span<const TypeSpec*> parameters) {
  std::vector<const TypeSpec*> results;
  results.reserve(parameters.size());
  for (auto param : parameters) {
    bool skip = false;
    for (auto result : results) {
      if (param->IsEqual(*result)) {
        skip = true;
        break;
      }
    }
    if (skip) {
      continue;
    }
    results.emplace_back(param);
  }
  return results;
}

void TypeUtils::FindUnboundTypes(const TypeSpec* type_spec,
                                 absl::flat_hash_set<std::string>* type_names) {
  if (!type_spec->is_bound_type()) {
    type_names->emplace(type_spec->name());
  }
  for (auto param : type_spec->parameters()) {
    if (!type_names->contains(param->name())) {
      FindUnboundTypes(param, type_names);
    }
  }
}

void BaseTypesStore::CreateBaseTypes() {
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeType>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeAny>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeNull>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeUnion>(this, nullptr))
          .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeNullable>(this, nullptr))
               .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeNumeric>(this, nullptr))
               .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeInt>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeInt8>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeInt16>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeInt32>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeUInt>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeUInt8>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeUInt16>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeUInt32>(this, nullptr))
          .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeFloat64>(this, nullptr))
               .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeFloat32>(this, nullptr))
               .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeString>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeBytes>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeBool>(this, nullptr))
          .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeTimestamp>(this, nullptr))
               .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeDate>(this, nullptr))
          .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeDateTime>(this, nullptr))
               .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeTimeInterval>(this, nullptr))
               .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeDecimal>(this, nullptr))
               .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeIterable>(this, nullptr))
               .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeArray>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeSet>(this, nullptr))
          .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeTuple>(this, nullptr))
          .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeStruct>(
                           this, std::make_shared<StructMemberStore>(
                                     TypeUtils::EnsureType(this, kTypeNameAny),
                                     nullptr)))
               .status());
  CHECK_OK(
      DeclareType(*scope_name_, "", std::make_unique<TypeMap>(this, nullptr))
          .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeFunction>(this, nullptr))
               .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeDataset>(this, nullptr))
               .status());
}

StoredTypeSpec::StoredTypeSpec(TypeStore* type_store, pb::TypeId type_id,
                               absl::string_view name,
                               std::shared_ptr<NameStore> type_member_store,
                               bool is_bound_type, const TypeSpec* ancestor,
                               std::vector<const TypeSpec*> parameters)
    : TypeSpec(type_id, name, std::move(type_member_store), is_bound_type,
               ancestor, std::move(parameters)),
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

const TypeSpec* StoredTypeSpec::type_spec() const { return object_type_spec_; }

const ScopeName& StoredTypeSpec::scope_name() const {
  if (scope_name_.has_value()) {
    return scope_name_.value();
  }
  return type_store_->scope_name();
}

void StoredTypeSpec::set_scope_name(ScopeName scope_name) {
  scope_name_ = std::move(scope_name);
}

TypeType::TypeType(TypeStore* type_store,
                   std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::TYPE_ID, kTypeNameType,
                     std::move(type_member_store)) {}

std::unique_ptr<TypeSpec> TypeType::Clone() const {
  return CloneType<TypeType>();
}

TypeAny::TypeAny(TypeStore* type_store,
                 std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::ANY_ID, kTypeNameAny,
                     std::move(type_member_store)) {}
std::unique_ptr<TypeSpec> TypeAny::Clone() const {
  return CloneType<TypeAny>();
}

TypeModule::TypeModule(TypeStore* type_store,
                       std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::MODULE_ID, kTypeNameModule,
                     std::move(type_member_store)) {}

std::unique_ptr<TypeSpec> TypeModule::Clone() const {
  return CloneType<TypeModule>();
}

TypeNull::TypeNull(TypeStore* type_store,
                   std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::NULL_ID, kTypeNameNull,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeNull::Clone() const {
  return CloneType<TypeNull>();
}

TypeNumeric::TypeNumeric(TypeStore* type_store,
                         std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::NUMERIC_ID, kTypeNameNumeric,
                     std::move(type_member_store), false,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeNumeric::Clone() const {
  return CloneType<TypeNumeric>();
}

TypeInt::TypeInt(TypeStore* type_store,
                 std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INT_ID, kTypeNameInt,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameNumeric)) {}
std::unique_ptr<TypeSpec> TypeInt::Clone() const {
  return CloneType<TypeInt>();
}
bool TypeInt::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsIntType(type_spec.type_id());
}

TypeInt8::TypeInt8(TypeStore* type_store,
                   std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INT8_ID, kTypeNameInt8,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameInt)) {}
std::unique_ptr<TypeSpec> TypeInt8::Clone() const {
  return CloneType<TypeInt8>();
}
bool TypeInt8::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsIntType(type_spec.type_id());
}

TypeInt16::TypeInt16(TypeStore* type_store,
                     std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INT16_ID, kTypeNameInt16,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameInt)) {}
std::unique_ptr<TypeSpec> TypeInt16::Clone() const {
  return CloneType<TypeInt16>();
}
bool TypeInt16::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsIntType(type_spec.type_id());
}

TypeInt32::TypeInt32(TypeStore* type_store,
                     std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::INT32_ID, kTypeNameInt32,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameInt)) {}
std::unique_ptr<TypeSpec> TypeInt32::Clone() const {
  return CloneType<TypeInt32>();
}
bool TypeInt32::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsIntType(type_spec.type_id());
}

TypeUInt::TypeUInt(TypeStore* type_store,
                   std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::UINT_ID, kTypeNameUInt,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameNumeric)) {}
std::unique_ptr<TypeSpec> TypeUInt::Clone() const {
  return CloneType<TypeUInt>();
}
bool TypeUInt::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsUIntType(type_spec.type_id());
}

TypeUInt8::TypeUInt8(TypeStore* type_store,
                     std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::UINT8_ID, kTypeNameUInt8,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameUInt)) {}
std::unique_ptr<TypeSpec> TypeUInt8::Clone() const {
  return CloneType<TypeUInt8>();
}
bool TypeUInt8::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsUIntType(type_spec.type_id());
}

TypeUInt16::TypeUInt16(TypeStore* type_store,
                       std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::UINT16_ID, kTypeNameUInt16,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameUInt)) {}
std::unique_ptr<TypeSpec> TypeUInt16::Clone() const {
  return CloneType<TypeUInt16>();
}
bool TypeUInt16::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsUIntType(type_spec.type_id());
}

TypeUInt32::TypeUInt32(TypeStore* type_store,
                       std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::UINT32_ID, kTypeNameUInt32,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameUInt)) {}
std::unique_ptr<TypeSpec> TypeUInt32::Clone() const {
  return CloneType<TypeUInt32>();
}
bool TypeUInt32::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return TypeUtils::IsUIntType(type_spec.type_id());
}

TypeFloat64::TypeFloat64(TypeStore* type_store,
                         std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::FLOAT64_ID, kTypeNameFloat64,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameNumeric)) {}
std::unique_ptr<TypeSpec> TypeFloat64::Clone() const {
  return CloneType<TypeFloat64>();
}
bool TypeFloat64::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return (TypeUtils::IsFloatType(type_spec.type_id()) ||
          TypeUtils::IsIntType(type_spec.type_id()) ||
          TypeUtils::IsUIntType(type_spec.type_id()));
}

TypeFloat32::TypeFloat32(TypeStore* type_store,
                         std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::FLOAT32_ID, kTypeNameFloat32,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameFloat64)) {}
std::unique_ptr<TypeSpec> TypeFloat32::Clone() const {
  return CloneType<TypeFloat32>();
}
bool TypeFloat32::IsConvertibleFrom(const TypeSpec& type_spec) const {
  return (TypeUtils::IsFloatType(type_spec.type_id()) ||
          TypeUtils::IsIntType(type_spec.type_id()) ||
          TypeUtils::IsUIntType(type_spec.type_id()));
}

TypeString::TypeString(TypeStore* type_store,
                       std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::STRING_ID, kTypeNameString,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeString::Clone() const {
  return CloneType<TypeString>();
}

TypeBytes::TypeBytes(TypeStore* type_store,
                     std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::BYTES_ID, kTypeNameBytes,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeBytes::Clone() const {
  return CloneType<TypeBytes>();
}

TypeBool::TypeBool(TypeStore* type_store,
                   std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::BOOL_ID, kTypeNameBool,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeBool::Clone() const {
  return CloneType<TypeBool>();
}

TypeTimestamp::TypeTimestamp(TypeStore* type_store,
                             std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::TIMESTAMP_ID, kTypeNameTimestamp,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeTimestamp::Clone() const {
  return CloneType<TypeTimestamp>();
}

TypeDate::TypeDate(TypeStore* type_store,
                   std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::DATE_ID, kTypeNameDate,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameTimestamp)) {}
std::unique_ptr<TypeSpec> TypeDate::Clone() const {
  return CloneType<TypeDate>();
}

TypeDateTime::TypeDateTime(TypeStore* type_store,
                           std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::DATETIME_ID, kTypeNameDateTime,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameTimestamp)) {}
std::unique_ptr<TypeSpec> TypeDateTime::Clone() const {
  return CloneType<TypeDateTime>();
}

TypeTimeInterval::TypeTimeInterval(TypeStore* type_store,
                                   std::shared_ptr<NameStore> type_member_store)
    : StoredTypeSpec(type_store, pb::TypeId::TIMEINTERVAL_ID,
                     kTypeNameTimeInterval, std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)) {}
std::unique_ptr<TypeSpec> TypeTimeInterval::Clone() const {
  return CloneType<TypeTimeInterval>();
}

TypeDecimal::TypeDecimal(TypeStore* type_store,
                         std::shared_ptr<NameStore> type_member_store,
                         int precision, int scale)
    : StoredTypeSpec(type_store, pb::TypeId::DECIMAL_ID, kTypeNameDecimal,
                     std::move(type_member_store), precision > 0 && scale >= 0,
                     TypeUtils::EnsureType(type_store, kTypeNameNumeric)),
      precision_(precision),
      scale_(scale) {}

std::unique_ptr<TypeSpec> TypeDecimal::Clone() const {
  return std::make_unique<TypeDecimal>(type_store_, type_member_store_,
                                       precision_, scale_);
}

std::string TypeDecimal::full_name() const {
  if (IsBound()) {
    return absl::StrCat(name(), "<", precision_, ", ", scale_, ">");
  }
  return name();
}

pb::ExpressionTypeSpec TypeDecimal::ToProto() const {
  auto proto = TypeSpec::ToProto();
  if (absl::GetFlag(FLAGS_nudl_short_analysis_proto)) {
    return proto;
  }
  proto.add_parameter_value(precision_);
  proto.add_parameter_value(scale_);
  return proto;
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeDecimal::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (IsBound()) {
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
                           std::shared_ptr<NameStore> type_member_store,
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

TypeArray::TypeArray(TypeStore* type_store,
                     std::shared_ptr<NameStore> type_member_store,
                     const TypeSpec* param)
    : StoredTypeSpec(type_store, pb::TypeId::ARRAY_ID, kTypeNameArray,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameIterable),
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

TypeSet::TypeSet(TypeStore* type_store,
                 std::shared_ptr<NameStore> type_member_store,
                 const TypeSpec* param)
    : StoredTypeSpec(type_store, pb::TypeId::SET_ID, kTypeNameSet,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameIterable),
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

TypeStruct::TypeStruct(TypeStore* type_store,
                       std::shared_ptr<StructMemberStore> type_member_store,
                       absl::string_view name, std::vector<Field> fields)
    : StoredTypeSpec(type_store, pb::TypeId::STRUCT_ID, name,
                     CHECK_NOTNULL(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)),
      fields_(std::move(fields)),
      struct_member_store_(std::move(type_member_store)) {
  for (auto field : fields_) {
    parameters_.push_back(CHECK_NOTNULL(field.type_spec));
  }
  struct_member_store_->set_type_spec(this);
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

absl::StatusOr<TypeStruct*> TypeStruct::AddTypeStruct(
    const ScopeName& scope_name, TypeStore* type_store, absl::string_view name,
    std::vector<Field> fields) {
  ASSIGN_OR_RETURN(auto struct_spec,
                   type_store->FindTypeByName(kTypeNameStruct),
                   _ << "Probably a setup error: base type is not registered");
  auto struct_member_store = std::make_shared<StructMemberStore>(
      struct_spec, struct_spec->type_member_store_ptr());
  auto struct_type = absl::make_unique<TypeStruct>(
      type_store, std::move(struct_member_store), name, std::move(fields));
  RETURN_IF_ERROR(
      struct_type->struct_member_store()->AddFields(struct_type->fields()))
      << "Adding fields to: " << struct_type->full_name();
  TypeStruct* ret_type = struct_type.get();
  struct_type->set_scope_name(scope_name);
  RETURN_IF_ERROR(
      type_store->DeclareType(scope_name, name, std::move(struct_type))
          .status());
  return {ret_type};
}

std::unique_ptr<TypeSpec> TypeStruct::Clone() const {
  return absl::make_unique<TypeStruct>(type_store_, struct_member_store_, name_,
                                       fields_);
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
                                     std::shared_ptr<NameStore> ancestor)
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
  CHECK_NOTNULL(type_spec_);
  fields_.reserve(fields.size());
  field_vars_.reserve(fields.size());
  for (const auto& field : fields) {
    if (!NameUtil::IsValidName(field.name)) {
      return status::InvalidArgumentErrorBuilder()
             << "Invalid field name: " << field.name
             << " in type: " << type_spec_->full_name();
    }
    auto field_var = std::make_unique<Field>(
        field.name, CHECK_NOTNULL(field.type_spec), type_spec_, this);
    RETURN_IF_ERROR(AddChildStore(field.name, field_var.get()))
        << "Adding field: " << field.name
        << " to type: " << type_spec_->full_name();
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
                 std::shared_ptr<NameStore> type_member_store,
                 const TypeSpec* key, const TypeSpec* val)
    : StoredTypeSpec(type_store, pb::TypeId::MAP_ID, kTypeNameMap,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameIterable),
                     {TypeUtils::EnsureType(type_store, kTypeNameAny, key),
                      TypeUtils::EnsureType(type_store, kTypeNameAny, val)}),
      // TODO(catalin): TBD if we want a structure or a tuple as values.
      result_type_(std::make_unique<TypeStruct>(
          type_store_,
          std::make_shared<StructMemberStore>(
              TypeUtils::EnsureType(type_store, kTypeNameStruct),
              TypeUtils::EnsureType(type_store, kTypeNameStruct)
                  ->type_member_store_ptr()),
          absl::StrCat("_MapValue_", NextTypeId()),
          std::vector<TypeStruct::Field>(
              {TypeStruct::Field{"key", parameters_.front()},
               TypeStruct::Field{"value", parameters_.back()}}))) {
  CHECK_OK(
      result_type_->struct_member_store()->AddFields(result_type_->fields()));
}

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

// TODO(catalin): at some point should probably restrict the key types.
absl::StatusOr<std::unique_ptr<TypeSpec>> TypeMap::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings));
  CHECK_EQ(types.size(), 2ul);
  return {std::make_unique<TypeMap>(type_store_, type_member_store_,
                                    types.front(), types.back())};
}

TypeTuple::TypeTuple(TypeStore* type_store,
                     std::shared_ptr<NameStore> type_member_store,
                     std::vector<const TypeSpec*> parameters)
    : StoredTypeSpec(type_store, pb::TypeId::TUPLE_ID, kTypeNameTuple,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny),
                     std::move(parameters)) {}

const TypeSpec* TypeTuple::IndexType() const {
  if (!index_type_) {
    index_type_ = TypeUtils::IntIndexType(type_store_);
  }
  return index_type_.get();
}

std::unique_ptr<TypeSpec> TypeTuple::Clone() const {
  return std::make_unique<TypeTuple>(type_store_, type_member_store_,
                                     parameters_);
}

absl::StatusOr<std::unique_ptr<TypeSpec>> TypeTuple::Bind(
    const std::vector<TypeBindingArg>& bindings) const {
  if (!parameters_.empty() || bindings.empty()) {
    return TypeSpec::Bind(bindings);
  }
  ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                   _ << "Extracting types from bindings");
  CHECK_EQ(bindings.size(), types.size());
  return std::make_unique<TypeTuple>(type_store_, type_member_store_,
                                     std::move(types));
}

TypeFunction::TypeFunction(TypeStore* type_store,
                           std::shared_ptr<NameStore> type_member_store,
                           absl::string_view name,
                           std::vector<Argument> arguments,
                           const TypeSpec* result,
                           absl::optional<size_t> first_default_value_index)
    : StoredTypeSpec(type_store, pb::TypeId::FUNCTION_ID, name,
                     std::move(type_member_store), true,
                     TypeUtils::EnsureType(type_store, kTypeNameAny)),
      arguments_(std::move(arguments)),
      result_(result),
      first_default_value_index_(first_default_value_index) {
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
  if (result_) {
    ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings),
                     _ << "Extracting types from bindings");
    CHECK_EQ(types.size(), arguments_.size() + 1);
    arguments = arguments_;
    for (size_t i = 0; i < arguments.size(); ++i) {
      arguments[i].type_spec = types[i];
    }
    result_type = types.back();
  } else {
    if (bindings.empty()) {
      return absl::InvalidArgumentError("Empty binding arguments for Function");
    }
    arguments.reserve(bindings.size() - 1);
    ASSIGN_OR_RETURN(auto types, TypesFromBindings(bindings, false),
                     _ << "Extracting types from bindings");
    CHECK_EQ(types.size(), bindings.size());
    for (size_t i = 0; i + 1 < bindings.size(); ++i) {
      arguments.push_back(Argument{absl::StrCat("arg_", i + 1), "", types[i]});
    }
    result_type = types.back();
  }
  return {std::make_unique<TypeFunction>(type_store_, type_member_store_,
                                         name(), std::move(arguments),
                                         result_type)};
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
    std::optional<size_t> first_default_index) const {
  RET_CHECK(!first_default_index.has_value() ||
            first_default_index.value() < arguments.size());
  if (!result_) {
    return {std::make_unique<TypeFunction>(type_store_, type_member_store_,
                                           name_, arguments, result_type,
                                           first_default_index)};
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
             << "Too may arguments to bind function type: " << arguments.size()
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
  return {std::make_unique<TypeFunction>(type_store_, type_member_store_, name_,
                                         std::move(bind_args), result_type,
                                         first_default_value_index_)};
}

std::unique_ptr<TypeSpec> TypeFunction::Clone() const {
  return std::make_unique<TypeFunction>(
      type_store_, type_member_store_, name_,
      arguments_, result_, first_default_value_index_);
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
                     std::shared_ptr<NameStore> type_member_store,
                     std::vector<const TypeSpec*> parameters)
    : StoredTypeSpec(type_store, pb::TypeId::UNION_ID, kTypeNameUnion,
                     std::move(type_member_store), false,
                     TypeUtils::EnsureType(type_store, kTypeNameAny),
                     UnionSortTypes(std::move(parameters))) {}

bool TypeUnion::IsBound() const {
  return parameters_.size() == 1 && parameters_.front()->IsBound();
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
  return {std::make_unique<TypeUnion>(type_store_, type_member_store_,
                                      std::move(types))};
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
                           std::shared_ptr<NameStore> type_member_store,
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
  }
  if (types.size() != 1 && !nullable_bind) {
    return status::InvalidArgumentErrorBuilder()
           << "Nullable type requires exactly one parameter for binding. "
              " Provided: "
           << types.size();
  }
  if (parameters_.empty()) {
    if (types.front()->type_id() == pb::TypeId::NULL_ID) {
      return status::InvalidArgumentErrorBuilder()
             << "Cannot bind type Null as an argument to a Nullable type";
    }
    return std::make_unique<TypeNullable>(type_store_, type_member_store_,
                                          types.front());
  }
  const TypeSpec* arg_type = nullable_bind ? nullable_bind : types.front();
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
  return std::make_unique<TypeNullable>(type_store_, type_member_store_,
                                        nullable_bind);
}

std::unique_ptr<TypeSpec> TypeNullable::Clone() const {
  return std::make_unique<TypeNullable>(
      type_store_, type_member_store_,
      parameters_.empty() ? nullptr : parameters_.back());
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
                         std::shared_ptr<NameStore> type_member_store,
                         absl::string_view name, const TypeSpec* type_spec)
    : StoredTypeSpec(
          type_store, pb::TypeId::DATASET_ID,
          name.empty() ? kTypeNameDataset : name, std::move(type_member_store),
          true, TypeUtils::EnsureType(type_store, kTypeNameAny),
          {TypeUtils::EnsureType(type_store, kTypeNameAny, type_spec)}) {}

std::unique_ptr<TypeSpec> TypeDataset::Clone() const {
  CHECK_EQ(parameters_.size(), 1ul);
  return std::make_unique<TypeDataset>(type_store_, type_member_store_, name_,
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
  return std::make_unique<TypeDataset>(type_store_, type_member_store_, name_,
                                       types.front());
}

std::unique_ptr<TypeSpec> TypeUnknown::Clone() const {
  return std::make_unique<TypeUnknown>(type_member_store_ptr());
}

TypeUnknown::TypeUnknown(std::shared_ptr<NameStore> type_member_store)
    : TypeSpec(pb::TypeId::UNKNOWN_ID, kTypeNameUnknown,
               std::move(type_member_store)) {}

const TypeUnknown* TypeUnknown::Instance() {
  static const TypeUnknown* const kUnknownType = new TypeUnknown(nullptr);
  return kUnknownType;
}

const TypeSpec* TypeUnknown::type_spec() const { return this; }

const ScopeName& TypeUnknown::scope_name() const {
  static const auto kEmptyScope = new ScopeName();
  return *kEmptyScope;
}

}  // namespace analysis
}  // namespace nudl
