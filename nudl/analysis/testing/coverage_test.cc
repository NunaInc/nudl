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

#include "nudl/analysis/testing/analysis_test.h"
#include "nudl/status/testing.h"
#include "nudl/testing/protobuf_matchers.h"

// This covers things that are a bit hard to cover via other mechanisms.

namespace nudl {
namespace analysis {

TEST_F(AnalysisTest, Vars) {
  CheckCode("coverage_test", "vars_set", R"(
import cdm

def f(name: cdm.HumanName) => {
  z = name;
  z.given = "Hammurabi"
}

// TODO(catalin): To decide - what we want with these ?
//   Do we want side effects? Right now is ok - but we may decide
//   otherwise to keep the purity and for better analysis.
def foo(x: cdm.HumanName, y: cdm.HumanName) => {
  x.prefix = y.prefix;
  len(x.prefix)
}

def g(x: Int) => x + 1

y: Nullable<cdm.HumanName> = null
x: cdm.HumanName = _ensured(y)  // this is a hack
z: Int = 10
param a: Int = 20;

)");
  auto module = env()->module_store()->GetModule("vars_set");
  ASSERT_TRUE(module.has_value());
  EXPECT_TRUE(module.value()->is_module());
  EXPECT_EQ(module.value()->type_spec()->type_id(), pb::TypeId::MODULE_ID);
  EXPECT_EQ(module.value()->type_spec()->Clone()->type_id(),
            pb::TypeId::MODULE_ID);
  EXPECT_EQ(module.value()->parent(), module.value()->top_scope());
  EXPECT_FALSE(module.value()->FindFunctionAncestor().has_value());
  ASSERT_OK_AND_ASSIGN(auto x, module.value()->GetName("x", true));
  EXPECT_EQ(x->type_spec()->type_id(), pb::TypeId::STRUCT_ID);
  EXPECT_EQ(x->kind(), pb::ObjectKind::OBJ_VARIABLE);
  auto x_var = static_cast<VarBase*>(x);
  EXPECT_EQ(x_var->assignments().size(), 1);
  EXPECT_EQ(x_var->assign_types().size(), 1);
  ASSERT_OK_AND_ASSIGN(auto given, x_var->GetName("given", true));
  ASSERT_OK_AND_ASSIGN(auto given2, x_var->GetName("given", true));
  EXPECT_EQ(given, given2);
  EXPECT_EQ(given->kind(), pb::ObjectKind::OBJ_FIELD);
  auto given_var = static_cast<VarBase*>(given);
  EXPECT_RAISES(x_var->AddName("foo", given), Unimplemented);
  EXPECT_RAISES(x_var->AddChildStore("foo", given_var), Unimplemented);
  EXPECT_EQ(given_var->GetRootVar(), x_var);
  ASSERT_TRUE(x_var->parent_store().has_value());
  EXPECT_EQ(x_var->parent_store().value(), module.value());
  auto x_copy = x_var->Clone(x_var->parent_store());
  auto given_copy = given_var->Clone(x_copy.get());
  EXPECT_EQ(given_copy->parent_store().value(), x_copy.get());
  EXPECT_FALSE(given_copy->full_name().empty());
  EXPECT_EQ(given_copy->kind(), pb::ObjectKind::OBJ_FIELD);
  EXPECT_EQ(given_copy->kind(), pb::ObjectKind::OBJ_FIELD);
  ASSERT_OK_AND_ASSIGN(auto a, module.value()->GetName("a", true));
  auto a_var = static_cast<VarBase*>(a);
  EXPECT_EQ(a_var->kind(), pb::ObjectKind::OBJ_PARAMETER);
  EXPECT_EQ(a_var->Clone(a_var->parent_store().value())->kind(),
            pb::ObjectKind::OBJ_PARAMETER);
  ASSERT_OK_AND_ASSIGN(auto g, module.value()->GetName("g", true));
  EXPECT_EQ(g->kind(), pb::ObjectKind::OBJ_FUNCTION_GROUP);
  auto gg = static_cast<FunctionGroup*>(g);
  EXPECT_FALSE(gg->DebugString().empty());
  ASSERT_OK_AND_ASSIGN(auto gf, gg->GetName("g__i0", true));
  EXPECT_EQ(gf->kind(), pb::ObjectKind::OBJ_FUNCTION);
  auto gfun = static_cast<Function*>(gf);
  ASSERT_OK_AND_ASSIGN(auto xarg, gfun->GetName("x", true));
  EXPECT_EQ(xarg->kind(), pb::ObjectKind::OBJ_ARGUMENT);
  EXPECT_EQ(xarg->parent_store().value(), gf);
  EXPECT_EQ(static_cast<Argument*>(xarg)->Clone(gfun)->kind(),
            pb::ObjectKind::OBJ_ARGUMENT);
  EXPECT_THAT(x->ToProtoRef(), EqualsProto("name: \"x\" kind: OBJ_VARIABLE"));
  EXPECT_TRUE(x->name_store().has_value());

  // Test some bases WrappedNameStore functions:
  WrappedNameStore ws("xcopy", x_var);
  EXPECT_EQ(ws.kind(), x_var->kind());
  EXPECT_FALSE(ws.DebugString().empty());
  EXPECT_EQ(ws.DefinedNames(), x_var->DefinedNames());

  // Test some scope error paths:
  EXPECT_RAISES(
      module.value()->AddSubScope(std::make_unique<Scope>(
          std::make_shared<ScopeName>(ScopeName::Parse("foo.barsky").value()),
          module.value())),
      InvalidArgument);
  EXPECT_RAISES(
      module.value()->AddSubScope(std::make_unique<Scope>(
          std::make_shared<ScopeName>(ScopeName::Parse("vars_set.foo").value()),
          module.value())),
      AlreadyExists);
  EXPECT_RAISES(module.value()->AddDefinedVar(std::make_unique<Var>(
                    "x-x", module.value()->FindTypeInt(), module.value())),
                InvalidArgument);
  EXPECT_TRUE(Scope::IsScopeKind(*module.value()));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      module.value()
          ->FindName(ScopeName::Parse("foox.bar").value(),
                     ScopedName::Parse("quxix").value())
          .status(),
      NotFound, testing::HasSubstr("looked up in scope"));
}

TEST_F(AnalysisTest, ObjectNames) {
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_UNKNOWN), "Unknown");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_VARIABLE), "Variable");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_PARAMETER), "Parameter");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_ARGUMENT), "Argument");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_FIELD), "Field");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_SCOPE), "Scope");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_FUNCTION), "Function");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_METHOD), "Method");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_LAMBDA), "Lambda");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_MODULE), "Module");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_TYPE), "Type");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_FUNCTION_GROUP),
            "FunctionGroup");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_METHOD_GROUP), "MethodGroup");
  EXPECT_EQ(ObjectKindName(pb::ObjectKind::OBJ_TYPE_MEMBER_STORE),
            "TypeMemberStore");
  EXPECT_EQ(ObjectKindName(static_cast<pb::ObjectKind>(1000)), "Unknown");
}

