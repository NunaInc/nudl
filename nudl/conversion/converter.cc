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

#include "nudl/conversion/converter.h"

#include <utility>

#include "glog/logging.h"
#include "nudl/proto/analysis.pb.h"
#include "nudl/status/status.h"

namespace nudl {
namespace conversion {

ConvertState::ConvertState(analysis::Module* module)
    : module_(CHECK_NOTNULL(module)) {}

ConvertState::~ConvertState() {}

analysis::Module* ConvertState::module() const { return module_; }

Converter::Converter() {}

Converter::~Converter() {}

absl::StatusOr<std::string> Converter::ConvertModule(
    analysis::Module* module) const {
  ASSIGN_OR_RETURN(auto state, BeginModule(module));
  RETURN_IF_ERROR(ProcessModule(module, state.get()));
  return FinishModule(module, std::move(state));
}

absl::Status Converter::ConvertExpression(
    const analysis::Expression& expression, ConvertState* state) const {
  switch (expression.expr_kind()) {
    case pb::ExpressionKind::EXPR_UNKNOWN:
      return absl::InvalidArgumentError("Unknown expression type generated");
    case pb::ExpressionKind::EXPR_ASSIGNMENT:
      return ConvertAssignment(
          static_cast<const analysis::Assignment&>(expression), state);
    case pb::ExpressionKind::EXPR_EMPTY_STRUCT:
      return ConvertEmptyStruct(
          static_cast<const analysis::EmptyStruct&>(expression), state);
    case pb::ExpressionKind::EXPR_LITERAL:
      return ConvertLiteral(static_cast<const analysis::Literal&>(expression),
                            state);
    case pb::ExpressionKind::EXPR_IDENTIFIER:
      return ConvertIdentifier(
          static_cast<const analysis::Identifier&>(expression), state);
    case pb::ExpressionKind::EXPR_FUNCTION_RESULT:
      return ConvertFunctionResult(
          static_cast<const analysis::FunctionResultExpression&>(expression),
          state);
    case pb::ExpressionKind::EXPR_ARRAY_DEF:
      return ConvertArrayDefinition(
          static_cast<const analysis::ArrayDefinitionExpression&>(expression),
          state);
    case pb::ExpressionKind::EXPR_MAP_DEF:
      return ConvertMapDefinition(
          static_cast<const analysis::MapDefinitionExpression&>(expression),
          state);
    case pb::ExpressionKind::EXPR_IF:
      return ConvertIfExpression(
          static_cast<const analysis::IfExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_BLOCK:
      return ConvertExpressionBlock(
          static_cast<const analysis::ExpressionBlock&>(expression), state);
    case pb::ExpressionKind::EXPR_INDEX:
      return ConvertIndexExpression(
          static_cast<const analysis::IndexExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_TUPLE_INDEX:
      return ConvertTupleIndexExpression(
          static_cast<const analysis::TupleIndexExpression&>(expression),
          state);
    case pb::ExpressionKind::EXPR_LAMBDA:
      return ConvertLambdaExpression(
          static_cast<const analysis::LambdaExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_DOT_ACCESS:
      return ConvertDotAccessExpression(
          static_cast<const analysis::DotAccessExpression&>(expression), state);
    case pb::ExpressionKind::EXPR_FUNCTION_CALL:
      return ConvertFunctionCallExpression(
          static_cast<const analysis::FunctionCallExpression&>(expression),
          state);
    case pb::ExpressionKind::EXPR_IMPORT_STATEMENT:
      return ConvertImportStatement(
          static_cast<const analysis::ImportStatementExpression&>(expression),
          state);
    case pb::ExpressionKind::EXPR_FUNCTION_DEF:
      return ConvertFunctionDefinition(
          static_cast<const analysis::FunctionDefinitionExpression&>(
              expression),
          state);
    case pb::ExpressionKind::EXPR_SCHEMA_DEF:
      return ConvertSchemaDefinition(
          static_cast<const analysis::SchemaDefinitionExpression&>(expression),
          state);
    case pb::ExpressionKind::EXPR_NOP:
      return absl::OkStatus();
    case pb::ExpressionKind::EXPR_TYPE_DEFINITION:
      return ConvertTypeDefinition(
          static_cast<const analysis::TypeDefinitionExpression&>(expression),
          state);
  }
  return absl::InvalidArgumentError(
      "Cannot determine the type of the generated expression");
}

}  // namespace conversion
}  // namespace nudl
