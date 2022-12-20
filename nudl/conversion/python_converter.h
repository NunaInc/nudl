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

#ifndef NUDL_CONVERSION_PYTHON_CONVERTER_H__
#define NUDL_CONVERSION_PYTHON_CONVERTER_H__

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "nudl/analysis/analysis.h"
#include "nudl/conversion/converter.h"

namespace nudl {
namespace conversion {

class PythonConvertState;

class PythonConverter : public Converter {
 public:
  explicit PythonConverter(bool bindings_on_use = false);

 protected:
  absl::StatusOr<std::unique_ptr<ConvertState>> BeginModule(
      analysis::Module* module) const override;
  absl::Status ProcessModule(analysis::Module* module,
                             ConvertState* state) const override;
  absl::StatusOr<ConversionResult> FinishModule(
      analysis::Module* module,
      std::unique_ptr<ConvertState> state) const override;

  absl::Status ConvertAssignment(const analysis::Assignment& expression,
                                 ConvertState* state) const override;
  absl::Status ConvertEmptyStruct(const analysis::EmptyStruct& expression,
                                  ConvertState* state) const override;
  absl::Status ConvertLiteral(const analysis::Literal& expression,
                              ConvertState* state) const override;
  absl::Status ConvertIdentifier(const analysis::Identifier& expression,
                                 ConvertState* state) const override;
  absl::Status ConvertFunctionResult(
      const analysis::FunctionResultExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertArrayDefinition(
      const analysis::ArrayDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertMapDefinition(
      const analysis::MapDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertTupleDefinition(
      const analysis::TupleDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertIfExpression(const analysis::IfExpression& expression,
                                   ConvertState* state) const override;
  absl::Status ConvertExpressionBlock(
      const analysis::ExpressionBlock& expression,
      ConvertState* state) const override;
  absl::Status ConvertIndexExpression(
      const analysis::IndexExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertTupleIndexExpression(
      const analysis::TupleIndexExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertLambdaExpression(
      const analysis::LambdaExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertDotAccessExpression(
      const analysis::DotAccessExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertFunctionCallExpression(
      const analysis::FunctionCallExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertImportStatement(
      const analysis::ImportStatementExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertFunctionDefinition(
      const analysis::FunctionDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertSchemaDefinition(
      const analysis::SchemaDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertTypeDefinition(
      const analysis::TypeDefinitionExpression& expression,
      ConvertState* state) const override;

  std::string GetTypeString(const analysis::TypeSpec* type_spec,
                            PythonConvertState* state) const;
  absl::StatusOr<bool> ConvertFunction(analysis::Function* fun, bool is_on_use,
                                       ConvertState* state) const;
  absl::Status ConvertFunctionGroup(analysis::FunctionGroup* group,
                                    PythonConvertState* state) const;
  absl::StatusOr<bool> ConvertBindings(analysis::Function* fun,
                                       PythonConvertState* state) const;
  absl::Status ConvertStructType(const analysis::TypeStruct* ts,
                                 PythonConvertState* state) const;
  absl::Status ConvertInlineExpression(
      const analysis::Expression& expression, PythonConvertState* state,
      absl::optional<std::string*> str = {}) const;
  absl::Status ConvertNativeFunctionCallExpression(
      const analysis::FunctionCallExpression& expression,
      analysis::Function* fun, PythonConvertState* state) const;
  std::string GetStructTypeName(const analysis::TypeSpec* type_spec,
                                bool force_name,
                                PythonConvertState* state) const;
  absl::Status AddTypeName(const analysis::TypeSpec* type_spec,
                           bool force_struct_name,
                           PythonConvertState* state) const;
  std::string DefaultFieldFactory(const analysis::TypeSpec* type_spec,
                                  PythonConvertState* state) const;

  absl::StatusOr<std::string> LocalFunctionName(analysis::Function* fun,
                                                bool is_on_use,
                                                ConvertState* state) const;
  absl::StatusOr<std::string> ConvertMainFunction(
      analysis::Function* fun, PythonConvertState* state) const;
  absl::Status ProcessFieldUsageMacro(PythonConvertState* state,
                                      analysis::FunctionBinding* binding) const;

  absl::StatusOr<
      absl::flat_hash_map<std::string, std::unique_ptr<ConvertState>>>
  ProcessMacros(const absl::flat_hash_set<std::string>& macros,
                analysis::Scope* scope, analysis::FunctionBinding* binding,
                analysis::Function* fun, ConvertState* state) const;
  absl::Status ProcessSeedMacro(PythonConvertState* state,
                                const analysis::TypeSpec* result_type,
                                bool is_dataset_seed) const;

  // If we define function bindings where they are used, as opposed
  // to where they are defined.
  const bool bindings_on_use_;
};

// Changes the possible composed name, to a 'python_safe' version.
std::string PythonSafeName(absl::string_view name,
                           absl::optional<analysis::NamedObject*> object);
// If provided basic name is a python builtin function or standard module.
// We rename thes for everything beside fields.
bool IsPythonBuiltin(absl::string_view name);
// If the provided name is a keyword. We rename these always.
bool IsPythonKeyword(absl::string_view name);
// If the provided name is a normal python __ function. E.g.
//  __init__, __eq__ etc.keyword. We rename these always.
bool IsPythonSpecialName(absl::string_view name);
// Termination we add to all conflicting names in python.
inline constexpr absl::string_view kPythonRenameEnding = "__nudl";
}  // namespace conversion
}  // namespace nudl

#endif  // NUDL_CONVERSION_PYTHON_CONVERTER_H__
