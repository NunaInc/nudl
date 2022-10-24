#ifndef NUDL_ANALYSIS_CONVERTER_H__
#define NUDL_ANALYSIS_CONVERTER_H__

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nudl/analysis/expression.h"
#include "nudl/analysis/module.h"

namespace nudl {
namespace analysis {

class ConvertState {
 public:
  explicit ConvertState(Module* module);
  virtual ~ConvertState();

  Module* module() const;

 protected:
  Module* module_;
};

class Converter {
 public:
  Converter();
  virtual ~Converter();

  absl::StatusOr<std::string> ConvertModule(Module* module) const;

 protected:
  virtual absl::StatusOr<std::unique_ptr<ConvertState>> BeginModule(
      Module* module) const = 0;
  virtual absl::Status ProcessModule(Module* module,
                                     ConvertState* state) const = 0;
  virtual absl::StatusOr<std::string> FinishModule(
      Module* module, std::unique_ptr<ConvertState> state) const = 0;

  virtual absl::Status ConvertExpression(const Expression& expression,
                                         ConvertState* state) const;

  // Individual expression conversion:
  virtual absl::Status ConvertAssignment(const Assignment& expression,
                                         ConvertState* state) const = 0;
  virtual absl::Status ConvertEmptyStruct(const EmptyStruct& expression,
                                          ConvertState* state) const = 0;
  virtual absl::Status ConvertLiteral(const Literal& expression,
                                      ConvertState* state) const = 0;
  virtual absl::Status ConvertIdentifier(const Identifier& expression,
                                         ConvertState* state) const = 0;
  virtual absl::Status ConvertFunctionResult(
      const FunctionResultExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertArrayDefinition(
      const ArrayDefinitionExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertMapDefinition(
      const MapDefinitionExpression& expression, ConvertState* state) const = 0;
  virtual absl::Status ConvertIfExpression(const IfExpression& expression,
                                           ConvertState* state) const = 0;
  virtual absl::Status ConvertExpressionBlock(const ExpressionBlock& expression,
                                              ConvertState* state) const = 0;
  virtual absl::Status ConvertIndexExpression(const IndexExpression& expression,
                                              ConvertState* state) const = 0;
  virtual absl::Status ConvertTupleIndexExpression(
      const TupleIndexExpression& expression, ConvertState* state) const = 0;
  virtual absl::Status ConvertLambdaExpression(
      const LambdaExpression& expression, ConvertState* state) const = 0;
  virtual absl::Status ConvertDotAccessExpression(
      const DotAccessExpression& expression, ConvertState* state) const = 0;
  virtual absl::Status ConvertFunctionCallExpression(
      const FunctionCallExpression& expression, ConvertState* state) const = 0;
  virtual absl::Status ConvertImportStatement(
      const ImportStatementExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertFunctionDefinition(
      const FunctionDefinitionExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertSchemaDefinition(
      const SchemaDefinitionExpression& expression,
      ConvertState* state) const = 0;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_CONVERTER_H__
