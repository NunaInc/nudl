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

#include "nudl/grammar/tree_builder.h"

#include <string>
#include <utility>
#include <vector>

#include "NudlDslParser.h"
#include "NudlDslParserBaseVisitor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "antlr4-runtime.h"  // NOLINT
#include "glog/logging.h"
#include "nudl/grammar/tree_util.h"
#include "nudl/proto/dsl.pb.h"
#include "re2/re2.h"

namespace nudl {
namespace grammar {

namespace {
class DslVisitor : public NudlDslParserBaseVisitor {
 private:
  const antlr4::Parser* parser_ = nullptr;
  absl::string_view code_;
  VisitorOptions options_;

 public:
  DslVisitor(const antlr4::Parser* parser, absl::string_view code,
             VisitorOptions options)
      : NudlDslParserBaseVisitor(),
        parser_(CHECK_NOTNULL(parser)),
        code_(code),
        options_(std::move(options)) {}

  std::string FillInterval(const antlr4::tree::ParseTree& pt,
                           pb::CodeInterval* interval) const {
    *interval = TreeUtil::GetInterval(pt);
    if (options_.no_interval_positions) {
      interval->mutable_begin()->clear_position();
      interval->mutable_end()->clear_position();
    }
    return std::string(
        TreeUtil::CodeSnippet(code_, interval->begin(), interval->end()));
  }

  pb::Expression emptyExpression(const antlr4::tree::ParseTree& pt) const {
    pb::Expression expression;
    if (!code_.empty() && !options_.no_intervals) {
      expression.set_code(FillInterval(pt, expression.mutable_code_interval()));
    }
    return expression;
  }
  std::any setError(pb::Expression* expression, antlr4::tree::ParseTree* pt,
                    absl::string_view message) const {
    auto error = expression->mutable_error();
    error->set_description(std::string(message));
    return std::make_any<pb::Expression>(std::move(*expression));
  }
  template <class Context>
  pb::Expression computeExpression(Context* context) {
    return std::any_cast<pb::Expression>(visit(context));
  }
  pb::ExpressionBlock expressionBlock(
      NudlDslParser::ExpressionBlockContext* context) {
    return std::any_cast<pb::ExpressionBlock>(visit(context));
  }
  pb::Identifier identifier(NudlDslParser::ComposedIdentifierContext* context) {
    return std::any_cast<pb::Identifier>(visit(context));
  }
  pb::FunctionParameter paramDefinition(
      NudlDslParser::ParamDefinitionContext* context) {
    return std::any_cast<pb::FunctionParameter>(visit(context));
  }
  template <class Context>
  pb::TypeSpec typeSpec(Context* context) {
    return std::any_cast<pb::TypeSpec>(visit(context));
  }

