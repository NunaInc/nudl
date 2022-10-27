#ifndef NUDL_ANALYSIS_VARS_H__
#define NUDL_ANALYSIS_VARS_H__

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "nudl/analysis/expression.h"
#include "nudl/analysis/named_object.h"
#include "nudl/analysis/type_spec.h"
#include "nudl/proto/analysis.pb.h"

namespace nudl {
namespace analysis {

// This is base of assignable objects: variable, parameters, arguments and
// fields. They are initialized in a scope, for vars, parameters and arguments,
// or in another VarBase for fields.
class VarBase : public WrappedNameStore {
 public:
  VarBase(absl::string_view name, const TypeSpec* type_spec,
          absl::optional<NameStore*> parent_store = {});
  ~VarBase() override;

  const TypeSpec* type_spec() const override;
  absl::optional<NameStore*> parent_store() const override;
  const std::vector<Expression*> assignments() const;
  const std::vector<const TypeSpec*> assign_types() const;
  const TypeSpec* original_type() const;

  // Marks the assignment of a variable / field with an expression
  // The value of the status or can be a new expression that was
  // created for auto type conversion.
  // Errors are returned for type mismatch.
  absl::StatusOr<std::unique_ptr<Expression>> Assign(
      std::unique_ptr<Expression> expression);

  // We override this, to create local instances of the field/var
  // based names in the get.
  absl::StatusOr<NamedObject*> GetName(absl::string_view local_name,
                                       bool in_self_only) override;

  // Override these: we don't support - they should be added to types.
  absl::Status AddName(absl::string_view local_name,
                       NamedObject* object) override;
  absl::Status AddChildStore(absl::string_view local_name,
                             NameStore* store) override;

  // Returns the root VarBase from accessing this one.
  // Most would return this VarBase, except the Field, which would
  // return the top var from which it is obtained.
  VarBase* GetRootVar();

  // Clones this var (mostly fields).
  virtual std::unique_ptr<VarBase> Clone(
      absl::optional<NameStore*> parent_store) const = 0;

  // If provided named object is a variable kind (var, field, parma):
  static bool IsVarKind(const NamedObject& object);

 protected:
  const TypeSpec* const original_type_;
  const TypeSpec* type_spec_ = nullptr;
  absl::optional<NameStore*> const parent_store_;
  absl::flat_hash_map<std::string, NamedObject*> local_fields_map_;
  std::vector<std::unique_ptr<NamedObject>> local_fields_;
  std::vector<Expression*> assignments_;
  std::vector<const TypeSpec*> assign_types_;
};

// A field in structure based VarBase.
class Field : public VarBase {
 public:
  Field(absl::string_view name, const TypeSpec* type_spec,
        const TypeSpec* parent_type,
        std::optional<NameStore*> parent_store = {});

  pb::ObjectKind kind() const override;
  std::string full_name() const override;
  std::unique_ptr<VarBase> Clone(
      absl::optional<NameStore*> parent_store) const override;

 private:
  const TypeSpec* const parent_type_;
};

// A variable in a function or module scope.
class Var : public VarBase {
 public:
  Var(absl::string_view name, const TypeSpec* type_spec,
      absl::optional<NameStore*> parent_store);
  pb::ObjectKind kind() const override;
  std::unique_ptr<VarBase> Clone(
      absl::optional<NameStore*> parent_store) const override;
};

// A configuration parameter, in a module scope.
class Parameter : public Var {
 public:
  Parameter(absl::string_view name, const TypeSpec* type_spec,
            absl::optional<NameStore*> parent_store);
  pb::ObjectKind kind() const override;
  std::unique_ptr<VarBase> Clone(
      absl::optional<NameStore*> parent_store) const override;
};

// Argument in a function - we expect parent_store to be such.
// Note: we use term 'argument' for function parameters as well, to
// differentiate from the 'paremeter' above, which refers to a
// configuration parameter, at module level.
class Argument : public Var {
 public:
  Argument(absl::string_view name, const TypeSpec* type_spec,
           absl::optional<NameStore*> parent_store);
  pb::ObjectKind kind() const override;
  std::unique_ptr<VarBase> Clone(
      absl::optional<NameStore*> parent_store) const override;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_VARS_H__
