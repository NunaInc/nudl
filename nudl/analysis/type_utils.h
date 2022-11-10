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

#ifndef NUDL_ANALYSIS_TYPE_UTILS_H__
#define NUDL_ANALYSIS_TYPE_UTILS_H__

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "nudl/analysis/type_spec.h"
#include "nudl/analysis/type_store.h"
#include "nudl/proto/analysis.pb.h"

namespace nudl {
namespace analysis {

// Names for all standard types.
inline constexpr absl::string_view kTypeNameUnknown = "Unknown";
inline constexpr absl::string_view kTypeNameAny = "Any";
inline constexpr absl::string_view kTypeNameNull = "Null";
inline constexpr absl::string_view kTypeNameNumeric = "Numeric";
inline constexpr absl::string_view kTypeNameInt = "Int";
inline constexpr absl::string_view kTypeNameInt8 = "Int8";
inline constexpr absl::string_view kTypeNameInt16 = "Int16";
inline constexpr absl::string_view kTypeNameInt32 = "Int32";
inline constexpr absl::string_view kTypeNameUInt = "UInt";
inline constexpr absl::string_view kTypeNameUInt8 = "UInt8";
inline constexpr absl::string_view kTypeNameUInt16 = "UInt16";
inline constexpr absl::string_view kTypeNameUInt32 = "UInt32";
inline constexpr absl::string_view kTypeNameString = "String";
inline constexpr absl::string_view kTypeNameBytes = "Bytes";
inline constexpr absl::string_view kTypeNameBool = "Bool";
inline constexpr absl::string_view kTypeNameFloat32 = "Float32";
inline constexpr absl::string_view kTypeNameFloat64 = "Float64";
inline constexpr absl::string_view kTypeNameDate = "Date";
inline constexpr absl::string_view kTypeNameDateTime = "DateTime";
inline constexpr absl::string_view kTypeNameTimeInterval = "TimeInterval";
inline constexpr absl::string_view kTypeNameTimestamp = "Timestamp";
inline constexpr absl::string_view kTypeNameDecimal = "Decimal";
inline constexpr absl::string_view kTypeNameIterable = "Iterable";
inline constexpr absl::string_view kTypeNameArray = "Array";
inline constexpr absl::string_view kTypeNameTuple = "Tuple";
inline constexpr absl::string_view kTypeNameSet = "Set";
inline constexpr absl::string_view kTypeNameMap = "Map";
inline constexpr absl::string_view kTypeNameStruct = "Struct";
inline constexpr absl::string_view kTypeNameFunction = "Function";
inline constexpr absl::string_view kTypeNameUnion = "Union";
inline constexpr absl::string_view kTypeNameNullable = "Nullable";
inline constexpr absl::string_view kTypeNameDataset = "Dataset";
inline constexpr absl::string_view kTypeNameType = "Type";
inline constexpr absl::string_view kTypeNameModule = "Module";
inline constexpr absl::string_view kTypeNameIntegral = "Integral";
inline constexpr absl::string_view kTypeNameContainer = "Container";
inline constexpr absl::string_view kTypeNameGenerator = "Generator";

// These are not different types, but type aliases with
// different binds.
inline constexpr absl::string_view kTypeNameTupleJoin = "TupleJoin";
inline constexpr absl::string_view kTypeNameDatasetAggregate =
    "DatasetAggregate";
inline constexpr absl::string_view kTypeNameDatasetJoin = "DatasetJoin";

class BaseTypesStore : public ScopeTypeStore {
 public:
  explicit BaseTypesStore(TypeStore* global_store)
      : ScopeTypeStore(absl::make_unique<ScopeName>(), global_store) {
    CreateBaseTypes();
  }

 private:
  void CreateBaseTypes();
};

class TypeUtils {
 public:
  // Utility that converts a type id to one of the names above.
  static absl::string_view BaseTypeName(pb::TypeId type_id);

  // Remove duplicate types in provided span:
  static std::vector<const TypeSpec*> DedupTypes(
      absl::Span<const TypeSpec*> parameters);

  // This is used for standard type construction.
  // If spec is not null we return it, else we check that the provided type
  // name can be found in the store and return it.
  static const TypeSpec* EnsureType(TypeStore* type_store,
                                    absl::string_view name,
                                    const TypeSpec* spec = nullptr);

  // Finds specific types in type_spec or parameters that are unbound.
  static void FindUnboundTypes(const TypeSpec* type_spec,
                               absl::flat_hash_set<std::string>* type_names);

  // Classifying parameters:
  static bool IsIntType(const TypeSpec& type_spec);
  static bool IsUIntType(const TypeSpec& type_spec);
  static bool IsFloatType(const TypeSpec& type_spec);
  static bool IsNullType(const TypeSpec& type_spec);
  static bool IsNullableType(const TypeSpec& type_spec);
  static bool IsAnyType(const TypeSpec& type_spec);
  static bool IsTupleType(const TypeSpec& type_spec);
  static bool IsNamedTupleType(const TypeSpec& type_spec);
  static bool IsTupleJoinType(const TypeSpec& type_spec);
  static bool IsFunctionType(const TypeSpec& type_spec);
  static bool IsStructType(const TypeSpec& type_spec);
  static bool IsDatasetType(const TypeSpec& type_spec);

  // If this type when passed as an arg to a function, cannot yield
  // us much information, and would mostly fail the function if
  // we try to parse its expressions.
  static bool IsUndefinedArgType(const TypeSpec* arg_type);

  // Builds and returns a union of int and uint.
  // This expects the provided store to be fully built and initialized
  // with the standard types, else check fails.
  static std::unique_ptr<TypeSpec> IntIndexType(TypeStore* type_store);
  // Builds and returns a nullable of provided type.
  // This expects the provided store to be fully built and initialized
  // with the standard types, else check fails.
  static std::unique_ptr<TypeSpec> NullableType(TypeStore* type_store,
                                                const TypeSpec* type_spec);
  // Checks if the provided type is a function, or it is bound.
  static absl::Status CheckFunctionTypeIsBound(const TypeSpec* type_spec);
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_TYPE_UTILS_H__
