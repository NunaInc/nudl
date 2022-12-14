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
#include "nudl/analysis/type_utils.h"
#include "nudl/proto/analysis.pb.h"

namespace nudl {
namespace analysis {

class StoredTypeSpec : public TypeSpec {
 public:
  StoredTypeSpec(TypeStore* type_store, int type_id, absl::string_view name,
                 std::shared_ptr<TypeMemberStore> type_member_store,
                 bool is_bound_type = false, const TypeSpec* ancestor = nullptr,
                 std::vector<const TypeSpec*> parameters = {},
                 absl::optional<const TypeSpec*> original_bind = {});

  template <class C>
  std::unique_ptr<TypeSpec> CloneType() const {
    return std::make_unique<C>(type_store_, type_member_store_);
  }
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* type_spec() const override;
  const ScopeName& scope_name() const override;
  TypeStore* type_store() const;

 protected:
  TypeStore* const type_store_;
  const TypeSpec* const object_type_spec_;
};

// Type that represents the type of a type named object.
class TypeType : public StoredTypeSpec {
 public:
  TypeType(TypeStore* type_store,
           std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeAny : public StoredTypeSpec {
 public:
  TypeAny(TypeStore* type_store,
          std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeModule : public StoredTypeSpec {
 public:
  TypeModule(TypeStore* type_store, absl::string_view module_name,
             NameStore* module);
  std::unique_ptr<TypeSpec> Clone() const override;

 protected:
  std::string module_name_;
  NameStore* const module_;
};

class TypeNull : public StoredTypeSpec {
 public:
  TypeNull(TypeStore* type_store,
           std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
  // Binding on null returns a new null or nullable
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
};

class TypeNumeric : public StoredTypeSpec {
 public:
  TypeNumeric(TypeStore* type_store,
              std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeIntegral : public StoredTypeSpec {
 public:
  TypeIntegral(TypeStore* type_store,
               std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeInt : public StoredTypeSpec {
 public:
  TypeInt(TypeStore* type_store,
          std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeInt8 : public StoredTypeSpec {
 public:
  TypeInt8(TypeStore* type_store,
           std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeInt16 : public StoredTypeSpec {
 public:
  TypeInt16(TypeStore* type_store,
            std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeInt32 : public StoredTypeSpec {
 public:
  TypeInt32(TypeStore* type_store,
            std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeUInt : public StoredTypeSpec {
 public:
  TypeUInt(TypeStore* type_store,
           std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeUInt8 : public StoredTypeSpec {
 public:
  TypeUInt8(TypeStore* type_store,
            std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeUInt16 : public StoredTypeSpec {
 public:
  TypeUInt16(TypeStore* type_store,
             std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeUInt32 : public StoredTypeSpec {
 public:
  TypeUInt32(TypeStore* type_store,
             std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
};

class TypeFloat64 : public StoredTypeSpec {
 public:
  TypeFloat64(TypeStore* type_store,
              std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeFloat32 : public StoredTypeSpec {
 public:
  TypeFloat32(TypeStore* type_store,
              std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeString : public StoredTypeSpec {
 public:
  TypeString(TypeStore* type_store,
             std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeBytes : public StoredTypeSpec {
 public:
  TypeBytes(TypeStore* type_store,
            std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeBool : public StoredTypeSpec {
 public:
  TypeBool(TypeStore* type_store,
           std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeTimestamp : public StoredTypeSpec {
 public:
  TypeTimestamp(TypeStore* type_store,
                std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeDate : public StoredTypeSpec {
 public:
  TypeDate(TypeStore* type_store,
           std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeDateTime : public StoredTypeSpec {
 public:
  TypeDateTime(TypeStore* type_store,
               std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeTimeInterval : public StoredTypeSpec {
 public:
  TypeTimeInterval(TypeStore* type_store,
                   std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
};

class TypeDecimal : public StoredTypeSpec {
 public:
  TypeDecimal(TypeStore* type_store,
              std::shared_ptr<TypeMemberStore> type_member_store,
              int precision = -1, int scale = -1);
  std::unique_ptr<TypeSpec> Clone() const override;
  std::string full_name() const override;
  pb::ExpressionTypeSpec ToProto() const override;
  pb::TypeSpec ToTypeSpecProto(const ScopeName& call_scope_name) const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
  static constexpr int kMaxPrecision = 78;

 protected:
  const int precision_ = -1;
  const int scale_ = -1;
};

class TypeIterable : public StoredTypeSpec {
 public:
  TypeIterable(TypeStore* type_store,
               std::shared_ptr<TypeMemberStore> type_member_store,
               const TypeSpec* param = nullptr);
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsIterable() const override;
};

class TypeGenerator : public StoredTypeSpec {
 public:
  TypeGenerator(TypeStore* type_store,
                std::shared_ptr<TypeMemberStore> type_member_store,
                const TypeSpec* param = nullptr);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeContainer : public StoredTypeSpec {
 public:
  TypeContainer(TypeStore* type_store,
                std::shared_ptr<TypeMemberStore> type_member_store,
                const TypeSpec* param = nullptr);
  std::unique_ptr<TypeSpec> Clone() const override;
};

class TypeArray : public StoredTypeSpec {
 public:
  TypeArray(TypeStore* type_store,
            std::shared_ptr<TypeMemberStore> type_member_store,
            const TypeSpec* param = nullptr);
  const TypeSpec* IndexType() const override;
  const TypeSpec* IndexedType() const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;

 private:
  mutable std::unique_ptr<TypeSpec> index_type_;
  mutable std::unique_ptr<TypeSpec> indexed_type_;
};

class TypeSet : public StoredTypeSpec {
 public:
  TypeSet(TypeStore* type_store,
          std::shared_ptr<TypeMemberStore> type_member_store,
          const TypeSpec* param = nullptr);
  const TypeSpec* IndexType() const override;
  const TypeSpec* IndexedType() const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;

 private:
  const TypeSpec* const bool_type_;
};

class TypeTuple : public StoredTypeSpec {
 public:
  TypeTuple(TypeStore* type_store,
            std::shared_ptr<TypeMemberStore> type_member_store,
            std::vector<const TypeSpec*> parameters = {},
            std::vector<std::string> names = {},
            absl::optional<const TypeSpec*> original_bind = {});
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* IndexType() const override;
  std::string full_name() const override;
  bool IsBound() const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;

  const std::vector<std::string>& names() const;
  bool is_named() const;

  void UpdateNames(const TypeSpec* type_spec);

 private:
  bool ComputeIsNamed() const;

  mutable std::unique_ptr<TypeSpec> index_type_;
  std::vector<std::string> names_;
  bool is_named_ = false;
};

// This behaves exactly as a TypeTuple, except it overrides
// Bind and Clone, and is registered under a different name.
// In effect we would have only an abstract instance of,
// TypeTupleJoin as its Bind creates tuples.
class TypeTupleJoin : public TypeTuple {
 public:
  TypeTupleJoin(TypeStore* type_store,
                std::shared_ptr<TypeMemberStore> type_member_store,
                std::vector<const TypeSpec*> parameters = {});
  absl::StatusOr<std::unique_ptr<TypeSpec>> Build(
      const std::vector<TypeBindingArg>& bindings) const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  std::unique_ptr<TypeSpec> Clone() const override;
};

class StructMemberStore;
class TypeStruct : public StoredTypeSpec {
 public:
  // TODO(catalin): add field annotations & struct annotations.
  struct Field {
    std::string name;
    const TypeSpec* type_spec;
  };
  // Creates a structured type with specified name and fields.
  static absl::StatusOr<std::unique_ptr<TypeStruct>> CreateTypeStruct(
      TypeStore* base_store, TypeStore* registration_store,
      absl::string_view name, std::vector<Field> fields);
  // Main way to create a type struct and add it to a store.
  // Used CreateTypeStruct, then sets the scope and adds it to store.
  static absl::StatusOr<TypeStruct*> AddTypeStruct(
      const ScopeName& scope_name, TypeStore* base_store,
      TypeStore* registration_store, absl::string_view name,
      std::vector<Field> fields);

  explicit TypeStruct(TypeStore* type_store,
                      std::shared_ptr<StructMemberStore> type_member_store,
                      absl::string_view name = kTypeNameStruct,
                      std::vector<Field> fields = {},
                      bool is_abstract_struct = true);
  std::string full_name() const override;
  bool IsBound() const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsEqual(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  pb::ExpressionTypeSpec ToProto() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
  pb::TypeSpec ToTypeSpecProto(const ScopeName& call_scope_name) const override;
  std::string TypeSignature() const override;

  const std::vector<Field>& fields() const;
  StructMemberStore* struct_member_store() const;
  bool is_abstract_struct() const;

 protected:
  bool CheckStruct(const TypeSpec& type_spec,
                   const std::function<bool(const TypeSpec*, const TypeSpec*)>&
                       checker) const;

  std::vector<Field> fields_;
  std::shared_ptr<StructMemberStore> struct_member_store_;
  const bool is_abstract_struct_;
};

class StructMemberStore : public TypeMemberStore {
 public:
  StructMemberStore(const TypeSpec* type_spec,
                    std::shared_ptr<TypeMemberStore> ancestor);
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
  TypeMap(TypeStore* type_store,
          std::shared_ptr<TypeMemberStore> type_member_store,
          const TypeSpec* key = nullptr, const TypeSpec* val = nullptr);
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* ResultType() const override;
  const TypeSpec* IndexedType() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  const TypeSpec* IndexType() const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;

 protected:
  std::unique_ptr<TypeTuple> result_type_;
  mutable std::unique_ptr<TypeSpec> indexed_type_;
};

class Function;
class TypeFunction : public StoredTypeSpec {
 public:
  struct Argument {
    std::string name;
    std::string type_name;
    const TypeSpec* type_spec = nullptr;
    std::string ToString() const;
  };
  TypeFunction(
      TypeStore* type_store, std::shared_ptr<TypeMemberStore> type_member_store,
      absl::string_view name = kTypeNameFunction,
      std::vector<Argument> arguments = {}, const TypeSpec* result = nullptr,
      absl::optional<const TypeSpec*> original_bind = {},
      absl::optional<size_t> first_default_value_index = {},
      std::shared_ptr<absl::flat_hash_set<Function*>> function_instances = {});

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
      absl::optional<size_t> first_default_index) const;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;

  const absl::flat_hash_set<Function*>& function_instances() const;
  void add_function_instance(Function* instance);

 protected:
  std::vector<Argument> arguments_;
  const TypeSpec* result_;
  absl::optional<size_t> first_default_value_index_;
  std::shared_ptr<absl::flat_hash_set<Function*>> function_instances_;
};

class TypeUnion : public StoredTypeSpec {
 public:
  TypeUnion(TypeStore* type_store,
            std::shared_ptr<TypeMemberStore> type_member_store,
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
               std::shared_ptr<TypeMemberStore> type_member_store,
               const TypeSpec* type_spec = nullptr);
  std::string full_name() const override;
  std::unique_ptr<TypeSpec> Clone() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  absl::StatusOr<pb::Expression> DefaultValueExpression(
      const ScopeName& call_scope_name) const override;
  std::string TypeSignature() const override;
};

class TypeDataset : public StoredTypeSpec {
 public:
  TypeDataset(TypeStore* type_store,
              std::shared_ptr<TypeMemberStore> type_member_store,
              absl::optional<const TypeSpec*> original_bind = {},
              absl::string_view name = "", const TypeSpec* type_spec = nullptr);
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* ResultType() const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;

  // This is to set in which new dataset structure types produced
  // by Aggregates / Joins should be registered.
  // We want to add them to the current module, but is no proper way
  // to pass that through the binding process where they are created,
  // so we need a static setting:
  static void PushRegistrationStore(TypeStore* type_store);
  static void PopRegistrationStore();
  static TypeStore* GetRegistrationStore(TypeStore* default_type_store);
};

class DatasetAggregate : public TypeDataset {
 public:
  DatasetAggregate(TypeStore* type_store,
                   std::shared_ptr<TypeMemberStore> type_member_store,
                   std::vector<const TypeSpec*> parameters = {});
  absl::StatusOr<std::unique_ptr<TypeSpec>> Build(
      const std::vector<TypeBindingArg>& bindings) const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  std::unique_ptr<TypeSpec> Clone() const override;

 protected:
  absl::StatusOr<const TypeSpec*> AggregateFieldType(
      absl::string_view aggregate_type, const TypeSpec* type_spec) const;
  mutable std::vector<std::unique_ptr<TypeSpec>> allocated_types_;
};

class DatasetJoin : public TypeDataset {
 public:
  DatasetJoin(TypeStore* type_store,
              std::shared_ptr<TypeMemberStore> type_member_store,
              std::vector<const TypeSpec*> parameters = {});
  absl::StatusOr<std::unique_ptr<TypeSpec>> Build(
      const std::vector<TypeBindingArg>& bindings) const override;
  absl::StatusOr<std::unique_ptr<TypeSpec>> Bind(
      const std::vector<TypeBindingArg>& bindings) const override;
  bool IsAncestorOf(const TypeSpec& type_spec) const override;
  bool IsConvertibleFrom(const TypeSpec& type_spec) const override;
  std::unique_ptr<TypeSpec> Clone() const override;

 private:
  mutable std::vector<std::unique_ptr<TypeSpec>> allocated_types_;
};

// Used for - no-type cases - use Instance() to get a static instance of this
// from any type store.
class TypeUnknown : public TypeSpec {
 public:
  explicit TypeUnknown(std::shared_ptr<TypeMemberStore> type_member_store);
  std::unique_ptr<TypeSpec> Clone() const override;
  const TypeSpec* type_spec() const override;

  static const TypeUnknown* Instance();
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_TYPES_H__
