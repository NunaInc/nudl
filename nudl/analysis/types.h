#ifndef NUDL_ANALYSIS_TYPES_H__
#define NUDL_ANALYSIS_TYPES_H__

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "glog/logging.h"
#include "nudl/analysis/type_spec.h"
#include "nudl/analysis/type_store.h"
#include "nudl/proto/analysis.pb.h"

namespace nudl {
namespace analysis {

class StoredTypeSpec : public TypeSpec {
 public:
  StoredTypeSpec(TypeStore* type_store, pb::TypeId type_id,
                 absl::string_view name,
                 std::shared_ptr<NameStore> type_member_store,
                 bool is_bound_type = false, const TypeSpec* ancestor = nullptr,
                 std::vector<const TypeSpec*> parameters = {});

  template <class C>
  std::unique_ptr<TypeSpec> CloneType() const {
    return std::make_unique<C>(type_store_, type_member_store_);
  }
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* type_spec() const override;
  const ScopeName& scope_name() const override;
  void set_scope_name(ScopeName scope_name);

 protected:
  TypeStore* const type_store_;
  const TypeSpec* const object_type_spec_;
  std::optional<ScopeName> scope_name_;
};

class BaseTypesStore : public ScopeTypeStore {
 public:
  explicit BaseTypesStore(TypeStore* global_store)
      : ScopeTypeStore(absl::make_unique<ScopeName>(), global_store) {
    CreateBaseTypes();
  }

 private:
  void CreateBaseTypes();
};

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

  static bool IsIntType(pb::TypeId type_id);
  static bool IsUIntType(pb::TypeId type_id);
  static bool IsFloatType(pb::TypeId type_id);

