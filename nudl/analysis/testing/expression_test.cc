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

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "nudl/analysis/testing/analysis_test.h"
#include "nudl/status/testing.h"
#include "nudl/testing/protobuf_matchers.h"

ABSL_DECLARE_FLAG(bool, nudl_short_analysis_proto);

// This covers some error cases and specific paths for expressions
// building and expression objects.

namespace nudl {
namespace analysis {
TEST_F(AnalysisTest, ExpressionLiteral) {
  Scope base_scope;
  // Some test cases for Literal expression:
  EXPECT_RAISES(
      Literal::CheckType(base_scope.FindTypeFunction(), std::make_any<int>(10)),
      Unimplemented);
  EXPECT_RAISES(
      Literal::CheckType(base_scope.FindTypeBool(), std::make_any<int>(10)),
      InvalidArgument);
  pb::Literal exp;
  EXPECT_RAISES(Literal::Build(&base_scope, exp).status(), InvalidArgument);
  exp.set_int_value(3);
  ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
  EXPECT_EQ(literal->build_type_spec()->type_id(), pb::TypeId::INT_ID);
  ASSERT_OK_AND_ASSIGN(auto negotiated, literal->type_spec());
  EXPECT_EQ(negotiated->type_id(), pb::TypeId::INT_ID);
  EXPECT_FALSE(literal->named_object().has_value());

  Var var("foo", base_scope.FindTypeBool(), &base_scope);
  literal->set_named_object(&var);
  ASSERT_TRUE(literal->named_object().has_value());
  EXPECT_EQ(literal->named_object().value(), &var);
}

TEST_F(AnalysisTest, ExpressionNamedObject) {
  // Goes over named_object method testing in most expressions.
  pb::Literal exp;
  exp.set_int_value(3);
  Scope base_scope;
  Var var1("foo", base_scope.FindTypeBool(), &base_scope);
  Var var2("foo", base_scope.FindTypeInt(), &base_scope);
  {
    Identifier identifier(&base_scope, ScopedName::Parse("foo").value(), &var1);
    EXPECT_EQ(identifier.object(), &var1);
    EXPECT_EQ(identifier.named_object().value(), &var1);
    EXPECT_EQ(identifier.type_spec().value()->type_id(), pb::TypeId::BOOL_ID);
    absl::SetFlag(&FLAGS_nudl_short_analysis_proto, false);
    EXPECT_THAT(identifier.ToProto(), EqualsProto(R"pb(
                  kind: EXPR_IDENTIFIER
                  type_spec { type_id: BOOL_ID name: "Bool" }
                  named_object { name: "foo" kind: OBJ_VARIABLE }
                  identifier { name: "foo" }
                )pb"));
    absl::SetFlag(&FLAGS_nudl_short_analysis_proto, true);
    identifier.set_named_object(&var2);
    EXPECT_EQ(identifier.named_object().value(), &var2);
  }
  {
    ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
    ASSERT_OK_AND_ASSIGN(auto assign_exp, var2.Assign(std::move(literal)))
    Assignment assign(&base_scope, ScopedName::Parse("foo").value(), &var2,
                      std::move(assign_exp), false, true);
    EXPECT_TRUE(assign.is_initial_assignment());
    EXPECT_FALSE(assign.has_type_spec());
    EXPECT_EQ(assign.var(), &var2);
    EXPECT_EQ(assign.named_object(), &var2);
    assign.set_named_object(&var1);
    EXPECT_EQ(assign.named_object().value(), &var1);
  }
  // Build a stupid function: def f(n) => n
  pb::FunctionDefinition fdef;
  CodeContext context;
  fdef.set_name("g$");
  EXPECT_RAISES(Function::BuildInScope(&base_scope, fdef, "", context).status(),
                InvalidArgument);
  fdef.set_name("g");
  fdef.add_param()->set_name("n");
  EXPECT_RAISES(Function::BuildInScope(&base_scope, fdef, "", context).status(),
                InvalidArgument);
  fdef.mutable_expression_block()
      ->add_expression()
      ->mutable_identifier()
      ->add_name("n");
  ASSERT_RAISES(Function::BuildInScope(&base_scope, fdef, "", context).status(),
                AlreadyExists);
  fdef.set_name("f");
  ASSERT_OK_AND_ASSIGN(auto fun,
                       Function::BuildInScope(&base_scope, fdef, "", context));
  EXPECT_EQ(fun->result_kind(), pb::FunctionResultKind::RESULT_NONE);
  EXPECT_EQ(fun->definition_scope(), &base_scope);
  EXPECT_TRUE(fun->bindings_by_function().empty());
  EXPECT_TRUE(fun->native_impl().empty());
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      fun->ValidateAssignment(ScopedName::Parse("foo").value(), &var1).status(),
      InvalidArgument, testing::HasSubstr("outside its scope"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(fun->AddAsMethod(TypeUnknown::Instance()),
                                  InvalidArgument,
                                  testing::HasSubstr("can only be"));

  // To be used next:
  {
    FunctionResultExpression res(&base_scope, fun,
                                 pb::FunctionResultKind::RESULT_NONE, {});
    EXPECT_EQ(res.parent_function(), fun);
    EXPECT_EQ(res.result_kind(), pb::FunctionResultKind::RESULT_NONE);
    EXPECT_TRUE(res.DebugString().empty());
    EXPECT_FALSE(res.named_object().has_value());
  }
  {
    FunctionResultExpression res(
        &base_scope, fun, pb::FunctionResultKind::RESULT_RETURN,
        {std::make_unique<Identifier>(
            &base_scope, ScopedName::Parse("foo").value(), &var1)});
    EXPECT_EQ(res.parent_function(), fun);
    EXPECT_EQ(res.result_kind(), pb::FunctionResultKind::RESULT_RETURN);
    EXPECT_FALSE(res.DebugString().empty());
    ASSERT_TRUE(res.named_object().has_value());
    EXPECT_EQ(res.named_object().value(), &var1);
    res.set_named_object(&var2);
    EXPECT_EQ(res.named_object().value(), &var2);
  }
  pb::Literal str_exp;
  str_exp.set_str_value("x");
  {
    ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
    std::vector<std::unique_ptr<Expression>> exps;
    exps.emplace_back(std::move(literal));
    exps.emplace_back(std::make_unique<Identifier>(
        &base_scope, ScopedName::Parse("foo").value(), &var1));
    ArrayDefinitionExpression def(&base_scope, std::move(exps));
    EXPECT_RAISES(def.type_spec(base_scope.FindTypeInt()).status(),
                  InvalidArgument);
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        def.type_spec(base_scope.FindTypeByName("Array<Int>").value()).status(),
        InvalidArgument, testing::HasSubstr("Invalid element"));
  }
  {
    ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
    std::vector<std::unique_ptr<Expression>> exps;
    exps.emplace_back(std::move(literal));
    exps.emplace_back(std::make_unique<Identifier>(
        &base_scope, ScopedName::Parse("foo").value(), &var1));
    MapDefinitionExpression def(&base_scope, std::move(exps));
    EXPECT_RAISES(def.type_spec(base_scope.FindTypeInt()).status(),
                  InvalidArgument);
  }
  {
    ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
    std::vector<std::unique_ptr<Expression>> exps;
    exps.emplace_back(std::move(literal));
    ExpressionBlock block(&base_scope, std::move(exps));
    ASSERT_OK_AND_ASSIGN(auto type_spec, block.type_spec());
    EXPECT_EQ(type_spec->type_id(), pb::TypeId::INT_ID);
  }
  {
    ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
    ASSERT_OK_AND_ASSIGN(auto literal_str,
                         Literal::Build(&base_scope, str_exp));
    IndexExpression index(&base_scope, std::move(literal),
                          std::move(literal_str));
    EXPECT_RAISES(index.GetIndexedType(base_scope.FindTypeInt()).status(),
                  InvalidArgument);
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        index.type_spec().status(), InvalidArgument,
        testing::HasSubstr("does not support indexed access"));
  }
  {
    ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
    std::vector<std::unique_ptr<Expression>> exps;
    exps.emplace_back(std::move(literal));
    auto def = std::make_unique<ArrayDefinitionExpression>(&base_scope,
                                                           std::move(exps));
    IndexExpression index(
        &base_scope, std::move(def),
        std::make_unique<Identifier>(&base_scope,
                                     ScopedName::Parse("foo").value(), &var1));
    EXPECT_RAISES_WITH_MESSAGE_THAT(index.type_spec().status(), InvalidArgument,
                                    testing::HasSubstr("as index expression"));
  }
  {
    ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
    ASSERT_OK_AND_ASSIGN(auto literal_str,
                         Literal::Build(&base_scope, str_exp));
    TupleIndexExpression index(&base_scope, std::move(literal),
                               std::move(literal_str), 0);
    EXPECT_RAISES(index.GetIndexedType(base_scope.FindTypeInt()).status(),
                  InvalidArgument);
  }
  {
    ASSERT_OK_AND_ASSIGN(auto literal, Literal::Build(&base_scope, exp));
    DotAccessExpression dot(&base_scope, std::move(literal), "foo", &var1);
    EXPECT_EQ(dot.object(), &var1);
    EXPECT_EQ(dot.named_object().value(), &var1);
    dot.set_named_object(&var2);
    EXPECT_EQ(dot.named_object().value(), &var2);
  }
  {
    CheckCode("coverage_test", "import_stmt", "x = 20");
    auto module = env()->module_store()->GetModule("import_stmt");
    ASSERT_TRUE(module.has_value());
    ImportStatementExpression stmt(&base_scope, "stmt", false, module.value());
    EXPECT_EQ(stmt.named_object().value(), module);
    EXPECT_EQ(stmt.type_spec().value(), module.value()->type_spec());
    stmt.set_named_object(&var2);
    EXPECT_EQ(stmt.named_object().value(), &var2);
  }
  {
    FunctionDefinitionExpression fdef(&base_scope, fun);
    EXPECT_EQ(fdef.named_object().value(), fun);
    EXPECT_EQ(fdef.type_spec().value(), fun->type_spec());
    fdef.set_named_object(&var2);
    EXPECT_EQ(fdef.named_object().value(), &var2);
  }
  {
    ASSERT_OK_AND_ASSIGN(auto struct_type,
                         base_scope.FindTypeByName("Struct<Int>"));
    ASSERT_EQ(struct_type->type_id(), pb::TypeId::STRUCT_ID);
    SchemaDefinitionExpression def(
        &base_scope,
        const_cast<TypeStruct*>(static_cast<const TypeStruct*>(struct_type)));
    EXPECT_EQ(def.named_object().value(), struct_type);
    EXPECT_EQ(def.type_spec().value(), struct_type);
    def.set_named_object(&var2);
    EXPECT_EQ(def.named_object().value(), &var2);
  }
}

TEST_F(AnalysisTest, BuildOperatorsFailures) {
  Scope base_scope(env()->builtin_module());
  {
    pb::Expression exp;
    exp.mutable_operator_expr()->add_op("$");
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp),
                                    FailedPrecondition,
                                    testing::HasSubstr("Badly built"));
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(1);
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp),
                                    InvalidArgument,
                                    testing::HasSubstr("Unknown unary"));
    exp.mutable_operator_expr()->add_op("$");
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp),
                                    FailedPrecondition,
                                    testing::HasSubstr("Badly built unary"));
  }
  {
    pb::Expression exp;
    exp.mutable_operator_expr()->add_op("$");
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(1);
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(2);
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(3);
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        base_scope.BuildExpression(exp), InvalidArgument,
        testing::HasSubstr("Unknown ternary operator"));
  }

  {
    pb::Expression exp;
    exp.mutable_operator_expr()->add_op("$");
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(1);
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(2);
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        base_scope.BuildExpression(exp), InvalidArgument,
        testing::HasSubstr("Unknown binary operator"));
    exp.mutable_operator_expr()->add_op("$");
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp),
                                    FailedPrecondition,
                                    testing::HasSubstr("Badly built binary"));
  }
  {
    pb::Expression exp;
    exp.mutable_operator_expr()->add_op("==");
    exp.mutable_operator_expr()->add_op("==");
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(1);
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(2);
    exp.mutable_operator_expr()
        ->add_argument()
        ->mutable_literal()
        ->set_int_value(3);
    ASSERT_OK_AND_ASSIGN(auto expr, base_scope.BuildExpression(exp));
    EXPECT_THAT(
        expr->ToProto(), EqualsProto(R"pb(
          kind: EXPR_FUNCTION_CALL
          call_spec {
            call_name { full_name: "__and____i0" }
            argument {
              name: "x"
              value {
                kind: EXPR_FUNCTION_CALL
                type_spec { name: "Bool" }
                call_spec {
                  call_name { full_name: "__eq____i0" }
                  argument {
                    name: "x"
                    value {
                      kind: EXPR_LITERAL
                      literal { int_value: 1 }
                    }
                  }
                  argument {
                    name: "y"
                    value {
                      kind: EXPR_LITERAL
                      literal { int_value: 2 }
                    }
                  }
                  binding_type { name: "Function<Bool(x: Int, y: Int)>" }
                }
              }
            }
            argument {
              name: "y"
              value {
                kind: EXPR_FUNCTION_CALL
                type_spec { name: "Bool" }
                call_spec {
                  call_name { full_name: "__eq____i0" }
                  argument {
                    name: "x"
                    value {
                      kind: EXPR_LITERAL
                      literal { int_value: 2 }
                    }
                  }
                  argument {
                    name: "y"
                    value {
                      kind: EXPR_LITERAL
                      literal { int_value: 3 }
                    }
                  }
                  binding_type { name: "Function<Bool(x: Int, y: Int)>" }
                }
              }
            }
            binding_type { name: "Function<Bool(x: Bool, y: Bool)>" }
          })pb"));
  }
}

