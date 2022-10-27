#ifndef NUDL_ANALYSIS_NAMED_OBJECT_H__
#define NUDL_ANALYSIS_NAMED_OBJECT_H__

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "nudl/analysis/names.h"
#include "nudl/proto/analysis.pb.h"

namespace nudl {
namespace analysis {

absl::string_view ObjectKindName(pb::ObjectKind kind);

class NameStore;
class TypeSpec;

// This represents a language object that can be named,
// and later retrieved by its name.
class NamedObject {
 public:
  explicit NamedObject(absl::string_view name);
  virtual ~NamedObject();

  virtual pb::ObjectKind kind() const = 0;

  // This is the store associated with this named object.
  // Usually if this name supports underlying name finding.
  // By default returns nullptr.
  virtual absl::optional<NameStore*> name_store();

  // This is the store that contains this named object.
  virtual absl::optional<NameStore*> parent_store() const;

  // The data type associated with this object, if any.
  // By default returns nullptr.
  virtual const TypeSpec* type_spec() const = 0;

  // A full descriptive name of this object.
  virtual std::string full_name() const;

  // Returns a quick reference to this object: kind and name.
  pb::NamedObjectRef ToProtoRef() const;

  // Returns the full reference of this object and names underneath.
  virtual pb::NamedObject ToProtoObject() const;

  const std::string& name() const;
  absl::string_view kind_name() const;

  // If this object is an ancestor store of provided named object,
  // obtained by walking up on object->parent_store().
  bool IsAncestorOf(const NamedObject* object) const;

 protected:
  std::string name_;
};

// A class that finds named object, when looking them up
// from a scope.
class NameStore : public NamedObject {
 public:
  explicit NameStore(absl::string_view name);

  absl::optional<NameStore*> name_store() override;

  // While making a lookup in lookup_scope, we try to find
  // the object designated by scoped_name. The scoping
  // lookup order follows the c++ order.
  // Cannot make this function const, as some implementations
  // may choose to add the name, or return a pointer to
  // themselves (e.g. scopes).
  virtual absl::StatusOr<NamedObject*> FindName(
      const ScopeName& lookup_scope, const ScopedName& scoped_name) = 0;

  // Adds a name to the store, not owned byt this store.
  virtual absl::Status AddName(absl::string_view local_name,
                               NamedObject* object) = 0;

  // If this store contains the provided local name.
  virtual bool HasName(absl::string_view local_name,
                       bool in_self_only) const = 0;

  // Returns directly the name in this store, w/o lookup rules and such.
  virtual absl::StatusOr<NamedObject*> GetName(absl::string_view local_name,
                                               bool in_self_only) = 0;

  // Adds a child substore to this name store.
  virtual absl::Status AddChildStore(absl::string_view local_name,
                                     NameStore* store) = 0;
  // Adds a child sub-store that is owned by this store.
  virtual absl::Status AddOwnedChildStore(absl::string_view local_name,
                                          std::unique_ptr<NameStore> store) = 0;

  // Finds a underlying store in this one.
  virtual absl::StatusOr<NameStore*> FindChildStore(
      const ScopeName& lookup_scope) = 0;

  // Returns the available names in the store, mostly for error printing:
  virtual std::vector<std::string> DefinedNames() const = 0;

  virtual std::string DebugString() const = 0;
  virtual std::string DebugNames() const = 0;

  // Normalizes the a sope name to store in local store - this means
  // removing the '::' prefix of function scope names.
  absl::string_view NormalizeLocalName(absl::string_view local_name) const;
};

// A base implementation of a name store.
class BaseNameStore : public NameStore {
 public:
  explicit BaseNameStore(absl::string_view name);

  absl::StatusOr<NamedObject*> FindName(const ScopeName& lookup_scope,
                                        const ScopedName& scoped_name) override;
  absl::Status AddName(absl::string_view local_name,
                       NamedObject* object) override;
  bool HasName(absl::string_view local_name, bool in_self_only) const override;
  absl::StatusOr<NamedObject*> GetName(absl::string_view local_name,
                                       bool in_self_only) override;
  absl::Status AddChildStore(absl::string_view local_name,
                             NameStore* store) override;
  absl::Status AddOwnedChildStore(absl::string_view local_name,
                                  std::unique_ptr<NameStore> store) override;
  absl::StatusOr<NameStore*> FindChildStore(
      const ScopeName& lookup_scope) override;

  std::vector<std::string> DefinedNames() const override;
  std::string DebugString() const override;
  std::string DebugNames() const override;
  pb::NamedObject ToProtoObject() const override;

 protected:
  absl::flat_hash_map<std::string, NameStore*> child_name_stores_;
  absl::flat_hash_map<std::string, NamedObject*> named_objects_;
  std::vector<std::unique_ptr<NameStore>> owned_stores_;
};

// An implementation that uses an underlying name store
// for implementation.
class WrappedNameStore : public NameStore {
 public:
  WrappedNameStore(absl::string_view name, NameStore* wrapped_store);

  pb::ObjectKind kind() const override;
  const TypeSpec* type_spec() const override;
  absl::StatusOr<NamedObject*> FindName(const ScopeName& lookup_scope,
                                        const ScopedName& scoped_name) override;
  absl::Status AddName(absl::string_view local_name,
                       NamedObject* object) override;
  bool HasName(absl::string_view local_name, bool in_self_only) const override;
  absl::StatusOr<NamedObject*> GetName(absl::string_view local_name,
                                       bool in_self_only) override;
  absl::Status AddChildStore(absl::string_view local_name,
                             NameStore* store) override;
  absl::Status AddOwnedChildStore(absl::string_view local_name,
                                  std::unique_ptr<NameStore> store) override;
  absl::StatusOr<NameStore*> FindChildStore(
      const ScopeName& lookup_scope) override;

  std::vector<std::string> DefinedNames() const override;
  std::string DebugString() const override;
  std::string DebugNames() const override;
  pb::NamedObject ToProtoObject() const override;

 protected:
  NameStore* wrapped_store_;
};

// String we attach to all error messages that are bugs on our side.
inline constexpr absl::string_view kBugNotice = "; This is a bug, pls. report";

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_NAMED_OBJECT_H__