  // Builds and returns a union of int and uint.
  // This expects the provided store to be fully built and initialized
  // with the standard types, else check fails.
  static std::unique_ptr<TypeSpec> IntIndexType(TypeStore* type_store);
  // Builds and returns a nullable of provided type.
  // This expects the provided store to be fully built and initialized
  // with the standard types, else check fails.
  static std::unique_ptr<TypeSpec> NullableType(TypeStore* type_store,
                                                const TypeSpec* type_spec);
};

// Type that represents the type of a type named object.
class TypeType : public StoredTypeSpec {
 public:
  TypeType(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeAny : public StoredTypeSpec {
 public:
  TypeAny(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeModule : public StoredTypeSpec {
 public:
  TypeModule(TypeStore* type_store,
             std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeNull : public StoredTypeSpec {
 public:
  TypeNull(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const;
};

class TypeNumeric : public StoredTypeSpec {
 public:
  TypeNumeric(TypeStore* type_store,
              std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeInt : public StoredTypeSpec {
 public:
  TypeInt(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeInt8 : public StoredTypeSpec {
 public:
  TypeInt8(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeInt16 : public StoredTypeSpec {
 public:
  TypeInt16(TypeStore* type_store,
            std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeInt32 : public StoredTypeSpec {
 public:
  TypeInt32(TypeStore* type_store,
            std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeUInt : public StoredTypeSpec {
 public:
  TypeUInt(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeUInt8 : public StoredTypeSpec {
 public:
  TypeUInt8(TypeStore* type_store,
            std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeUInt16 : public StoredTypeSpec {
 public:
  TypeUInt16(TypeStore* type_store,
             std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeUInt32 : public StoredTypeSpec {
 public:
  TypeUInt32(TypeStore* type_store,
             std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeFloat64 : public StoredTypeSpec {
 public:
  TypeFloat64(TypeStore* type_store,
              std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeFloat32 : public StoredTypeSpec {
 public:
  TypeFloat32(TypeStore* type_store,
              std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeString : public StoredTypeSpec {
 public:
  TypeString(TypeStore* type_store,
             std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeBytes : public StoredTypeSpec {
 public:
  TypeBytes(TypeStore* type_store,
            std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeBool : public StoredTypeSpec {
 public:
  TypeBool(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeTimestamp : public StoredTypeSpec {
 public:
  TypeTimestamp(TypeStore* type_store,
                std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeDate : public StoredTypeSpec {
 public:
  TypeDate(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeDateTime : public StoredTypeSpec {
 public:
  TypeDateTime(TypeStore* type_store,
               std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeTimeInterval : public StoredTypeSpec {
 public:
  TypeTimeInterval(TypeStore* type_store,
                   std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeDecimal : public StoredTypeSpec {
 public:
  TypeDecimal(TypeStore* type_store,
              std::shared_ptr<NameStore> type_member_store, int precision = -1,
              int scale = -1);
  std::unique_ptr<TypeSpec> Clone() const override;
  std::string full_name() const override;
  pb::ExpressionTypeSpec ToProto() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  static constexpr int kMaxPrecision = 78;

 protected:
  const int precision_ = -1;
  const int scale_ = -1;
};

class TypeIterable : public StoredTypeSpec {
 public:
  TypeIterable(TypeStore* type_store,
               std::shared_ptr<NameStore> type_member_store,
               const TypeSpec* param = nullptr);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsIterable() const override;
};

class TypeArray : public StoredTypeSpec {
 public:
  TypeArray(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store,
            const TypeSpec* param = nullptr);
  const TypeSpec* IndexType() const override;
  const TypeSpec* IndexedType() const override;
  std::unique_ptr<TypeSpec> Clone() const override;

 private:
  mutable std::unique_ptr<TypeSpec> index_type_;
  mutable std::unique_ptr<TypeSpec> indexed_type_;
};

class TypeSet : public StoredTypeSpec {
 public:
  TypeSet(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store,
          const TypeSpec* param = nullptr);
  const TypeSpec* IndexType() const override;
  const TypeSpec* IndexedType() const override;
  std::unique_ptr<TypeSpec> Clone() const override;

 private:
  const TypeSpec* const bool_type_;
};

class TypeTuple : public StoredTypeSpec {
 public:
  TypeTuple(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store,
            std::vector<const TypeSpec*> parameters = {});
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* IndexType() const override;

 private:
  mutable std::unique_ptr<TypeSpec> index_type_;
};

class StructMemberStore;
class TypeStruct : public StoredTypeSpec {
 public:
  // TODO(catalin): add field annotations & struct annotations.
  struct Field {
    std::string name;
    const TypeSpec* type_spec;
  };
  // Main way to create a type struct and add it to a store.
  static absl::StatusOr<TypeStruct*> AddTypeStruct(const ScopeName& scope_name,
                                                   TypeStore* type_store,
                                                   absl::string_view name,
                                                   std::vector<Field> fields);

  explicit TypeStruct(TypeStore* type_store,
                      std::shared_ptr<StructMemberStore> type_member_store,
                      absl::string_view name = kTypeNameStruct,
                      std::vector<Field> fields = {});
  bool IsBound() const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsEqual(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  pb::ExpressionTypeSpec ToProto() const override;

  const std::vector<Field>& fields() const;
  StructMemberStore* struct_member_store() const;

 protected:
  bool CheckStruct(const TypeSpec& type_spec,
                   const std::function<bool(const TypeSpec*, const TypeSpec*)>&
                       checker) const;

  std::vector<Field> fields_;
  std::shared_ptr<StructMemberStore> struct_member_store_;
};

class StructMemberStore : public TypeMemberStore {
 public:
  StructMemberStore(const TypeSpec* type_spec,
                    std::shared_ptr<NameStore> ancestor);
  ~StructMemberStore() override;

  // Adds a set of fields to this store.
  absl::Status AddFields(const std::vector<TypeStruct::Field>& fields);

  // This is a bit dirty, but we need to update the type spec on
  // TypeStruct construction.
  void set_type_spec(const TypeSpec* type_spec);

 protected:
  std::vector<TypeStruct::Field> fields_;
  std::vector<std::unique_ptr<NameStore>> field_vars_;
};

class TypeMap : public StoredTypeSpec {
 public:
  TypeMap(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store,
          const TypeSpec* key = nullptr, const TypeSpec* val = nullptr);
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* ResultType() const override;
  const TypeSpec* IndexedType() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  const TypeSpec* IndexType() const override;

 protected:
  std::unique_ptr<TypeStruct> result_type_;
  mutable std::unique_ptr<TypeSpec> indexed_type_;
};

class TypeFunction : public StoredTypeSpec {
 public:
  struct Argument {
    std::string name;
    std::string type_name;
    const TypeSpec* type_spec = nullptr;
    std::string ToString() const;
  };
  TypeFunction(TypeStore* type_store,
               std::shared_ptr<NameStore> type_member_store,
               absl::string_view name = kTypeNameFunction,
               std::vector<Argument> arguments = {},
               const TypeSpec* result = nullptr,
               absl::optional<size_t> first_default_value_index = {});

  std::string full_name() const override;
  bool IsBound() const override;
  const TypeSpec* ResultType() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  pb::ExpressionTypeSpec ToProto() const override;

  const std::vector<Argument>& arguments() const;
  void set_argument_name(size_t index, std::string name);
  absl::optional<size_t> first_default_value_index() const;
  // Preconditions: if value.has_value(),
  //    value < arguments_->size() and
  //    first_default_value_index_ not yet set.
  void set_first_default_value_index(absl::optional<size_t> value);

  absl::StatusOr<std::unique_ptr<TypeSpec>> BindWithFunction(
      const TypeFunction& fun) const;
  absl::StatusOr<std::unique_ptr<TypeSpec>> BindWithComponents(
      const std::vector<Argument>& arguments, const TypeSpec* result_type,
      std::optional<size_t> first_default_index) const;

 protected:
  std::vector<Argument> arguments_;
  const TypeSpec* result_;
  absl::optional<size_t> first_default_value_index_;
};

class TypeUnion : public StoredTypeSpec {
 public:
  TypeUnion(TypeStore* type_store, std::shared_ptr<NameStore> type_member_store,
            std::vector<const TypeSpec*> parameters = {});
  bool IsBound() const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeNullable : public StoredTypeSpec {
 public:
  TypeNullable(TypeStore* type_store,
               std::shared_ptr<NameStore> type_member_store,
               const TypeSpec* type_spec = nullptr);
  std::string full_name() const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeDataset : public StoredTypeSpec {
 public:
  TypeDataset(TypeStore* type_store,
              std::shared_ptr<NameStore> type_member_store,
              absl::string_view name = "", const TypeSpec* type_spec = nullptr);
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* ResultType() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
};

// Used for - no-type cases - use Instance() to get a static instance of this
// from any type store.
class TypeUnknown : public TypeSpec {
 public:
  explicit TypeUnknown(std::shared_ptr<NameStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* type_spec() const override;
  const ScopeName& scope_name() const override;

  static const TypeUnknown* Instance();
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_TYPES_H__