TEST_F(AnalysisTest, BuildDefFailures) {
  Scope base_scope(env()->builtin_module());
  {
    pb::Expression exp;
    exp.mutable_array_def()->clear_element();
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        base_scope.BuildExpression(exp), InvalidArgument,
        testing::HasSubstr("Empty array definition"));
  }
  {
    pb::Expression exp;
    exp.mutable_map_def()->clear_element();
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp),
                                    InvalidArgument,
                                    testing::HasSubstr("Empty map definition"));
    exp.mutable_map_def()
        ->add_element()
        ->mutable_key()
        ->mutable_literal()
        ->set_int_value(1);
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp),
                                    InvalidArgument,
                                    testing::HasSubstr("element missing key"));
  }
}

TEST_F(AnalysisTest, BuildIfFailures) {
  Scope base_scope(env()->builtin_module());
  {
    pb::Expression exp;
    exp.mutable_if_expr()->clear_condition();
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        base_scope.BuildExpression(exp), InvalidArgument,
        testing::HasSubstr("No condition provided"));
    exp.mutable_if_expr()->add_condition()->mutable_literal()->set_bool_value(
        true);
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        base_scope.BuildExpression(exp), InvalidArgument,
        testing::HasSubstr("Invalid number of conditions"));
  }
}

TEST_F(AnalysisTest, DotExpression) {
  Scope base_scope(env()->builtin_module());
  {
    pb::Expression exp;
    exp.mutable_dot_expr();
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp),
                                    FailedPrecondition,
                                    testing::HasSubstr("Missing left part"));
  }
  {
    // Build a specific path that we cannot properly readh
    // with the grammar at this point:
    pb::Expression exp;
    auto dot = exp.mutable_dot_expr();
    auto fc = dot->mutable_left()->mutable_function_call();
    fc->mutable_identifier()->add_name("ensure");
    fc->add_argument()->mutable_value()->mutable_literal()->set_str_value(
        "fooo");
    dot->mutable_function_call()->mutable_identifier()->add_name("len");
    ASSERT_OK_AND_ASSIGN(auto expr, base_scope.BuildExpression(exp));
    EXPECT_THAT(expr->ToProto(), EqualsProto(R"pb(
                  kind: EXPR_FUNCTION_CALL
                  call_spec {
                    call_name { full_name: "len__i0" }
                    is_method: true
                    argument {
                      name: "l"
                      value {
                        kind: EXPR_FUNCTION_CALL
                        type_spec { name: "String" }
                        call_spec {
                          call_name { full_name: "ensure__i2__bind_1" }
                          argument {
                            name: "x"
                            value {
                              kind: EXPR_LITERAL
                              literal { str_value: "fooo" }
                            }
                          }
                          argument {
                            name: "val"
                            value {
                              kind: EXPR_LITERAL
                              literal { str_value: "" }
                            }
                          }
                          binding_type {
                            name: "Function<String(x: String, val: String*)>"
                          }
                        }
                      }
                    }
                    binding_type { name: "Function<UInt(l: String)>" }
                  }
                )pb"));
  }
}

TEST_F(AnalysisTest, BadReturns) {
  auto module = Module::BuildTopModule(env_->module_store());
  {
    pb::FunctionDefinition fdef;
    CodeContext context;
    fdef.set_name("f");
    fdef.mutable_expression_block()
        ->add_expression()
        ->mutable_return_expr()
        ->mutable_pragma_expr()
        ->set_name("log_scope_names");
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        Function::BuildInScope(module.get(), fdef, "", context).status(),
        InvalidArgument, testing::HasSubstr("does not have a type"));
  }
  {
    pb::FunctionDefinition fdef;
    CodeContext context;
    fdef.set_name("g");
    fdef.add_param()->set_name("x-");
    fdef.mutable_expression_block()
        ->add_expression()
        ->mutable_literal()
        ->set_int_value(0);
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        Function::BuildInScope(module.get(), fdef, "", context).status(),
        InvalidArgument, testing::HasSubstr("Invalid parameter name"));
  }
}

}  // namespace analysis
}  // namespace nudl

int main(int argc, char** argv) {
  return nudl::analysis::AnalysisTest::Main(argc, argv);
}
