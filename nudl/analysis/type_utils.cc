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

#include "nudl/analysis/type_utils.h"

#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_join.h"
#include "nudl/analysis/types.h"
#include "nudl/status/status.h"
#include "nudl/testing/stacktrace.h"

ABSL_FLAG(bool, nudl_accept_abstract_function_objects, false,
          "If we allow creation of concrete objects that point to "
          "abstract functions. E.g accept `f = (x => x + x)`, as is, "
          "instead of requiring something like `f = (x : Int => x + x)`. "
          "This is ok for dynamically typed languages, but for us, while "
          "we can do OK for most of the cases, some uses result in "
          "issues down the road, especially related to reassignment of "
          "these variables (`f` in this case), to something else, down in "
          "the code. Providing this as a flag for now. "
          "We may allow this with some limitations, (e.g. no reassignment) "
          "so the issues do not appear.");

namespace nudl {
namespace analysis {

const TypeSpec* TypeUtils::EnsureType(TypeStore* type_store,
                                      absl::string_view name,
                                      const TypeSpec* spec) {
  if (spec) {
    return spec;
  }
  auto result = type_store->FindTypeByName(name);
  if (result.ok()) {
    return result.value();
  }
  if (!type_store->GlobalStore()) {
    CHECK_OK(result.status());
  }
  auto result_global = type_store->GlobalStore()->FindTypeByName(name);
  if (result_global.ok()) {
    return result_global.value();
  }
  CHECK_OK(result_global.status()) << "Also: " << result.status();
  return nullptr;
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
          {pb::TypeId::INTEGRAL_ID, std::string(kTypeNameIntegral)},
          {pb::TypeId::CONTAINER_ID, std::string(kTypeNameContainer)},
          {pb::TypeId::GENERATOR_ID, std::string(kTypeNameGenerator)},
      });
  auto it = kTypeNames->find(type_id);
  if (it == kTypeNames->end()) {
    return kTypeNameUnknown;
  }
  return it->second;
}

bool TypeUtils::IsUIntType(const TypeSpec& type_spec) {
  const size_t type_id = type_spec.type_id();
  return (type_id == pb::TypeId::UINT_ID || type_id == pb::TypeId::UINT8_ID ||
          type_id == pb::TypeId::UINT16_ID || type_id == pb::TypeId::UINT32_ID);
}

bool TypeUtils::IsIntType(const TypeSpec& type_spec) {
  const size_t type_id = type_spec.type_id();
  return (type_id == pb::TypeId::INT_ID || type_id == pb::TypeId::INT8_ID ||
          type_id == pb::TypeId::INT16_ID || type_id == pb::TypeId::INT32_ID);
}

bool TypeUtils::IsFloatType(const TypeSpec& type_spec) {
  const size_t type_id = type_spec.type_id();
  return (type_id == pb::TypeId::FLOAT32_ID ||
          type_id == pb::TypeId::FLOAT64_ID);
}

bool TypeUtils::IsNullType(const TypeSpec& type_spec) {
  return type_spec.type_id() == pb::TypeId::NULL_ID;
}

bool TypeUtils::IsAnyType(const TypeSpec& type_spec) {
  return type_spec.type_id() == pb::TypeId::ANY_ID;
}

bool TypeUtils::IsTupleType(const TypeSpec& type_spec) {
  return type_spec.type_id() == pb::TypeId::TUPLE_ID;
}

bool TypeUtils::IsTupleJoinType(const TypeSpec& type_spec) {
  return IsTupleType(type_spec) && type_spec.name() == kTypeNameTupleJoin;
}

bool TypeUtils::IsNamedTupleType(const TypeSpec& type_spec) {
  if (!IsTupleType(type_spec)) {
    return false;
  }
  return static_cast<const TypeTuple&>(type_spec).is_named();
}

bool TypeUtils::IsNullLikeType(const TypeSpec& type_spec) {
  const size_t type_id = type_spec.type_id();
  return (type_id == pb::TypeId::NULL_ID || type_id == pb::TypeId::NULLABLE_ID);
}

bool TypeUtils::IsNullableType(const TypeSpec& type_spec) {
  const size_t type_id = type_spec.type_id();
  return type_id == pb::TypeId::NULLABLE_ID;
}

bool TypeUtils::IsFunctionType(const TypeSpec& type_spec) {
  return type_spec.type_id() == pb::TypeId::FUNCTION_ID;
}

