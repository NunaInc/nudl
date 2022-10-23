#include "nudl/analysis/converter.h"

#include <utility>

#include "glog/logging.h"
#include "nudl/proto/analysis.pb.h"
#include "nudl/status/status.h"

namespace nudl {
namespace analysis {

ConvertState::ConvertState(Module* module) : module_(CHECK_NOTNULL(module)) {}

ConvertState::~ConvertState() {}

Module* ConvertState::module() const { return module_; }

Converter::Converter() {}

Converter::~Converter() {}

absl::StatusOr<std::string> Converter::ConvertModule(Module* module) const {
  ASSIGN_OR_RETURN(auto state, BeginModule(module));
  RETURN_IF_ERROR(ProcessModule(module, state.get()));
  return FinishModule(module, std::move(state));
}

absl::Status Converter::ConvertExpression(const Expression& expression,
                                          ConvertState* state) const {
  switch (expression.expr_kind()) {
    case pb::ExpressionKind::EXPR_UNKNOWN:
      return absl::InvalidArgumentError("Unknown expression type generated");
    case pb::ExpressionKind::EXPR_ASSIGNMENT:
      return ConvertAssignment(static_cast<const Assignment&>(expression),
                               state);
    case pb::ExpressionKind::EXPR_EMPTY_STRUCT:
      return ConvertEmptyStruct(static_cast<const EmptyStruct&>(expression),
                                state);
    case pb::ExpressionKind::EXPR_LITERAL:
      return ConvertLiteral(static_cast<const Literal&>(expression), state);
    case pb::ExpressionKind::EXPR_IDENTIFIER:
      return ConvertIdentifier(static_cast<const Identifier&>(expression),
                               state);
    case pb::ExpressionKind::EXPR_FUNCTION_RESULT:
      return ConvertFunctionResult(
          static_cast<const FunctionResultExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_ARRAY_DEF:
      return ConvertArrayDefinition(
          static_cast<const ArrayDefinitionExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_MAP_DEF:
      return ConvertMapDefinition(
          static_cast<const MapDefinitionExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_IF:
      return ConvertIfExpression(static_cast<const IfExpression&>(expression),
                                 state);
    case pb::ExpressionKind::EXPR_BLOCK:
      return ConvertExpressionBlock(
          static_cast<const ExpressionBlock&>(expression), state);
    case pb::ExpressionKind::EXPR_INDEX:
      return ConvertIndexExpression(
          static_cast<const IndexExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_TUPLE_INDEX:
      return ConvertTupleIndexExpression(
          static_cast<const TupleIndexExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_LAMBDA:
      return ConvertLambdaExpression(
          static_cast<const LambdaExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_DOT_ACCESS:
      return ConvertDotAccessExpression(
          static_cast<const DotAccessExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_FUNCTION_CALL:
      return ConvertFunctionCallExpression(
          static_cast<const FunctionCallExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_IMPORT_STATEMENT:
      return ConvertImportStatement(
          static_cast<const ImportStatementExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_FUNCTION_DEF:
      return ConvertFunctionDefinition(
          static_cast<const FunctionDefinitionExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_SCHEMA_DEF:
      return ConvertSchemaDefinition(
          static_cast<const SchemaDefinitionExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_NOP:
      return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      "Cannot determine the type of the generated expression");
}

}  // namespace analysis
}  // namespace nudl
