#ifndef NUDL_ANALYSIS_PYTHON_CONVERTER_H__
#define NUDL_ANALYSIS_PYTHON_CONVERTER_H__

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "nudl/analysis/converter.h"
#include "nudl/analysis/function.h"
#include "nudl/analysis/type_spec.h"

namespace nudl {
namespace analysis {

class PythonConvertState;

class PythonConverter : public Converter {
 public:
  PythonConverter();

 protected:
  absl::StatusOr<std::unique_ptr<ConvertState>> BeginModule(
      Module* module) const override;
  absl::Status ProcessModule(Module* module,
                             ConvertState* state) const override;
  absl::StatusOr<std::string> FinishModule(
      Module* module, std::unique_ptr<ConvertState> state) const override;

  absl::Status ConvertAssignment(const Assignment& expression,
                                 ConvertState* state) const override;
  absl::Status ConvertEmptyStruct(const EmptyStruct& expression,
                                  ConvertState* state) const override;
  absl::Status ConvertLiteral(const Literal& expression,
                              ConvertState* state) const override;
  absl::Status ConvertIdentifier(const Identifier& expression,
                                 ConvertState* state) const override;
  absl::Status ConvertFunctionResult(const FunctionResultExpression& expression,
                                     ConvertState* state) const override;
  absl::Status ConvertArrayDefinition(
      const ArrayDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertMapDefinition(const MapDefinitionExpression& expression,
                                    ConvertState* state) const override;
  absl::Status ConvertIfExpression(const IfExpression& expression,
                                   ConvertState* state) const override;
  absl::Status ConvertExpressionBlock(const ExpressionBlock& expression,
                                      ConvertState* state) const override;
  absl::Status ConvertIndexExpression(const IndexExpression& expression,
                                      ConvertState* state) const override;
  absl::Status ConvertTupleIndexExpression(
      const TupleIndexExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertLambdaExpression(const LambdaExpression& expression,
                                       ConvertState* state) const override;
  absl::Status ConvertDotAccessExpression(const DotAccessExpression& expression,
                                          ConvertState* state) const override;
  absl::Status ConvertFunctionCallExpression(
      const FunctionCallExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertImportStatement(
      const ImportStatementExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertFunctionDefinition(
      const FunctionDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertSchemaDefinition(
      const SchemaDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertTypeDefinition(const TypeDefinitionExpression& expression,
                                     ConvertState* state) const override;

  std::string GetTypeString(const TypeSpec* type_spec,
                            PythonConvertState* state) const;
  absl::Status ConvertFunction(Function* fun, ConvertState* state) const;
  absl::Status ConvertBindings(Function* fun, ConvertState* state) const;
  absl::Status ConvertInlineExpression(const Expression& expression,
                                       PythonConvertState* state) const;
  absl::Status ConvertNativeFunctionCallExpression(
      const FunctionCallExpression& expression, Function* fun,
      PythonConvertState* state) const;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_PYTHON_CONVERTER_H__