bool TypeUtils::IsStructType(const TypeSpec& type_spec) {
  return type_spec.type_id() == pb::TypeId::STRUCT_ID;
}

bool TypeUtils::IsDatasetType(const TypeSpec& type_spec) {
  return type_spec.type_id() == pb::TypeId::DATASET_ID;
}

bool TypeUtils::IsArrayType(const TypeSpec& type_spec) {
  return type_spec.type_id() == pb::TypeId::ARRAY_ID;
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

// TODO(catalin): may want to include more here e.g. anything that contains
//  undefined in any parameter type.
bool TypeUtils::IsUndefinedArgType(const TypeSpec* arg_type) {
  const size_t type_id = CHECK_NOTNULL(arg_type)->type_id();
  if (type_id == pb::TypeId::ANY_ID || type_id == pb::TypeId::UNKNOWN_ID ||
      type_id == pb::TypeId::UNION_ID) {
    return true;
  } else if (type_id == pb::TypeId::NULLABLE_ID) {
    if (arg_type->parameters().empty()) {
      return true;
    }
    return IsUndefinedArgType(arg_type->parameters().back());
  } else if (type_id == pb::TypeId::TUPLE_ID ||
             type_id == pb::TypeId::STRUCT_ID) {
    if (arg_type->parameters().empty()) {
      return true;
    }
  } else if (type_id == pb::TypeId::FUNCTION_ID) {
    return arg_type->parameters().empty();
  }
  for (const TypeSpec* param : arg_type->parameters()) {
    if (IsUndefinedArgType(param)) {
      return true;
    }
  }
  return false;
}

absl::Status TypeUtils::CheckFunctionTypeIsBound(const TypeSpec* type_spec) {
  if (type_spec->type_id() != pb::TypeId::FUNCTION_ID || type_spec->IsBound()) {
    return absl::OkStatus();
  }
  // There is a discussion here. I want to return OK in this case.
  //   However, the problem is that while I can generate the
  //   bindings for all these things, I cannot properly direct to the correct
  //   binding up calling. E.g.
  // f = (x, y) => x + y
  // a = f(1, 2)
  // b = f(.1, .2)
  // While I can generate the bindings for int and float, upon for a and b.
  // And the problem is that it is the same f.. - so would need to actually
  // generate an f, and point dynamically to the first or second bind.
  // The solution is to actually generate two bindings for f as normally,
  // plus a map from string to function for f, and the actual call happend
  // by using "a = f[<signature1>](1, 2)" and "a = f[<signature2>](1, 2)"
  //
  // This however creates issues when generating the code, as we do not know
  // which bind to use. Would be OK for a dynamic typed languages, as all
  // the binds are basically identical. May also be ok for languages that
  // use generics, but still we do not know the type of the object instances
  // created this way, so for now we bail out and enable this only on a flag.
  //
  auto fun = static_cast<const TypeFunction*>(type_spec);
  if (absl::GetFlag(FLAGS_nudl_accept_abstract_function_objects) &&
      !fun->function_instances().empty()) {
    return absl::OkStatus();
  }
  absl::flat_hash_set<std::string> unbound_types;
  TypeUtils::FindUnboundTypes(type_spec, &unbound_types);
  return status::InvalidArgumentErrorBuilder()
         << "Provided function type needs to be bound. "
            "Please add non-abstract type specifications to all arguments "
            "and (maybe) define the return value as well if necessary"
            "Type found: "
         << type_spec->full_name()
         << " unbound argument types: " << absl::StrJoin(unbound_types, ", ");
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
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeIntegral>(this, nullptr))
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
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeContainer>(this, nullptr))
               .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<TypeGenerator>(this, nullptr))
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
                       std::make_unique<TypeTupleJoin>(
                           this, TypeUtils::EnsureType(this, kTypeNameTuple)
                                     ->type_member_store_ptr()))
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
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<DatasetAggregate>(
                           this, TypeUtils::EnsureType(this, kTypeNameDataset)
                                     ->type_member_store_ptr()))
               .status());
  CHECK_OK(DeclareType(*scope_name_, "",
                       std::make_unique<DatasetJoin>(
                           this, TypeUtils::EnsureType(this, kTypeNameDataset)
                                     ->type_member_store_ptr()))
               .status());
}

}  // namespace analysis
}  // namespace nudl