TEST_F(AnalysisTest, ScopeErrors) {
  CheckError("bad_scope1", R"(
def f(name: String) => {
  x = (s: String) => s;
  x.s
})",
             "Cannot find name");
  CheckError("bad_scope2", R"(
import cdm
def f(name: cdm.HumanName) => {
  name.givex
})",
             "in child name store");
  CheckError("bad_scope3", R"(
def f(name: String) => name
def g() => bad_scope3.f.f__i0.name
)",
             "cannot be accessed from scope");
  CheckError("bad_scope6", R"(
def f(name: String) => lenx(name)
)",
             "Cannot find name");
  CheckError("bad_assign1", R"(
x: Int = 3
x: String = 5
)",
             "Cannot redefine type");
  CheckError("bad_assign2", R"(
x: Int = 3
param x = 5
)",
             "Cannot use qualifiers");
  CheckError("bad_assign3", R"(
def f(x: Int) => {
  x + 10; x
})",
             "Meaningful result of function");
  CheckError("bad_method", "def method f() => 10", "at least a parameter");
  CheckError("bad_pass", "def f() => {if (true) { pass } 10}",
             "must explicitly `yield`");
}

TEST_F(AnalysisTest, ScopeBuild) {
  {
    Scope base_scope;
    pb::Expression exp;
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp).status(),
                                    InvalidArgument,
                                    testing::HasSubstr("Improper expression"));
    exp.mutable_error()->set_description("Foobarsky");
    EXPECT_RAISES_WITH_MESSAGE_THAT(base_scope.BuildExpression(exp).status(),
                                    FailedPrecondition,
                                    testing::HasSubstr("Foobarsky"));
  }
  {
    Scope base_scope;
    EXPECT_EQ(base_scope.type_spec()->type_id(), pb::TypeId::UNKNOWN_ID);
    pb::Expression exp;
    exp.mutable_literal()->set_int_value(33);
    ASSERT_OK_AND_ASSIGN(auto expr1, base_scope.BuildExpression(exp));
    EXPECT_EQ(expr1->type_spec().value()->type_id(), pb::TypeId::INT_ID);
    base_scope.add_expression(std::move(expr1));
    EXPECT_EQ(base_scope.type_spec()->type_id(), pb::TypeId::INT_ID);
    base_scope.add_expression(std::make_unique<NopExpression>(&base_scope));
    EXPECT_EQ(base_scope.type_spec()->type_id(), pb::TypeId::UNKNOWN_ID);
    pb::Expression exp2;
    exp2.set_pass_expr(pb::NullType::NULL_VALUE);
    EXPECT_RAISES_WITH_MESSAGE_THAT(
        base_scope.BuildExpression(exp2).status(), InvalidArgument,
        testing::HasSubstr("outside of a function"));
  }
}

}  // namespace analysis
}  // namespace nudl

int main(int argc, char** argv) {
  return nudl::analysis::AnalysisTest::Main(argc, argv);
}