  std::any visitLiteral(NudlDslParser::LiteralContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    auto literal = expression.mutable_literal();
    literal->set_original(TreeUtil::Recompose(context));
    if (context->KW_NULL()) {
      literal->set_null_value(pb::NullType::NULL_VALUE);
    } else if (context->KW_TRUE()) {
      literal->set_bool_value(true);
    } else if (context->KW_FALSE()) {
      literal->set_bool_value(false);
    } else if (context->LITERAL_DECIMAL()) {
      uint64_t value;
      if (!absl::SimpleAtoi(literal->original(), &value)) {
        return setError(&expression, context, "Invalid decimal literal");
      }
      literal->set_int_value(value);
    } else if (context->LITERAL_UNSIGNED_DECIMAL()) {
      uint64_t value;
      if (!absl::SimpleAtoi(
              absl::StripSuffix(absl::StripSuffix(literal->original(), "u"),
                                "U"),
              &value)) {
        return setError(&expression, context, "Invalid decimal literal");
      }
      literal->set_uint_value(value);
    } else if (context->LITERAL_HEXADECIMAL()) {
      absl::string_view value_str =
          absl::StripPrefix(absl::StripPrefix(literal->original(), "0x"), "0X");
      uint64_t value;
      if (!absl::SimpleHexAtoi(value_str, &value)) {
        return setError(&expression, context, "Invalid hexadecimal literal");
      }
      literal->set_int_value(value);
    } else if (context->LITERAL_UNSIGNED_HEXADECIMAL()) {
      absl::string_view value_str = absl::StripSuffix(
          absl::StripSuffix(
              absl::StripPrefix(absl::StripPrefix(literal->original(), "0x"),
                                "0X"),
              "u"),
          "U");
      uint64_t value;
      if (!absl::SimpleHexAtoi(value_str, &value)) {
        return setError(&expression, context, "Invalid hexadecimal literal");
      }
      literal->set_uint_value(value);
    } else if (context->LITERAL_FLOAT()) {
      float value;
      if (!absl::SimpleAtof(absl::StripSuffix(literal->original(), "f"),
                            &value)) {
        return setError(&expression, context, "Invalid float literal");
      }
      literal->set_float_value(value);
    } else if (context->LITERAL_DOUBLE()) {
      double value;
      if (!absl::SimpleAtod(literal->original(), &value)) {
        return setError(&expression, context, "Invalid double literal");
      }
      literal->set_double_value(value);
    } else if (context->LITERAL_STRING()) {
      absl::string_view value_str =
          absl::StripSuffix(absl::StripPrefix(literal->original(), "\""), "\"");
      std::string error;
      if (!absl::CUnescape(value_str, literal->mutable_str_value(), &error)) {
        return setError(&expression, context,
                        absl::StrCat("Invalid string literal: ", error));
      }
    } else if (context->LITERAL_BYTES()) {
      absl::string_view value_str = absl::StripSuffix(
          absl::StripPrefix(literal->original(), "b\""), "\"");
      std::string error;
      if (!absl::CUnescape(value_str, literal->mutable_bytes_value(), &error)) {
        return setError(&expression, context,
                        absl::StrCat("Invalid bytes literal: ", error));
      }
    } else if (!context->LITERAL_TIMERANGE().empty()) {
      absl::Duration duration;
      for (auto token : context->LITERAL_TIMERANGE()) {
        const std::string text = token->toString();
        size_t value = 0;
        std::string unit;
        re2::StringPiece input(text);
        if (!RE2::Consume(&input, "(\\d+)(\\w+)", &value, &unit)) {
          return setError(&expression, context, "invalid timerange format");
        }
        if (unit == "seconds") {
          duration += absl::Seconds(value);
        } else if (unit == "minutes") {
          duration += absl::Minutes(value);
        } else if (unit == "hours") {
          duration += absl::Hours(value);
        } else if (unit == "days") {
          duration += absl::Hours(value * 24);
        } else if (unit == "weeks") {
          duration += absl::Hours(value * 24 * 7);
        } else {
          return setError(&expression, context,
                          absl::StrCat("Unknown unit: ", unit));
        }
      }
      literal->mutable_time_range()->set_seconds(
          absl::ToInt64Seconds(duration));
    }
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitEmptyStruct(
      NudlDslParser::EmptyStructContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    expression.set_empty_struct(pb::NullType::NULL_VALUE);
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitArrayDefinition(
      NudlDslParser::ArrayDefinitionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    CHECK_NOTNULL(context->computeExpressions());
    auto array_def = expression.mutable_array_def();
    for (auto expression : context->computeExpressions()->computeExpression()) {
      *array_def->add_element() = computeExpression(expression);
    }
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitMapDefinition(
      NudlDslParser::MapDefinitionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    CHECK_NOTNULL(context->mapElements());
    auto map_def = expression.mutable_map_def();
    for (auto element : context->mapElements()->mapElement()) {
      auto keyval = element->computeExpression();
      CHECK_EQ(keyval.size(), 2ul);
      auto elem = map_def->add_element();
      *elem->mutable_key() = computeExpression(keyval[0]);
      *elem->mutable_value() = computeExpression(keyval[1]);
    }
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitNamedTupleDefinition(
      NudlDslParser::NamedTupleDefinitionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    CHECK_NOTNULL(context->namedTupleElements());
    auto tuple_def = expression.mutable_tuple_def();
    for (auto element : context->namedTupleElements()->namedTupleElement()) {
      auto elem = tuple_def->add_element();
      elem->set_name(TreeUtil::Recompose(CHECK_NOTNULL(element->IDENTIFIER())));
      *elem->mutable_value() =
          computeExpression(CHECK_NOTNULL(element->computeExpression()));
      if (element->typeAssignment()) {
        *elem->mutable_type_spec() = typeSpec(
            CHECK_NOTNULL(element->typeAssignment()->typeExpression()));
      }
    }
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitComposedIdentifier(
      NudlDslParser::ComposedIdentifierContext* context) override {
    pb::Identifier identifier;
    identifier.add_name(
        TreeUtil::Recompose(CHECK_NOTNULL(context->IDENTIFIER())));
    for (auto dot : context->dotIdentifier()) {
      identifier.add_name(
          TreeUtil::Recompose(CHECK_NOTNULL(dot->IDENTIFIER())));
    }
    return std::make_any<pb::Identifier>(std::move(identifier));
  }

  std::any visitIfExpression(
      NudlDslParser::IfExpressionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    auto if_expr = expression.mutable_if_expr();
    *if_expr->add_condition() =
        computeExpression(CHECK_NOTNULL(context->computeExpression()));
    *if_expr->add_expression_block() =
        expressionBlock(CHECK_NOTNULL(context->expressionBlock()));
    if (context->elseExpression()) {
      *if_expr->add_expression_block() = expressionBlock(
          CHECK_NOTNULL(context->elseExpression()->expressionBlock()));
    } else {
      auto crt_elif = context->elifExpression();
      while (crt_elif) {
        *if_expr->add_condition() =
            computeExpression(CHECK_NOTNULL(crt_elif->computeExpression()));
        *if_expr->add_expression_block() =
            expressionBlock(CHECK_NOTNULL(crt_elif->expressionBlock()));
        if (crt_elif->elseExpression()) {
          *if_expr->add_expression_block() = expressionBlock(
              CHECK_NOTNULL(crt_elif->elseExpression()->expressionBlock()));
          break;
        }
        crt_elif = crt_elif->elifExpression();
      }
    }
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitWithExpression(
      NudlDslParser::WithExpressionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    auto with_expr = expression.mutable_with_expr();

    *with_expr->mutable_with() =
        computeExpression(CHECK_NOTNULL(context->computeExpression()));
    *with_expr->mutable_expression_block() = std::any_cast<pb::ExpressionBlock>(
        visit(CHECK_NOTNULL(context->expressionBlock())));
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitReturnExpression(
      NudlDslParser::ReturnExpressionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    *expression.mutable_return_expr() =
        computeExpression(CHECK_NOTNULL(context->computeExpression()));
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitYieldExpression(
      NudlDslParser::YieldExpressionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    *expression.mutable_yield_expr() =
        computeExpression(CHECK_NOTNULL(context->computeExpression()));
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitPassExpression(
      NudlDslParser::PassExpressionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    expression.set_pass_expr(pb::NullType::NULL_VALUE);
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitTypeAssignment(
      NudlDslParser::TypeAssignmentContext* context) override {
    return visitTypeExpression(CHECK_NOTNULL(context->typeExpression()));
  }

  std::any visitTypeExpression(
      NudlDslParser::TypeExpressionContext* context) override {
    pb::TypeSpec type_spec;
    if (context->typeNamedArgument()) {
      type_spec.set_is_local_type(true);
      type_spec.mutable_identifier()->add_name(TreeUtil::Recompose(
          CHECK_NOTNULL(context->typeNamedArgument()->IDENTIFIER())));
      if (context->typeNamedArgument()->typeAssignment()) {
        *type_spec.add_argument()->mutable_type_spec() =
            typeSpec(context->typeNamedArgument()->typeAssignment());
      }
      return std::make_any<pb::TypeSpec>(std::move(type_spec));
    }
    *type_spec.mutable_identifier() =
        identifier(CHECK_NOTNULL(context->composedIdentifier()));
    if (context->typeTemplate()) {
      for (auto arg : context->typeTemplate()->typeTemplateArgument()) {
        if (arg->typeExpression()) {
          *type_spec.add_argument()->mutable_type_spec() =
              typeSpec(arg->typeExpression());
        } else if (arg->LITERAL_DECIMAL()) {
          uint64_t value;
          if (absl::SimpleAtoi(TreeUtil::Recompose(arg->LITERAL_DECIMAL()),
                               &value)) {
            type_spec.add_argument()->set_int_value(value);
          }
        }
      }
    }
    return std::make_any<pb::TypeSpec>(std::move(type_spec));
  }

  pb::Assignment buildAssignment(
      NudlDslParser::AssignExpressionContext* context) {
    pb::Assignment assign;
    *assign.mutable_identifier() =
        identifier(CHECK_NOTNULL(context->composedIdentifier()));
    if (context->typeAssignment()) {
      *assign.mutable_type_spec() = typeSpec(context->typeAssignment());
    }
    *assign.mutable_value() =
        computeExpression(CHECK_NOTNULL(context->computeExpression()));
    return assign;
  }

  std::any visitAssignExpression(
      NudlDslParser::AssignExpressionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    *expression.mutable_assignment() = buildAssignment(context);
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitParamDefinition(
      NudlDslParser::ParamDefinitionContext* context) override {
    pb::FunctionParameter param;
    param.set_name(TreeUtil::Recompose(CHECK_NOTNULL(context->IDENTIFIER())));
    if (context->typeAssignment()) {
      *param.mutable_type_spec() = typeSpec(context->typeAssignment());
    }
    if (context->computeExpression()) {
      *param.mutable_default_value() =
          computeExpression(context->computeExpression());
    }
    return std::make_any<pb::FunctionParameter>(std::move(param));
  }

  std::any visitInlineBody(NudlDslParser::InlineBodyContext* context) override {
    pb::NativeSnippet snippet;
    std::string body_str(absl::StripPrefix(
        absl::StripSuffix(CHECK_NOTNULL(context->INLINE_BODY())->toString(),
                          "[[end]]"),
        "[["));
    re2::StringPiece native_body(body_str);
    std::string inline_name;
    if (RE2::Consume(&native_body, R"(([a-zA-Z_][a-zA-Z0-9_]*)\]\])",
                     &inline_name)) {
      snippet.set_name(std::move(inline_name));
      snippet.set_body(std::string(
          absl::StripSuffix(absl::StripPrefix(native_body, "\n"), "\n")));
    } else {
      snippet.set_body(std::string(
          absl::StripSuffix(absl::StripPrefix(body_str, "\n"), "\n")));
    }
    return std::make_any<pb::NativeSnippet>(std::move(snippet));
  }

  std::any visitFunctionDefinition(
      NudlDslParser::FunctionDefinitionContext* context) override {
    pb::FunctionDefinition fundef;
    fundef.set_name(TreeUtil::Recompose(CHECK_NOTNULL(context->IDENTIFIER())));
    if (context->functionAnnotation()) {
      if (context->functionAnnotation()->KW_METHOD()) {
        fundef.set_fun_type(pb::FunctionType::FUN_METHOD);
      } else if (context->functionAnnotation()->KW_CONSTRUCTOR()) {
        fundef.set_fun_type(pb::FunctionType::FUN_CONSTRUCTOR);
      }
    }
    if (context->expressionBlock()) {
      *fundef.mutable_expression_block() =
          expressionBlock(CHECK_NOTNULL(context->expressionBlock()));
    } else {
      for (auto body : context->inlineBody()) {
        *fundef.add_snippet() = std::any_cast<pb::NativeSnippet>(visit(body));
      }
    }
    if (context->typeAssignment()) {
      *fundef.mutable_result_type() = typeSpec(context->typeAssignment());
    }
    if (context->paramsList()) {
      for (auto param : context->paramsList()->paramDefinition()) {
        *fundef.add_param() = paramDefinition(param);
      }
    }
    return std::make_any<pb::FunctionDefinition>(std::move(fundef));
  }

  void setArgumentList(NudlDslParser::ArgumentListContext* context,
                       pb::FunctionCall* funcall) {
    if (!context) {
      return;
    }
    for (auto arg : context->argumentSpec()) {
      auto farg = funcall->add_argument();
      if (arg->IDENTIFIER()) {
        farg->set_name(TreeUtil::Recompose(arg->IDENTIFIER()));
      }
      *farg->mutable_value() =
          computeExpression(CHECK_NOTNULL(arg->computeExpression()));
    }
  }

  std::any visitFunctionCall(
      NudlDslParser::FunctionCallContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    auto funcall = expression.mutable_function_call();
    CHECK_NOTNULL(context->functionName());
    if (context->functionName()->composedIdentifier()) {
      *funcall->mutable_identifier() =
          identifier(context->functionName()->composedIdentifier());
    } else if (context->functionName()->typeExpression()) {
      *funcall->mutable_type_spec() =
          typeSpec(context->functionName()->typeExpression());
    }
    setArgumentList(context->argumentList(), funcall);
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitLambdaExpression(
      NudlDslParser::LambdaExpressionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    auto lambda = expression.mutable_lambda_def();
    for (auto param : context->paramDefinition()) {
      *lambda->add_param() = paramDefinition(param);
    }
    if (context->typeAssignment()) {
      *lambda->mutable_result_type() = typeSpec(context->typeAssignment());
    }
    *lambda->mutable_expression_block() =
        expressionBlock(CHECK_NOTNULL(context->expressionBlock()));
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitParenthesisedExpression(
      NudlDslParser::ParenthesisedExpressionContext* context) override {
    return visitComputeExpression(context->computeExpression());
  }

  std::any visitPrimaryExpression(
      NudlDslParser::PrimaryExpressionContext* context) override {
    if (context->composedIdentifier()) {
      pb::Expression expression(emptyExpression(*context));
      *expression.mutable_identifier() =
          identifier(context->composedIdentifier());
      return std::make_any<pb::Expression>(std::move(expression));
    }
    return visitChildren(context);
  }

  std::any visitPostfixExpression(
      NudlDslParser::PostfixExpressionContext* context) override {
    pb::Expression primary_expression = std::any_cast<pb::Expression>(
        visit(CHECK_NOTNULL(context->primaryExpression())));
    for (auto postfix : context->postfixValue()) {
      pb::Expression expression(emptyExpression(*context));
      if (postfix->LBRACKET()) {
        auto index_expr = expression.mutable_index_expr();
        *index_expr->mutable_object() = std::move(primary_expression);
        *index_expr->mutable_index() =
            computeExpression(CHECK_NOTNULL(postfix->computeExpression()));
      } else if (postfix->LPAREN()) {
        auto funcall = expression.mutable_function_call();
        *funcall->mutable_expr_spec() = std::move(primary_expression);
        setArgumentList(postfix->argumentList(), funcall);
      } else if (postfix->DOT()) {
        auto dot_expr = expression.mutable_dot_expr();
        *dot_expr->mutable_left() = std::move(primary_expression);
        if (postfix->IDENTIFIER()) {
          dot_expr->set_name(TreeUtil::Recompose(postfix->IDENTIFIER()));
        } else {
          *dot_expr->mutable_function_call() = std::move(
              *std::any_cast<pb::Expression>(visit(postfix->functionCall()))
                   .mutable_function_call());
        }
      } else {
        break;
      }
      primary_expression = std::move(expression);
    }
    return std::make_any<pb::Expression>(std::move(primary_expression));
  }

  template <class T, class O>
  class OperatorBuilder {
   private:
    antlr4::tree::ParseTree* pt_;
    std::vector<T*> exprs_;
    std::vector<O*> opers_;
    DslVisitor* visitor_;

   public:
    OperatorBuilder(antlr4::tree::ParseTree* pt, std::vector<T*> exprs,
                    std::vector<O*> opers, DslVisitor* visitor)
        : pt_(pt),
          exprs_(std::move(exprs)),
          opers_(std::move(opers)),
          visitor_(visitor) {}
    OperatorBuilder(antlr4::tree::ParseTree* pt, std::vector<T*> exprs, O* oper,
                    DslVisitor* visitor)
        : pt_(pt), exprs_(std::move(exprs)), visitor_(visitor) {
      if (oper) {
        opers_.push_back(oper);
      }
    }
    OperatorBuilder(antlr4::tree::ParseTree* pt, T* expr, O* oper,
                    DslVisitor* visitor)
        : pt_(pt), exprs_(1, CHECK_NOTNULL(expr)), visitor_(visitor) {
      if (oper) {
        opers_.push_back(oper);
      }
    }
    std::any visit() {
      CHECK(!exprs_.empty());
      if (opers_.empty()) {
        CHECK_EQ(exprs_.size(), 1ul);
        return visitor_->visit(exprs_.front());
      }
      pb::Expression expression(visitor_->emptyExpression(*pt_));
      auto oper = expression.mutable_operator_expr();
      for (auto expr : exprs_) {
        *oper->add_argument() =
            std::any_cast<pb::Expression>(visitor_->visit(expr));
      }
      for (auto oper_node : opers_) {
        oper->add_op(TreeUtil::Recompose(oper_node));
      }
      return std::make_any<pb::Expression>(std::move(expression));
    }
  };

  std::any visitUnaryOperatorExpression(
      NudlDslParser::UnaryOperatorExpressionContext* context) override {
    return OperatorBuilder(context, context->postfixExpression(),
                           context->unaryOperator(), this)
        .visit();
  }

  std::any visitMultiplicativeExpression(
      NudlDslParser::MultiplicativeExpressionContext* context) override {
    return OperatorBuilder(context, context->unaryOperatorExpression(),
                           context->multiplicativeOperator(), this)
        .visit();
  }

  std::any visitAdditiveExpression(
      NudlDslParser::AdditiveExpressionContext* context) override {
    return OperatorBuilder(context, context->multiplicativeExpression(),
                           context->additiveOperator(), this)
        .visit();
  }

  std::any visitShiftExpression(
      NudlDslParser::ShiftExpressionContext* context) override {
    return OperatorBuilder(context, context->additiveExpression(),
                           context->shiftOperator(), this)
        .visit();
  }

  std::any visitRelationalExpression(
      NudlDslParser::RelationalExpressionContext* context) override {
    return OperatorBuilder(context, context->shiftExpression(),
                           context->relationalOperator(), this)
        .visit();
  }

  std::any visitEqualityExpression(
      NudlDslParser::EqualityExpressionContext* context) override {
    return OperatorBuilder(context, context->relationalExpression(),
                           context->equalityOperator(), this)
        .visit();
  }

  std::any visitAndExpression(
      NudlDslParser::AndExpressionContext* context) override {
    return OperatorBuilder(context, context->equalityExpression(),
                           context->AMPERSAND(), this)
        .visit();
  }

  std::any visitXorExpression(
      NudlDslParser::XorExpressionContext* context) override {
    return OperatorBuilder(context, context->andExpression(), context->CARET(),
                           this)
        .visit();
  }

  std::any visitOrExpression(
      NudlDslParser::OrExpressionContext* context) override {
    return OperatorBuilder(context, context->xorExpression(), context->VBAR(),
                           this)
        .visit();
  }

  std::any visitBetweenExpression(
      NudlDslParser::BetweenExpressionContext* context) override {
    return OperatorBuilder(context, context->orExpression(),
                           context->KW_BETWEEN(), this)
        .visit();
  }

  std::any visitInExpression(
      NudlDslParser::InExpressionContext* context) override {
    return OperatorBuilder(context, context->betweenExpression(),
                           context->KW_IN(), this)
        .visit();
  }

  std::any visitLogicalAndExpression(
      NudlDslParser::LogicalAndExpressionContext* context) override {
    return OperatorBuilder(context, context->inExpression(), context->KW_AND(),
                           this)
        .visit();
  }

  std::any visitLogicalXorExpression(
      NudlDslParser::LogicalXorExpressionContext* context) override {
    return OperatorBuilder(context, context->logicalAndExpression(),
                           context->KW_XOR(), this)
        .visit();
  }

  std::any visitLogicalOrExpression(
      NudlDslParser::LogicalOrExpressionContext* context) override {
    return OperatorBuilder(context, context->logicalXorExpression(),
                           context->KW_OR(), this)
        .visit();
  }

  std::any visitConditionalExpression(
      NudlDslParser::ConditionalExpressionContext* context) override {
    return OperatorBuilder(context, context->logicalOrExpression(),
                           context->QUESTION(), this)
        .visit();
  }

  std::any visitComputeExpressions(
      NudlDslParser::ComputeExpressionsContext* context) override {
    pb::ExpressionBlock block;
    for (auto expr : context->computeExpression()) {
      *block.add_expression() = computeExpression(expr);
    }
    return std::make_any<pb::ExpressionBlock>(std::move(block));
  }

  std::any visitExpressionBlock(
      NudlDslParser::ExpressionBlockContext* context) override {
    if (context->blockBody()) {
      return visit(context->blockBody());
    }
    pb::ExpressionBlock block;
    *block.add_expression() =
        computeExpression(CHECK_NOTNULL(context->blockElement()));
    return std::make_any<pb::ExpressionBlock>(std::move(block));
  }

  std::any visitBlockBody(NudlDslParser::BlockBodyContext* context) override {
    pb::ExpressionBlock block;
    for (auto expr : context->blockElement()) {
      *block.add_expression() = computeExpression(expr);
    }
    return std::make_any<pb::ExpressionBlock>(std::move(block));
  }

  std::any visitSchemaDefinition(
      NudlDslParser::SchemaDefinitionContext* context) override {
    pb::SchemaDefinition schema;
    schema.set_name(TreeUtil::Recompose(CHECK_NOTNULL(context->IDENTIFIER())));
    if (context->fieldsDefinition()) {
      for (auto field_def : context->fieldsDefinition()->fieldDefinition()) {
        *schema.add_field() =
            std::any_cast<pb::SchemaDefinition::Field>(visit(field_def));
      }
    }
    return std::make_any<pb::SchemaDefinition>(std::move(schema));
  }

  std::any visitFieldDefinition(
      NudlDslParser::FieldDefinitionContext* context) override {
    pb::SchemaDefinition::Field field;
    field.set_name(TreeUtil::Recompose(CHECK_NOTNULL(context->IDENTIFIER())));
    *field.mutable_type_spec() =
        typeSpec(CHECK_NOTNULL(context->typeAssignment()));
    if (context->fieldOptions()) {
      for (auto option : context->fieldOptions()->fieldOption()) {
        auto pb_option = field.add_field_option();
        pb_option->set_name(
            TreeUtil::Recompose(CHECK_NOTNULL(option->IDENTIFIER())));
        *pb_option->mutable_value() =
            computeExpression(CHECK_NOTNULL(option->literal()));
      }
    }
    return std::make_any<pb::SchemaDefinition::Field>(std::move(field));
  }

  std::any visitImportStatement(
      NudlDslParser::ImportStatementContext* context) override {
    pb::ImportStatement stmt;
    for (auto spec : context->importSpecification()) {
      *stmt.add_spec() =
          std::any_cast<pb::ImportStatement::Specification>(visit(spec));
    }
    return std::make_any<pb::ImportStatement>(std::move(stmt));
  }

  std::any visitImportSpecification(
      NudlDslParser::ImportSpecificationContext* context) override {
    pb::ImportStatement::Specification spec;
    if (context->IDENTIFIER()) {
      spec.set_alias(TreeUtil::Recompose(context->IDENTIFIER()));
    }
    *spec.mutable_module() =
        identifier(CHECK_NOTNULL(context->composedIdentifier()));
    return std::make_any<pb::ImportStatement::Specification>(std::move(spec));
  }

  std::any visitModuleAssignment(
      NudlDslParser::ModuleAssignmentContext* context) override {
    auto assign = buildAssignment(CHECK_NOTNULL(context->assignExpression()));
    for (auto qualifier : context->assignQualifier()) {
      if (qualifier->KW_PARAM()) {
        assign.add_qualifier(pb::QualifierType::QUAL_PARAM);
      }
    }
    return std::make_any<pb::Assignment>(std::move(assign));
  }

  pb::PragmaExpression buildPragma(
      NudlDslParser::PragmaExpressionContext* context) {
    pb::PragmaExpression pragma_node;
    pragma_node.set_name(
        TreeUtil::Recompose(CHECK_NOTNULL(context->IDENTIFIER())));
    if (context->computeExpression()) {
      *pragma_node.mutable_value() =
          computeExpression(context->computeExpression());
    }
    return pragma_node;
  }

  std::any visitPragmaExpression(
      NudlDslParser::PragmaExpressionContext* context) override {
    pb::Expression expression(emptyExpression(*context));
    *expression.mutable_pragma_expr() = buildPragma(context);
    return std::make_any<pb::Expression>(std::move(expression));
  }

  std::any visitTypeDefinition(
      NudlDslParser::TypeDefinitionContext* context) override {
    pb::TypeDefinition type_def;
    type_def.set_name(
        TreeUtil::Recompose(CHECK_NOTNULL(context->IDENTIFIER())));
    *type_def.mutable_type_spec() =
        typeSpec(CHECK_NOTNULL(context->typeExpression()));
    return std::make_any<pb::TypeDefinition>(std::move(type_def));
  }

  std::any visitModuleElement(
      NudlDslParser::ModuleElementContext* context) override {
    pb::ModuleElement element;
    if (!code_.empty() && !options_.no_intervals) {
      element.set_code(FillInterval(*context, element.mutable_code_interval()));
    }
    if (context->importStatement()) {
      *element.mutable_import_stmt() =
          std::any_cast<pb::ImportStatement>(visit(context->importStatement()));
    } else if (context->schemaDefinition()) {
      *element.mutable_schema() = std::any_cast<pb::SchemaDefinition>(
          visit(context->schemaDefinition()));
    } else if (context->functionDefinition()) {
      *element.mutable_function_def() = std::any_cast<pb::FunctionDefinition>(
          visit(context->functionDefinition()));
    } else if (context->moduleAssignment()) {
      *element.mutable_assignment() =
          std::any_cast<pb::Assignment>(visit(context->moduleAssignment()));
    } else if (context->pragmaExpression()) {
      *element.mutable_pragma_expr() = buildPragma(context->pragmaExpression());
    } else if (context->typeDefinition()) {
      *element.mutable_type_def() =
          std::any_cast<pb::TypeDefinition>(visit(context->typeDefinition()));
    }
    return std::make_any<pb::ModuleElement>(std::move(element));
  }

  std::any visitModule(NudlDslParser::ModuleContext* context) override {
    pb::Module module;
    for (auto element : context->moduleElement()) {
      *module.add_element() = std::any_cast<pb::ModuleElement>(visit(element));
    }
    return std::make_any<pb::Module>(std::move(module));
  }
};
}  // namespace

std::unique_ptr<NudlDslParserVisitor> BuildVisitor(const antlr4::Parser* parser,
                                                   absl::string_view code,
                                                   VisitorOptions options) {
  return std::make_unique<DslVisitor>(parser, code, options);
}

}  // namespace grammar
}  // namespace nudl
