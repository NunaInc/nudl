#include "nudl/grammar/dsl.h"

#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/strip.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nudl/grammar/tree_util.h"
#include "nudl/status/testing.h"
#include "nudl/testing/protobuf_matchers.h"

ABSL_FLAG(bool, interactive, false, "Turn on the interactive query parsing.");
ABSL_FLAG(bool, display_snippet, false, "Display expected code snippets.");
ABSL_FLAG(bool, dsl_debug_tokens, false,
          "Logs the lexer tokens for each parse.");
ABSL_FLAG(bool, dsl_debug_trace, false, "Traces all each.");

namespace nudl {
namespace grammar {

void CheckOkParse(absl::string_view code, absl::string_view checker = "",
                  absl::string_view proto_txt = "", bool print_tree = false) {
  // std::cout << "Parsing:\n" << code << "\n";
  ASSERT_OK_AND_ASSIGN(
      auto data,
      ModuleParseData::Parse(
          code, ParseOptions{true, false, absl::GetFlag(FLAGS_dsl_debug_tokens),
                             absl::GetFlag(FLAGS_dsl_debug_trace)}));
  for (const auto& error : data->errors) {
    std::cerr << "Error found: " << error.ToString() << "\n";
  }
  LOG_IF(INFO, print_tree) << "Parse Tree:\n"
                           << TreeUtil::ToString(data->tree, *data->parser)
                           << "\n";
  std::string short_str(TreeUtil::ToShortString(data->tree, *data->parser));
  std::string proto_str;
  bool display_snippet =
      absl::GetFlag(FLAGS_display_snippet) && proto_txt.empty();
  if (data->proto) {
    proto_str = data->proto->DebugString();
  }
  std::string check_snippet(
      absl::StrCat("  CheckOkParse(\n"
                   "      \"",
                   absl::CEscape(code), "\",\n", "      R\"(\n", short_str,
                   "\n)\",\n", "      R\"(\n", proto_str, ")\");\n"));
  if (!checker.empty()) {
    EXPECT_EQ(absl::StripAsciiWhitespace(checker), short_str)
        << "For: `" << code << "`. Properly:\n"
        << check_snippet;
  } else {
    display_snippet = true;
  }
  if (!proto_txt.empty() && data->proto) {
    EXPECT_THAT(*data->proto, EqualsProto(std::string(proto_txt)));
  }
  if (display_snippet) {
    std::cout << check_snippet;
  }
  EXPECT_TRUE(data->errors.empty()) << "For: " << code;
}

std::string ReadSnippet() {
  std::vector<std::string> lines;
  std::cout << "Enter expression:" << std::endl;
  std::string line;
  while (!std::cin.eof()) {
    std::cout << " > ";
    std::getline(std::cin, line);
    if (absl::StartsWith(line, "--")) {
      break;
    }
    lines.emplace_back(std::move(line));
  }
  return absl::StrJoin(lines, "\n");
}

TEST(Parser, SimpleComp) {
  {
    auto result = ParseModule("x = 10", ParseOptions{false, false, true, true});
    ASSERT_OK(result.status());
  }
  {
    auto result = ParseModule("x # 10", ParseOptions{false, false, true, true});
    EXPECT_RAISES(result.status(), InvalidArgument);
  }
  {
    std::vector<ErrorInfo> errors;
    auto result = ParseModule("$3d", ParseOptions{false, false}, &errors);
    EXPECT_RAISES(result.status(), InvalidArgument);
    EXPECT_TRUE(!errors.empty());
    for (const auto& error : errors) {
      std::cout << "Expected error: " << error.ToString() << std::endl;
    }
  }
  {
    auto result = ParseModule("$3d", ParseOptions{false, false});
    EXPECT_RAISES(result.status(), InvalidArgument);
  }
  {
    std::vector<ErrorInfo> errors;
    auto result = ParseModule("x = $3d", ParseOptions{false, false}, &errors);
    EXPECT_RAISES(result.status(), InvalidArgument);
    EXPECT_TRUE(!errors.empty());
    for (const auto& error : errors) {
      std::cout << "Expected error: " << error.ToString() << std::endl;
    }
  }
  {
    auto result = ParseModule("x = $3d", ParseOptions{false, false});
    EXPECT_RAISES(result.status(), InvalidArgument);
  }
  {
    std::vector<ErrorInfo> errors;
    auto result = ParseModule("x = * 2", ParseOptions{false, false}, &errors);
    EXPECT_RAISES(result.status(), InvalidArgument);
    EXPECT_TRUE(!errors.empty());
    for (const auto& error : errors) {
      std::cout << "Expected error: " << error.ToString() << std::endl;
    }
  }
  {
    auto result = ParseModule("x = * 2", ParseOptions{false, false});
    EXPECT_RAISES(result.status(), InvalidArgument);
  }
  {
    ASSERT_OK_AND_ASSIGN(auto data, ModuleParseData::Parse("x = 20"));
    auto err = ErrorInfo::FromParseTree(data->tree, "x = 20");
    std::cout << "Pseudo error: " << err.ToString() << std::endl;
    antlr4::RecognitionException except(
        "foo", data->parser.get(), data->input.get(),
        static_cast<antlr4::ParserRuleContext*>(data->tree), nullptr);
    err = ErrorInfo::FromException(except, "x = 20");
    std::cout << "Pseudo exception: " << err.ToString() << std::endl;
  }
  {
    ASSERT_OK_AND_ASSIGN(auto data, TypeSpecParseData::Parse("foobar"));
    auto token = const_cast<antlr4::Token*>(CHECK_NOTNULL(TreeUtil::GetToken(
        *static_cast<NudlDslParser::TypeExpressionContext*>(data->tree)
             ->composedIdentifier()
             ->IDENTIFIER())));
    auto err = ErrorInfo::FromToken(*token, "foobar");
    std::cout << "Pseudo error: " << err.ToString() << std::endl;
    antlr4::RecognitionException except("foo", data->parser.get(),
                                        data->input.get(), nullptr, token);
    err = ErrorInfo::FromException(except, "foobar");
    std::cout << "Pseudo exception: " << err.ToString() << std::endl;
    antlr4::RecognitionException except2("foo", data->parser.get(),
                                         data->input.get(), nullptr, nullptr);
    err = ErrorInfo::FromException(except, "foobar");
    std::cout << "Pseudo exception2: " << err.ToString() << std::endl;
  }
}

TEST(Parser, SimpleParse) {
  CheckOkParse("",
               R"(

)",
               " ");
  CheckOkParse("x = y",
               R"(
module(assignExpression(x = y))
)",
               R"(
element { assignment { identifier { name: "x" }
                       value { identifier { name: "y" } } } }
)",
               true);
  CheckOkParse("x = -y;",
               R"(
module(
  moduleAssignment(assignExpression(x = unaryOperatorExpression(- y)) ;)
)
)",
               R"(
element { assignment {
    identifier { name: "x" }
    value { operator_expr { op: "-" argument { identifier { name: "y" } } } }
} })");
  CheckOkParse("x = a + b;",
               R"(
module(moduleAssignment(assignExpression(x = additiveExpression(a + b)) ;))
)",
               R"(
element { assignment {
  identifier { name: "x" }
  value { operator_expr {
    op: "+"
    argument { identifier { name: "a" } }
    argument { identifier { name: "b" } }
  } }
} })");
  CheckOkParse("x = y = 20",
               R"(
module(assignExpression(x = assignExpression(y = 20)))
)",
               R"(
element { assignment {
  identifier { name: "x" }
  value { assignment {
    identifier { name: "y" }
    value { literal { int_value: 20 original: "20" } }
  } }
} })");
  CheckOkParse("param FOO = 20",
               R"(
module(moduleAssignment(param assignExpression(FOO = 20)))
)",
               R"(
element { assignment {
  identifier { name: "FOO" }
  value { literal { int_value: 20 original: "20" } }
  qualifier: QUAL_PARAM
} })");
}

TEST(Parser, Literals) {
  CheckOkParse("x = true", "module(assignExpression(x = true))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { bool_value: true original: "true" } }
} })");
  CheckOkParse("x = 1.2", "module(assignExpression(x = 1.2))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { double_value: 1.2 original: "1.2" } }
} })");
  CheckOkParse("x = 0", "module(assignExpression(x = 0))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { int_value: 0 original: "0" } }
} })");
  CheckOkParse("x = 34", "module(assignExpression(x = 34))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { int_value: 34 original: "34" } }
} })");
  CheckOkParse("x = 34u", "module(assignExpression(x = 34u))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { uint_value: 34 original: "34u" } }
} })");
  CheckOkParse("x = 0xa2", "module(assignExpression(x = 0xa2))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { int_value: 162 original: "0xa2" } }
} })");
  CheckOkParse("x = 0xa2u", "module(assignExpression(x = 0xa2u))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { uint_value: 162 original: "0xa2u" } }
} })");
  CheckOkParse("x = 1.2e-3", "module(assignExpression(x = 1.2e-3))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { double_value: 0.0012 original: "1.2e-3" } }
} })");
  CheckOkParse("x = \"foo\"", R"(module(assignExpression(x = "foo")))",
               R"(
element { assignment {
  identifier { name: "x" }
  value { literal { str_value: "foo" original: "\"foo\"" } }
} }
)");
  CheckOkParse("x = \"f\\n\\\"\\\'\\\\\\r\\n\\u0243\"",
               R"(module(assignExpression(x = "f\n\"\'\\\r\n\u0243")))",
               R"(
element { assignment {
  identifier { name: "x" }
  value {
    literal {
      str_value: "f\n\"\'\\\r\n\311\203"
      original: "\"f\\n\\\"\\\'\\\\\\r\\n\\u0243\""
    }
  }
} })");
  CheckOkParse("x = b\"baz\"", R"(module(assignExpression(x = b"baz")))",
               R"(
element { assignment {
  identifier { name: "x" }
  value {
    literal { bytes_value: "baz" original: "b\"baz\"" }
  } } })");
  CheckOkParse("x = b\"z\\\\r\\x38\\xfc\"",
               R"(module(assignExpression(x = b"z\\r\x38\xfc")))",
               R"(
element { assignment {
  identifier { name: "x" }
  value {
    literal {
      bytes_value: "z\\r8\374"
      original: "b\"z\\\\r\\x38\\xfc\"" }
  }
} })");
CheckOkParse("x = 5weeks 7days 1hours 4minutes 1seconds",
               R"(module(
  assignExpression(x = literal(5weeks 7days 1hours 4minutes 1seconds))
))",
               R"(
element { assignment {
  identifier { name: "x" }
  value {
    literal {
      original: "5weeks 7days 1hours 4minutes 1seconds"
      time_range {
        seconds: 3632641
      }
    }
  }
  }
} )");
}

TEST(Parser, TypesAndStructs) {
  CheckOkParse("x : int = 33",
               R"(
module(assignExpression(x typeAssignment(: int) = 33))
)",
               R"(
element { assignment {
  identifier { name: "x" }
  type_spec { identifier { name: "int" } }
  value { literal { int_value: 33 original: "33" } }
} })");
  CheckOkParse("x : Array<int> = []",
               R"(
module(
  assignExpression(x
    typeAssignment(: typeExpression(Array typeTemplate(< int >))) =
    emptyStruct([ ])
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    type_spec {
      identifier { name: "Array" }
      argument { type_spec { identifier { name: "int" } } }
    }
    value { empty_struct: NULL_VALUE }
  }
}
)");
  CheckOkParse("x : Array<int> = [1, 2 + x, 3 * 4]",
               R"(
module(
  assignExpression(x
    typeAssignment(: typeExpression(Array typeTemplate(< int >))) =
    arrayDefinition([
      computeExpressions(1 , additiveExpression(2 + x) ,
        multiplicativeExpression(3 * 4)
      ) ]
    )
  )
)
)",
               R"(
element { assignment {
  identifier { name: "x" }
  type_spec { identifier { name: "Array" }
    argument { type_spec { identifier { name: "int" } } } }
  value {
    array_def {
      element { literal { int_value: 1 original: "1" } }
      element {
        operator_expr {
          op: "+"
          argument { literal { int_value: 2 original: "2" } }
          argument { identifier { name: "x" } }
        }
      }
      element {
        operator_expr {
          op: "*"
          argument { literal { int_value: 3 original: "3" } }
          argument { literal { int_value: 4 original: "4" } }
        }
      }
    }
  }
} })");
  CheckOkParse("x : Map<int, string> = []",
               R"(
module(
  assignExpression(x
    typeAssignment(: typeExpression(Map typeTemplate(< int , string >))) =
    emptyStruct([ ])
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    type_spec {
      identifier { name: "Map" }
      argument {
        type_spec {
          identifier { name: "int" } } }
      argument {
        type_spec {
          identifier { name: "string" } } }
    }
    value { empty_struct: NULL_VALUE }
  }
})");
  CheckOkParse("x : Map<int, string> = [2: \"foo\", 4: \"bar\"]",
               R"(
module(
  assignExpression(x
    typeAssignment(: typeExpression(Map typeTemplate(< int , string >))) =
    mapDefinition([ mapElements(mapElement(2 : "foo") , mapElement(4 : "bar"))
      ]
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    type_spec {
      identifier { name: "Map" }
      argument { type_spec { identifier { name: "int" } } }
      argument { type_spec { identifier { name: "string" } } }
    }
    value {
      map_def {
        element {
          key { literal { int_value: 2 original: "2" } }
          value { literal { str_value: "foo" original: "\"foo\"" } }
        }
        element {
          key { literal { int_value: 4 original: "4" } }
          value { literal { str_value: "bar" original: "\"bar\"" } }
        }
      }
    }
  }
}
)");

  CheckOkParse("x = a[5]",
               R"(
module(assignExpression(x = postfixExpression(a postfixValue([ 5 ]))))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      index_expr {
        object { identifier { name: "a" } }
        index { literal { int_value: 5 original: "5" } }
      }
    }
  }
})");
  CheckOkParse("x = a[[1,2]]",
               R"(
module(
  assignExpression(x =
    postfixExpression(a
      postfixValue([ arrayDefinition([ computeExpressions(1 , 2) ]) ])
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      index_expr {
        object { identifier { name: "a" } }
        index {
          array_def {
            element { literal { int_value: 1 original: "1" } }
            element { literal { int_value: 2 original: "2" } }
          }
        }
      }
    }
  }
})");
  CheckOkParse("x : Iterable<{T:Nullable<{V:Iterable<{Z}>}>}> = []",
               R"(
module(
  assignExpression(x
    typeAssignment(:
      typeExpression(Iterable
        typeTemplate(<
          typeNamedArgument({ T
            typeAssignment(:
              typeExpression(Nullable
                typeTemplate(<
                  typeNamedArgument({ V
                    typeAssignment(:
                      typeExpression(Iterable typeTemplate(< typeNamedArgument({ Z }) >))
                    ) }
                  ) >
                )
              )
            ) }
          ) >
        )
      )
    ) = emptyStruct([ ])
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    type_spec {
      identifier { name: "Iterable" }
      argument {
        type_spec {
          identifier { name: "T" }
          argument {
            type_spec {
              identifier { name: "Nullable" }
              argument {
                type_spec {
                  identifier { name: "V" }
                  argument {
                    type_spec {
                      identifier { name: "Iterable" }
                      argument {
                        type_spec { identifier { name: "Z" } is_local_type: true }
                      }
                    }
                  }
                  is_local_type: true
                }
              }
            }
          }
          is_local_type: true
        }
      }
    }
    value { empty_struct: NULL_VALUE }
  }
}
)");
}

TEST(Parser, FunctionDef) {
  CheckOkParse("def f() => 1",
               R"(
module(functionDefinition(def f ( ) => 1))
)",
               R"(
element {
  function_def {
    name: "f"
    expression_block {
      expression { literal { int_value: 1 original: "1" } }
    }
  }
})");
  CheckOkParse("def f() => { 1 }",
               R"(
module(functionDefinition(def f ( ) => expressionBlock({ 1 })))
)",
               R"(
element {
  function_def {
    name: "f"
    expression_block {
      expression { literal { int_value: 1 original: "1" } }
    } } })");
  CheckOkParse("def f(x) => { x + 1 }",
               R"(
module(
  functionDefinition(def f ( x ) =>
    expressionBlock({ additiveExpression(x + 1) })
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" }
    expression_block {
      expression {
        operator_expr {
          op: "+"
          argument { identifier { name: "x" } }
          argument { literal { int_value: 1 original: "1" } }
        }
      }
    }
  }
})");
  CheckOkParse("def f(x: int) : int => { x + 1 }",
               R"(
module(
  functionDefinition(def f ( paramDefinition(x typeAssignment(: int)) )
    typeAssignment(: int) => expressionBlock({ additiveExpression(x + 1) })
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" type_spec { identifier { name: "int" } } }
    result_type { identifier { name: "int" } }
    expression_block {
      expression {
        operator_expr {
          op: "+"
          argument { identifier { name: "x" } }
          argument { literal { int_value: 1 original: "1" } }
        }
      }
    }
  }
})");
  CheckOkParse("def f(x: int = 0) : int => { z = x + 1; z }",
               R"(
module(
  functionDefinition(def f ( paramDefinition(x typeAssignment(: int) = 0) )
    typeAssignment(: int) =>
    expressionBlock({
      blockBody(assignExpression(z = additiveExpression(x + 1)) ; z) }
    )
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" type_spec { identifier { name: "int" } }
      default_value { literal { int_value: 0 original: "0" } } }
    result_type { identifier { name: "int" } }
    expression_block {
      expression {
        assignment {
          identifier { name: "z" }
          value {
            operator_expr {
              op: "+"
              argument { identifier { name: "x" } }
              argument { literal { int_value: 1 original: "1" } }
            }
          }
        }
      }
      expression { identifier { name: "z" } }
    }
  }
})");
  CheckOkParse("def f(x: int = 0, y: int = 3) : int => return x + y; foo = bar",
               R"(
module(
  functionDefinition(def f (
    paramsList(paramDefinition(x typeAssignment(: int) = 0) ,
      paramDefinition(y typeAssignment(: int) = 3)
    ) ) typeAssignment(: int) =>
    returnExpression(return additiveExpression(x + y)) ;
  ) assignExpression(foo = bar)
)
)",
               R"(
element {
  function_def {
    name: "f"
    param {
      name: "x" type_spec { identifier { name: "int" } }
      default_value { literal { int_value: 0 original: "0" } }
    }
    param {
      name: "y" type_spec { identifier { name: "int" } }
      default_value { literal { int_value: 3 original: "3" } }
    }
    result_type { identifier { name: "int" } }
    expression_block {
      expression {
        return_expr { operator_expr {
            op: "+"
            argument { identifier { name: "x" } }
            argument { identifier { name: "y" } }
          } }
      }
    }
  }
}
element { assignment {
    identifier { name: "foo" }
    value { identifier { name: "bar"
  } } } })");
  CheckOkParse("def fun(x, y) => { x + y }",
               R"(
module(
  functionDefinition(def fun ( paramsList(x , y) ) =>
    expressionBlock({ additiveExpression(x + y) })
  )
)
)",
               R"(
element {
  function_def {
    name: "fun" param { name: "x" } param { name: "y" }
    expression_block { expression {
        operator_expr {
          op: "+" argument { identifier { name: "x" } }
          argument { identifier { name: "y" } }
        }
      }
    }
  }
})");
  CheckOkParse("def f(x = 1) => { x + 2 }",
               R"(
module(
  functionDefinition(def f ( paramDefinition(x = 1) ) =>
    expressionBlock({ additiveExpression(x + 2) })
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param {
      name: "x"
      default_value { literal { int_value: 1 original: "1" } }
    }
    expression_block {
      expression {
        operator_expr {
          op: "+"
          argument { identifier { name: "x" } }
          argument { literal { int_value: 2 original: "2" } }
        }
      }
    }
  }
})");
  CheckOkParse("def f(x = 1, y) => { y * x - 2 }",
               R"(
module(
  functionDefinition(def f ( paramsList(paramDefinition(x = 1) , y) ) =>
    expressionBlock({ additiveExpression(multiplicativeExpression(y * x) - 2) })
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" default_value { literal { int_value: 1 original: "1"
    } } }
    param { name: "y" }
    expression_block {
      expression {
        operator_expr {
          op: "-"
          argument {
            operator_expr {
              op: "*"
              argument { identifier { name: "y" } }
              argument { identifier { name: "x" } }
            }
          }
          argument {
            literal { int_value: 2 original: "2" }
          }
        }
      }
    }
  }
})");
  CheckOkParse("def f(x : int = 3, y = \"foo\"): string => x + y",
               R"(
module(
  functionDefinition(def f (
    paramsList(paramDefinition(x typeAssignment(: int) = 3) ,
      paramDefinition(y = "foo")
    ) ) typeAssignment(: string) => additiveExpression(x + y)
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param {
      name: "x"
      type_spec { identifier { name: "int" } }
      default_value { literal { int_value: 3 original: "3" } }
    }
    param {
      name: "y"
      default_value { literal { str_value: "foo" original: "\"foo\"" } }
    }
    result_type { identifier { name: "string" } }
    expression_block {
      expression {
        operator_expr {
          op: "+"
          argument { identifier { name: "x" } }
          argument { identifier { name: "y" } }
        }
      }
    }
  }
})");
  CheckOkParse("def f(x) => x + 1 def g(x) => x - 1",
               R"(
module(functionDefinition(def f ( x ) => additiveExpression(x + 1))
  functionDefinition(def g ( x ) => additiveExpression(x - 1))
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" }
    expression_block {
      expression {
        operator_expr {
          op: "+"
          argument { identifier { name: "x" } }
          argument { literal { int_value: 1 original: "1" } }
        }
      }
    } } }
element {
  function_def {
    name: "g"
    param { name: "x" }
    expression_block {
      expression {
        operator_expr {
          op: "-"
          argument { identifier { name: "x" } }
          argument { literal { int_value: 1 original: "1" } }
        }
      }
    }
  }
})");
  CheckOkParse(
      "def f(x: {T}, y: {T: Iterable<{T:Numeric}>}) => y.map(z => x + z)",
      R"(
module(
  functionDefinition(def f (
    paramsList(paramDefinition(x typeAssignment(: typeNamedArgument({ T }))) ,
      paramDefinition(y
        typeAssignment(:
          typeNamedArgument({ T
            typeAssignment(:
              typeExpression(Iterable
                typeTemplate(< typeNamedArgument({ T typeAssignment(: Numeric) }) >)
              )
            ) }
          )
        )
      )
    ) ) =>
    postfixExpression(composedIdentifier(y dotIdentifier(. map))
      postfixValue(( lambdaExpression(z => additiveExpression(x + z)) ))
    )
  )
)
)",
      R"(
element {
  function_def {
    name: "f"
    param {
      name: "x"
      type_spec {
        identifier { name: "T" }
        is_local_type: true
      }
    }
    param {
      name: "y"
      type_spec {
        identifier { name: "T" }
        argument {
          type_spec {
            identifier {
              name: "Iterable"
            }
            argument {
              type_spec {
                identifier {
                  name: "T"
                }
                argument {
                  type_spec {
                    identifier {
                      name: "Numeric"
                    }
                  }
                }
                is_local_type: true
              }
            }
          }
        }
        is_local_type: true
      }
    }
    expression_block {
      expression {
        function_call {
          expr_spec {
            identifier { name: "y" name: "map" }
          }
          argument {
            value {
              lambda_def {
                param { name: "z" }
                expression_block {
                  expression {
                    operator_expr {
                      op: "+"
                      argument { identifier { name: "x" } }
                      argument { identifier { name: "z" } }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
})");
}

TEST(Parser, FunctionDefNative) {
  CheckOkParse(R"(
def f(x) =>
$$pyimpl
    y = x + 1
    return x ** y
$$end
    )",
               R"(
module(
  functionDefinition(def f ( x ) =>
    $$pyimpl
    y = x + 1
    return x ** y
$$end
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" }
    snippet {
      name: "pyimpl"
      body: "    y = x + 1\n    return x ** y"
    }
  }
}
)");

  CheckOkParse(R"(
def f(x, y) =>
$$pyinline
    x + y
$$end
$$ccinline
    x + y
$$end
     )",
               R"(
module(
  functionDefinition(def f ( paramsList(x , y) ) =>
    $$pyinline
    x + y
$$end
    $$ccinline
    x + y
$$end
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" }
    param { name: "y" }
    snippet {
      name: "pyinline"
      body: "    x + y"
    }
    snippet {
      name: "ccinline"
      body: "    x + y"
    }
  }
}
)");
}

TEST(Parser, FunctionCall) {
  CheckOkParse("x = fun(x, y)",
               R"(
module(
  assignExpression(x =
    postfixExpression(fun postfixValue(( argumentList(x , y) )))
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      function_call {
        expr_spec { identifier { name: "fun" } }
        argument { value { identifier { name: "x" } } }
        argument { value { identifier { name: "y"} } }
      }
    }
  }
}
)");
  CheckOkParse("x = date.month()",
               R"(
module(
  assignExpression(x =
    postfixExpression(composedIdentifier(date dotIdentifier(. month))
      postfixValue(( ))
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value { function_call {
        expr_spec { identifier { name: "date" name: "month" } }
      } }
  }
}
)");
  CheckOkParse("x = fun(x = 20, y = 2 * b)",
               R"(
module(
  assignExpression(x =
    postfixExpression(fun
      postfixValue((
        argumentList(argumentSpec(x = 20) ,
          argumentSpec(y = multiplicativeExpression(2 * b))
        ) )
      )
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      function_call {
        expr_spec { identifier { name: "fun" } }
        argument {
          name: "x"
          value { literal { int_value: 20 original: "20" } }
        }
        argument {
          name: "y"
          value {
            operator_expr {
              op: "*"
              argument { literal { int_value: 2 original: "2" } }
              argument { identifier { name: "b" } }
            }
          }
        }
      }
    }
  }
})");
}

TEST(Parser, FunctionalCall) {
  CheckOkParse("x = map(list, x => x + 1)",
               R"(
module(
  assignExpression(x =
    postfixExpression(map
      postfixValue((
        argumentList(list , lambdaExpression(x => additiveExpression(x + 1)))
        )
      )
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      function_call {
        expr_spec { identifier { name: "map" } }
        argument { value { identifier { name: "list" } } }
        argument {
          value {
            lambda_def {
              param { name: "x" }
              expression_block {
                expression {
                  operator_expr {
                    op: "+"
                    argument { identifier { name: "x" } }
                    argument { literal { int_value: 1 original: "1" } }
                  }
                }
              } } } }
      } } } })");
  CheckOkParse(
      "x = filter(members, m => m.foo > 10 or m.bar between (10, 20) )",
      R"(
module(
  assignExpression(x =
    postfixExpression(filter
      postfixValue((
        argumentList(members ,
          lambdaExpression(m =>
            logicalOrExpression(
              relationalExpression(composedIdentifier(m dotIdentifier(. foo)) > 10)
              or
              betweenExpression(composedIdentifier(m dotIdentifier(. bar)) between ( 10 ,
                20 )
              )
            )
          )
        ) )
      )
    )
  )
)
)",
      R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      function_call {
        expr_spec { identifier { name: "filter" } }
        argument { value { identifier { name: "members" } } }
        argument {
          value {
            lambda_def {
              param { name: "m" }
              expression_block {
                expression {
                  operator_expr {
                    op: "or"
                    argument {
                      operator_expr {
                        op: ">"
                        argument { identifier { name: "m" name: "foo" } }
                        argument { literal { int_value: 10 original: "10" } }
                      }
                    }
                    argument {
                      operator_expr {
                        op: "between"
                        argument { identifier { name: "m" name: "bar" } }
                        argument { literal { int_value: 10 original: "10" } }
                        argument { literal { int_value: 20 original: "20" } }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse(
      R"(
def foo(values: Array<int>) => {
  last = 0;
  map(values, v => { yield x + last / 2; last = x; pass })
}
)",
      R"(
module(
  functionDefinition(def foo (
    paramDefinition(values
      typeAssignment(: typeExpression(Array typeTemplate(< int >)))
    ) ) =>
    expressionBlock({
      blockBody(assignExpression(last = 0) ;
        postfixExpression(map
          postfixValue((
            argumentList(values ,
              lambdaExpression(v =>
                expressionBlock({
                  blockBody(
                    yieldExpression(yield
                      additiveExpression(x + multiplicativeExpression(last / 2))
                    ) ; assignExpression(last = x) ; pass
                  ) }
                )
              )
            ) )
          )
        )
      ) }
    )
  )
)
)",
      R"(
element {
  function_def {
    name: "foo"
    param {
      name: "values"
      type_spec {
        identifier { name: "Array" }
        argument { type_spec { identifier { name: "int" } } }
      }
    }
    expression_block {
      expression {
        assignment {
          identifier { name: "last" }
          value { literal { int_value: 0 original: "0" } }
        }
      }
      expression {
        function_call {
          expr_spec { identifier { name: "map" } }
          argument { value { identifier { name: "values" } } }
          argument { value { lambda_def {
                param { name: "v" }
                expression_block {
                  expression {
                    yield_expr {
                      operator_expr {
                        op: "+"
                        argument { identifier { name: "x" } }
                        argument {
                          operator_expr {
                            op: "/"
                            argument { identifier { name: "last" } }
                            argument { literal { int_value: 2 original: "2" } }
                          }
                        }
                      }
                    }
                  }
                  expression {
                    assignment {
                      identifier { name: "last" }
                      value { identifier { name: "x" } }
                    }
                  }
                  expression { pass_expr: NULL_VALUE }
                }
              }
            }
          }
        }
      }
    }
  }
})");
  CheckOkParse(
      R"(
def foo(claims) => {
  last = 0;
  claims
    .sort_asc(c => c.date)
    .filter(c => {yield c.amount > last; last = c.amount; pass})
    .map(c => {
      Extract.new(
        member = c.member_id,
        amount = c.amount
      )
    })
}
)",
      R"(
module(
  functionDefinition(def foo ( claims ) =>
    expressionBlock({
      blockBody(assignExpression(last = 0) ;
        postfixExpression(composedIdentifier(claims dotIdentifier(. sort_asc))
          postfixValue((
            lambdaExpression(c => composedIdentifier(c dotIdentifier(. date)))
            )
          ) postfixValue(. filter)
          postfixValue((
            lambdaExpression(c =>
              expressionBlock({
                blockBody(
                  yieldExpression(yield
                    relationalExpression(composedIdentifier(c dotIdentifier(. amount)) > last)
                  ) ;
                  assignExpression(last = composedIdentifier(c dotIdentifier(. amount)))
                  ; pass
                ) }
              )
            ) )
          ) postfixValue(. map)
          postfixValue((
            lambdaExpression(c =>
              expressionBlock({
                postfixExpression(composedIdentifier(Extract dotIdentifier(. new))
                  postfixValue((
                    argumentList(
                      argumentSpec(member = composedIdentifier(c dotIdentifier(. member_id)))
                      ,
                      argumentSpec(amount = composedIdentifier(c dotIdentifier(. amount)))
                    ) )
                  )
                ) }
              )
            ) )
          )
        )
      ) }
    )
  )
)
)",
      R"(
element {
  function_def {
    name: "foo"
    param {
      name: "claims"
    }
    expression_block {
      expression {
        assignment {
          identifier { name: "last" }
          value { literal { int_value: 0 original: "0" } }
        }
      }
      expression {
        function_call {
          expr_spec {
            dot_expr {
              left {
                function_call {
                  expr_spec {
                    dot_expr {
                      left {
                        function_call {
                          expr_spec { identifier {
                            name: "claims" name: "sort_asc" } }
                          argument {
                            value {
                              lambda_def {
                                param { name: "c" }
                                expression_block {
                                  expression {
                                    identifier { name: "c" name: "date" } } }
                              }
                            }
                          }
                        }
                      }
                      name: "filter"
                    }
                  }
                  argument {
                    value {
                      lambda_def {
                        param { name: "c" }
                        expression_block {
                          expression {
                            yield_expr {
                              operator_expr {
                                op: ">"
                                argument { identifier {
                                  name: "c" name: "amount" } }
                                argument {
                                  identifier { name: "last" } }
                              }
                            }
                          }
                          expression {
                            assignment {
                              identifier { name: "last" }
                              value { identifier { name: "c" name: "amount" } }
                            }
                          }
                          expression { pass_expr: NULL_VALUE }
                        }
                      }
                    }
                  }
                }
              }
              name: "map"
            }
          }
          argument {
            value {
              lambda_def {
                param { name: "c" }
                expression_block {
                  expression {
                    function_call {
                      expr_spec { identifier { name: "Extract" name: "new" } }
                      argument {
                        name: "member"
                        value { identifier { name: "c" name: "member_id" } }
                      }
                      argument {
                        name: "amount"
                        value { identifier { name: "c" name: "amount" } }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
)");
}

TEST(Parser, Operators) {
  CheckOkParse("x = a + ~b",
               R"(
module(
  assignExpression(x = additiveExpression(a + unaryOperatorExpression(~ b)))
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "+"
        argument { identifier { name: "a" } }
        argument {
          operator_expr {
            op: "~"
            argument { identifier { name: "b" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = a + not b",
               R"(
module(
  assignExpression(x = additiveExpression(a + unaryOperatorExpression(not b)))
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "+"
        argument { identifier { name: "a" } }
        argument {
          operator_expr {
            op: "not"
            argument { identifier { name: "b" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = a * b + c",
               R"(
module(
  assignExpression(x =
    additiveExpression(multiplicativeExpression(a * b) + c)
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "+"
        argument {
          operator_expr {
            op: "*"
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
          }
        }
        argument { identifier { name: "c" } }
      }
    }
  }
}
)");
  CheckOkParse("x = a + b * c",
               R"(
module(
  assignExpression(x =
    additiveExpression(a + multiplicativeExpression(b * c))
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "+"
        argument { identifier { name: "a" } }
        argument {
          operator_expr {
            op: "*"
            argument { identifier { name: "b" } }
            argument { identifier { name: "c" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = (a + b) * c",
               R"(
module(
  assignExpression(x =
    multiplicativeExpression(
      parenthesisedExpression(( additiveExpression(a + b) )) * c
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "*"
        argument {
          operator_expr {
            op: "+"
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
          }
        }
        argument { identifier { name: "c" } }
      }
    }
  }
}
)");
  CheckOkParse("x = a - y * z >> 3",
               R"(
module(
  assignExpression(x =
    shiftExpression(additiveExpression(a - multiplicativeExpression(y * z))
      shiftOperator(> >) 3
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: ">>"
        argument {
          operator_expr {
            op: "-"
            argument { identifier { name: "a" } }
            argument {
              operator_expr {
                op: "*"
                argument { identifier { name: "y" } }
                argument { identifier { name: "z" } }
              }
            }
          }
        }
        argument { literal { int_value: 3 original: "3" } }
      }
    }
  }
}
)");
  CheckOkParse("x = a + b >> 3 - 1",
               R"(
module(
  assignExpression(x =
    shiftExpression(additiveExpression(a + b) shiftOperator(> >)
      additiveExpression(3 - 1)
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: ">>"
        argument {
          operator_expr {
            op: "+"
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
          }
        }
        argument {
          operator_expr {
            op: "-"
            argument { literal { int_value: 3  original: "3" } }
            argument { literal { int_value: 1 original: "1" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = 1 < 2 ",
               R"(
module(assignExpression(x = relationalExpression(1 < 2)))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "<"
        argument { literal { int_value: 1 original: "1" } }
        argument { literal { int_value: 2 original: "2" } }
      }
    }
  }
}
)");
  CheckOkParse("x = 1 <= 2 ",
               R"(
module(assignExpression(x = relationalExpression(1 <= 2)))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "<="
        argument { literal { int_value: 1 original: "1" } }
        argument { literal { int_value: 2 original: "2" } }
      }
    }
  }
}
)");
  CheckOkParse("x = 1 > 2 ",
               R"(
module(assignExpression(x = relationalExpression(1 > 2)))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: ">"
        argument { literal { int_value: 1 original: "1" } }
        argument { literal { int_value: 2 original: "2" } }
      }
    }
  }
}
)");
  CheckOkParse("x = 1 >= 2 ",
               R"(
module(assignExpression(x = relationalExpression(1 >= 2)))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: ">="
        argument { literal { int_value: 1 original: "1" } }
        argument { literal { int_value: 2 original: "2" } }
      }
    }
  }
}
)");
  CheckOkParse("x = a + b > 3 >> z",
               R"(
module(
  assignExpression(x =
    relationalExpression(additiveExpression(a + b) >
      shiftExpression(3 shiftOperator(> >) z)
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: ">"
        argument {
          operator_expr {
            op: "+"
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
          }
        }
        argument {
          operator_expr {
            op: ">>"
            argument { literal { int_value: 3 original: "3" } }
            argument { identifier { name: "z" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = a < b == b < c",
               R"(
module(
  assignExpression(x =
    equalityExpression(relationalExpression(a < b) ==
      relationalExpression(b < c)
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "=="
        argument {
          operator_expr {
            op: "<"
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
          }
        }
        argument {
          operator_expr {
            op: "<"
            argument { identifier { name: "b" } }
            argument { identifier { name: "c" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = a == b",
               R"(
module(assignExpression(x = equalityExpression(a == b)))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "=="
        argument { identifier { name: "a" } }
        argument { identifier { name: "b" } }
      }
    }
  }
}
)");
  CheckOkParse("x = a == b & c",
               R"(
module(assignExpression(x = andExpression(equalityExpression(a == b) & c)))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "&"
        argument {
          operator_expr {
            op: "=="
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
          }
        }
        argument { identifier { name: "c" } }
      }
    }
  }
}
)");
  CheckOkParse("x = a ^ b & c",
               R"(
module(assignExpression(x = xorExpression(a ^ andExpression(b & c))))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "^"
        argument { identifier { name: "a" } }
        argument {
          operator_expr {
            op: "&"
            argument { identifier { name: "b" } }
            argument { identifier { name: "c" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = a | b ^ c",
               R"(
module(assignExpression(x = orExpression(a | xorExpression(b ^ c))))
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "|"
        argument { identifier { name: "a" } }
        argument {
          operator_expr {
            op: "^"
            argument { identifier { name: "b" } }
            argument { identifier { name: "c" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = a | b between (c, d)",
               R"(
module(
  assignExpression(x =
    betweenExpression(orExpression(a | b) between ( c , d ))
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "between"
        argument {
          operator_expr {
            op: "|"
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
          }
        }
        argument { identifier { name: "c" } }
        argument { identifier { name: "d" } }
      }
    }
  }
}
)");
  CheckOkParse("x = a between (b, c) in [1, 2]",
               R"(
module(
  assignExpression(x =
    inExpression(betweenExpression(a between ( b , c )) in
      arrayDefinition([ computeExpressions(1 , 2) ])
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "in"
        argument {
          operator_expr {
            op: "between"
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
            argument { identifier { name: "c" } }
          }
        }
        argument {
          array_def {
            element { literal { int_value: 1 original: "1" } }
            element { literal { int_value: 2 original: "2" } }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = a and b in [1,2]",
               R"(
module(
  assignExpression(x =
    logicalAndExpression(a and
      inExpression(b in arrayDefinition([ computeExpressions(1 , 2) ]))
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "and"
        argument { identifier { name: "a" } }
        argument {
          operator_expr {
            op: "in"
            argument { identifier { name: "b" } }
            argument {
              array_def {
                element { literal { int_value: 1 original: "1" } }
                element { literal { int_value: 2 original: "2" } }
              }
            }
          }
        }
      }
    }
  }
})");
  CheckOkParse("z = a xor b == 1 and c < 2",
               R"(
module(
  assignExpression(z =
    logicalXorExpression(a xor
      logicalAndExpression(equalityExpression(b == 1) and
        relationalExpression(c < 2)
      )
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "z" }
    value {
      operator_expr {
        op: "xor"
        argument { identifier { name: "a" } }
        argument {
          operator_expr {
            op: "and"
            argument {
              operator_expr {
                op: "=="
                argument { identifier { name: "b" } }
                argument { literal { int_value: 1 original: "1" } }
              }
            }
            argument {
              operator_expr {
                op: "<"
                argument { identifier { name: "c" } }
                argument { literal { int_value: 2 original: "2" } }
              }
            }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("z = a or b xor c",
               R"(
module(
  assignExpression(z =
    logicalOrExpression(a or logicalXorExpression(b xor c))
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "z" }
    value {
      operator_expr {
        op: "or"
        argument { identifier { name: "a" } }
        argument {
          operator_expr {
            op: "xor"
            argument { identifier { name: "b" } }
            argument { identifier { name: "c" } }
          }
        }
      }
    }
  }
})");
  CheckOkParse("x = a or b ? (c or d, d + 1)",
               R"(
module(
  assignExpression(x =
    conditionalExpression(logicalOrExpression(a or b) ? (
      logicalOrExpression(c or d) , additiveExpression(d + 1) )
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      operator_expr {
        op: "?"
        argument {
          operator_expr {
            op: "or"
            argument { identifier { name: "a" } }
            argument { identifier { name: "b" } }
          }
        }
        argument {
          operator_expr {
            op: "or"
            argument { identifier { name: "c" } }
            argument { identifier { name: "d" } }
          }
        }
        argument {
          operator_expr {
            op: "+"
            argument { identifier { name: "d" } }
            argument { literal { int_value: 1 original: "1" } }
          }
        }
      }
    }
  }
})");
}

TEST(Parser, Ifs) {
  CheckOkParse("def f() => if (x > 2) { a }",
               R"(
module(
  functionDefinition(def f ( ) =>
    ifExpression(if ( relationalExpression(x > 2) ) expressionBlock({ a }))
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    expression_block {
      expression { if_expr {
        condition {
          operator_expr {
            op: ">"
            argument { identifier { name: "x" } }
            argument { literal { int_value: 2 original: "2" } }
          }
        }
        expression_block {
          expression { identifier { name: "a" } }
        }
      }
    } }
  }
}
)");
  CheckOkParse("def f() => if (x > 2) { a = 3 } else { a = 4 }",
               R"(
module(
  functionDefinition(def f ( ) =>
    ifExpression(if ( relationalExpression(x > 2) )
      expressionBlock({ assignExpression(a = 3) })
      elseExpression(else expressionBlock({ assignExpression(a = 4) }))
    )
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    expression_block {
      expression { if_expr {
        condition {
          operator_expr {
            op: ">"
            argument { identifier { name: "x" } }
            argument { literal { int_value: 2 original: "2" } }
          }
        }
        expression_block {
          expression {
            assignment {
              identifier { name: "a" }
              value { literal { int_value: 3 original: "3" } }
            }
          }
        }
        expression_block {
          expression {
            assignment {
              identifier { name: "a" }
              value { literal { int_value: 4 original: "4" } }
            }
          }
        }
      } }
    }
  }
}
)");
  CheckOkParse(
      "def f() => if (x < 1) { a = 1; b = x + 2 } "
      "else { z = 3; f(4) }",
      R"(
module(
  functionDefinition(def f ( ) =>
    ifExpression(if ( relationalExpression(x < 1) )
      expressionBlock({
        blockBody(assignExpression(a = 1) ;
          assignExpression(b = additiveExpression(x + 2))
        ) }
      )
      elseExpression(else
        expressionBlock({
          blockBody(assignExpression(z = 3) ;
            postfixExpression(f postfixValue(( 4 )))
          ) }
        )
      )
    )
  )
)
)",
      R"(
element {
  function_def {
    name: "f"
    expression_block {
      expression { if_expr {
        condition {
          operator_expr {
            op: "<"
            argument { identifier { name: "x" } }
            argument { literal { int_value: 1 original: "1" } }
          }
        }
        expression_block {
          expression {
            assignment {
              identifier { name: "a" }
              value { literal { int_value: 1 original: "1" } }
            }
          }
          expression {
            assignment {
              identifier { name: "b" }
              value {
                operator_expr {
                  op: "+"
                  argument { identifier { name: "x" } }
                  argument { literal { int_value: 2 original: "2" } }
                }
              }
            }
          }
        }
        expression_block {
          expression {
            assignment {
              identifier { name: "z" }
              value { literal { int_value: 3 original: "3" } }
            }
          }
          expression {
            function_call {
              expr_spec { identifier { name: "f" } }
              argument {
                value { literal { int_value: 4 original: "4" } }
              }
            }
          }
        }
      } }
    }
  }
}
)");
  CheckOkParse("def f() => if (x) { a; b; } elif (y) { c; d; } else { e; f }",
               R"(
module(
  functionDefinition(def f ( ) =>
    ifExpression(if ( x ) expressionBlock({ blockBody(a ; b ;) })
      elifExpression(elif ( y ) expressionBlock({ blockBody(c ; d ;) })
        elseExpression(else expressionBlock({ blockBody(e ; f) }))
      )
    )
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    expression_block {
      expression { if_expr {
        condition { identifier { name: "x" } }
        condition { identifier { name: "y" } }
        expression_block {
          expression { identifier { name: "a" } }
          expression { identifier { name: "b" } }
        }
        expression_block {
          expression { identifier { name: "c" } }
          expression { identifier { name: "d" } }
        }
        expression_block {
          expression { identifier { name: "e" } }
          expression { identifier { name: "f" } }
        }
      } }
    }
  }
}
)");
  CheckOkParse("def f(x) => { if (x > 0) { return x } return x / 2 }",
               R"(
module(
  functionDefinition(def f ( x ) =>
    expressionBlock({
      blockBody(
        ifExpression(if ( relationalExpression(x > 0) )
          expressionBlock({ returnExpression(return x) })
        ) returnExpression(return multiplicativeExpression(x / 2))
      ) }
    )
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" }
    expression_block {
      expression {
        if_expr {
          condition {
            operator_expr {
              op: ">"
              argument { identifier { name: "x" } }
              argument {
                literal { int_value: 0 original: "0" }
              }
            }
          }
          expression_block {
            expression {
              return_expr { identifier { name: "x" } }
            }
          }
        }
      }
      expression {
        return_expr {
          operator_expr {
            op: "/"
            argument { identifier { name: "x" } }
            argument {
              literal { int_value: 2 original: "2" }
            }
          }
        }
      }
    }
  }
}
)");
}

TEST(Paser, WithExpression) {
  CheckOkParse("def f(a) => with(a) { x = 1 + 1 }",
               R"(
module(
  functionDefinition(def f ( a ) =>
    withExpression(with ( a )
      expressionBlock({ assignExpression(x = additiveExpression(1 + 1)) })
    )
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param {
      name: "a"
    }
    expression_block {
      expression {
        with_expr {
          with {
            identifier {
              name: "a"
            }
          }
          expression_block {
            expression {
              assignment {
                identifier {
                  name: "x"
                }
                value {
                  operator_expr {
                    op: "+"
                    argument {
                      literal {
                        int_value: 1
                        original: "1"
                      }
                    }
                    argument {
                      literal {
                        int_value: 1
                        original: "1"
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
)");
}

TEST(Parser, Imports) {
  CheckOkParse("import foo",
               R"(
module(importStatement(import foo))
)",
               R"(
element {
  import_stmt {
    spec { module { name: "foo" } }
  }
}
)");
  CheckOkParse("import foo.bar",
               R"(
module(
  importStatement(import composedIdentifier(foo dotIdentifier(. bar)))
)
)",
               R"(
element {
  import_stmt {
    spec { module { name: "foo" name: "bar" } }
  }
}
)");
  CheckOkParse("import a.c import b.d",
               R"(
module(importStatement(import composedIdentifier(a dotIdentifier(. c)))
  importStatement(import composedIdentifier(b dotIdentifier(. d)))
)
)",
               R"(
element {
  import_stmt {
    spec { module { name: "a" name: "c" } }
  }
}
element {
  import_stmt {
    spec { module { name: "b" name: "d" } }
  }
}
)");
  CheckOkParse("import a.c\nimport b.d",
               R"(
module(importStatement(import composedIdentifier(a dotIdentifier(. c)))
  importStatement(import composedIdentifier(b dotIdentifier(. d)))
)
)",
               R"(
element {
  import_stmt {
    spec { module { name: "a" name: "c" } }
  }
}
element {
  import_stmt {
    spec { module { name: "b" name: "d" } }
  }
}
)");
}

TEST(Parser, Schemas) {
  CheckOkParse(
      R"(
schema Foo = {
  member_id: int [ is_id = true ];
  foo_code: Nullable<string> [ width = 12 ];
  amount: Decimal<10, 2>;
}
)",
      R"(
module(
  schemaDefinition(schema Foo = {
    fieldsDefinition(
      fieldDefinition(member_id typeAssignment(: int)
        fieldOptions([ fieldOption(is_id = true) ])
      ) ;
      fieldDefinition(foo_code
        typeAssignment(: typeExpression(Nullable typeTemplate(< string >)))
        fieldOptions([ fieldOption(width = 12) ])
      ) ;
      fieldDefinition(amount
        typeAssignment(: typeExpression(Decimal typeTemplate(< 10 , 2 >)))
      ) ;
    ) }
  )
)
)",
      R"(
element {
  schema {
    name: "Foo"
    field {
      name: "member_id"
      type_spec { identifier { name: "int" } }
      field_option {
        name: "is_id"
        value { literal { bool_value: true original: "true" } }
      }
    }
    field {
      name: "foo_code"
      type_spec {
        identifier { name: "Nullable" }
        argument { type_spec { identifier { name: "string" } } }
      }
      field_option {
        name: "width"
        value { literal { int_value: 12 original: "12" } }
      }
    }
    field {
      name: "amount"
      type_spec {
        identifier { name: "Decimal" }
        argument { int_value: 10 }
        argument { int_value: 2 }
      }
    }
  }
}
)");
}

TEST(Parser, FunctionObjects) {
  CheckOkParse("x = p => p + 1",
               R"(
module(
  assignExpression(x = lambdaExpression(p => additiveExpression(p + 1)))
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      lambda_def {
        param { name: "p" }
        expression_block {
          expression {
            operator_expr {
              op: "+"
              argument { identifier { name: "p" } }
              argument { literal { int_value: 1 original: "1" } }
            }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = (p, q) => { z = p + q; z * q }; y = 20",
               R"(
module(
  moduleAssignment(
    assignExpression(x =
      lambdaExpression(( p , q ) =>
        expressionBlock({
          blockBody(assignExpression(z = additiveExpression(p + q)) ;
            multiplicativeExpression(z * q)
          ) }
        )
      )
    ) ;
  ) assignExpression(y = 20)
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      lambda_def {
        param { name: "p" }
        param { name: "q" }
        expression_block {
          expression {
            assignment {
              identifier { name: "z" }
              value {
                operator_expr {
                  op: "+"
                  argument { identifier { name: "p" } }
                  argument { identifier { name: "q" } }
                }
              }
            }
          }
          expression {
            operator_expr {
              op: "*"
              argument { identifier { name: "z" } }
              argument { identifier { name: "q" } }
            }
          }
        }
      }
    }
  }
}
element {
  assignment {
    identifier { name: "y" }
    value { literal { int_value: 20 original: "20" } }
  }
})");
  CheckOkParse("x = x : Int = 3 => x + 4",
               R"(
module(
  assignExpression(x =
    lambdaExpression(paramDefinition(x typeAssignment(: Int) = 3) =>
      additiveExpression(x + 4)
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier { name: "x" }
    value {
      lambda_def {
        param {
          name: "x"
          type_spec { identifier { name: "Int" } }
          default_value {
              literal { int_value: 3 original: "3" } }
        }
        expression_block {
          expression {
            operator_expr {
              op: "+"
              argument {
                identifier {
                  name: "x"
                }
              }
              argument {
                literal {
                  int_value: 4
                  original: "4"
                }
              }
            }
          }
        }
      }
    }
  }
}
)");
  CheckOkParse("x = (p: String = \"foo\", q: Int = 3) : String => p + q",
               R"(
module(
  assignExpression(x =
    lambdaExpression(( paramDefinition(p typeAssignment(: String) = "foo") ,
      paramDefinition(q typeAssignment(: Int) = 3) )
      typeAssignment(: String) => additiveExpression(p + q)
    )
  )
)
)",
               R"(
element {
  assignment {
    identifier {
      name: "x"
    }
    value {
      lambda_def {
        param {
          name: "p"
          type_spec {
            identifier {
              name: "String"
            }
          }
          default_value {
            literal {
              str_value: "foo"
              original: "\"foo\""
            }
          }
        }
        param {
          name: "q"
          type_spec {
            identifier {
              name: "Int"
            }
          }
          default_value {
            literal {
              int_value: 3
              original: "3"
            }
          }
        }
        result_type: { identifier { name: "String" } }
        expression_block {
          expression {
            operator_expr {
              op: "+"
              argument {
                identifier {
                  name: "p"
                }
              }
              argument {
                identifier {
                  name: "q"
                }
              }
            }
          }
        }
      }
    }
  }
}
)");
}

TEST(Parser, Interactive) {
  if (!absl::GetFlag(FLAGS_interactive)) {
    std::cout << "Skipping interactive" << std::endl;
    return;
  }
  std::string code;
  do {
    code = ReadSnippet();
    if (!code.empty()) {
      CheckOkParse(code);
    }
  } while (!code.empty());
}

TEST(Parser, DirectParseFunctions) {
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<pb::Module> module,
                         ParseModule(R"(
  x : int = y;
  def f(a) => a + 1;
)",
                                     ParseOptions{true}));
    EXPECT_THAT(*module, EqualsProto(R"pb(
      element {
        assignment {
          identifier { name: "x" }
          type_spec { identifier { name: "int" } }
          value { identifier { name: "y" } }
        }
      }
      element {
        function_def {
          name: "f"
          param { name: "a" }
          expression_block {
            expression {
              operator_expr {
                op: "+"
                argument { identifier { name: "a" } }
                argument { literal { int_value: 1 original: "1" } }
              }
            }
          }
        }
      })pb"));
  }
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<pb::TypeSpec> type_spec,
                         ParseTypeSpec(R"(
Array<{T: Map<string, Any>}>
)",
                                       ParseOptions{true}));
    EXPECT_THAT(*type_spec, EqualsProto(R"pb(
      identifier { name: "Array" }
      argument {
        type_spec {
          identifier { name: "T" }
          argument {
            type_spec {
              identifier { name: "Map" }
              argument { type_spec { identifier { name: "string" } } }
              argument { type_spec { identifier { name: "Any" } } }
            }
          }
          is_local_type: true
        }
      })pb"));
  }
}

TEST(Parser, CodeSnippets) {
  absl::string_view code(R"(
  x : string = "foo bar";
  z : Array<int> = [1,
 2345, 547
];
  def f(a) => {
   a
   + 1
};
)");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<pb::Module> module, ParseModule(code));
  EXPECT_THAT(*module, EqualsProto(R"pb(
    element {
      assignment {
        identifier { name: "x" }
        type_spec { identifier { name: "string" } }
        value {
          literal { str_value: "foo bar" original: "\"foo bar\"" }
          code_interval {
            begin { position: 16 line: 2 column: 15 }
            end { position: 25 line: 2 column: 24 }
          }
          code: "\"foo bar\""
        }
      }
      code_interval {
        begin { position: 3 line: 2 column: 2 }
        end { position: 26 line: 2 column: 25 }
      }
      code: "x : string = \"foo bar\";"
    }
    element {
      assignment {
        identifier { name: "z" }
        type_spec {
          identifier { name: "Array" }
          argument { type_spec { identifier { name: "int" } } }
        }
        value {
          array_def {
            element {
              literal { int_value: 1 original: "1" }
              code_interval {
                begin { position: 47 line: 3 column: 20 }
                end { position: 48 line: 3 column: 21 }
              }
              code: "1"
            }
            element {
              literal { int_value: 2345 original: "2345" }
              code_interval {
                begin { position: 51 line: 4 column: 1 }
                end { position: 55 line: 4 column: 5 }
              }
              code: "2345"
            }
            element {
              literal { int_value: 547 original: "547" }
              code_interval {
                begin { position: 57 line: 4 column: 7 }
                end { position: 60 line: 4 column: 10 }
              }
              code: "547"
            }
          }
          code_interval {
            begin { position: 46 line: 3 column: 19 }
            end { position: 62 line: 5 column: 1 }
          }
          code: "[1,\n 2345, 547\n]"
        }
      }
      code_interval {
        begin { position: 29 line: 3 column: 2 }
        end { position: 63 line: 5 column: 2 }
      }
      code: "z : Array<int> = [1,\n 2345, 547\n];"
    }
    element {
      function_def {
        name: "f"
        param { name: "a" }
        expression_block {
          expression {
            operator_expr {
              op: "+"
              argument {
                identifier { name: "a" }
                code_interval {
                  begin { position: 83 line: 7 column: 3 }
                  end { position: 84 line: 7 column: 4 }
                }
                code: "a"
              }
              argument {
                literal { int_value: 1 original: "1" }
                code_interval {
                  begin { position: 90 line: 8 column: 5 }
                  end { position: 91 line: 8 column: 6 }
                }
                code: "1"
              }
            }
            code_interval {
              begin { position: 83 line: 7 column: 3 }
              end { position: 91 line: 8 column: 6 }
            }
            code: "a\n   + 1"
          }
        }
      }
      code_interval {
        begin { position: 66 line: 6 column: 2 }
        end { position: 94 line: 9 column: 2 }
      }
      code: "def f(a) => {\n   a\n   + 1\n};"
    })pb"));
  ASSERT_OK_AND_ASSIGN(module, ParseModule(code, ParseOptions{false, true}));
  EXPECT_THAT(*module, EqualsProto(R"pb(
    element {
      assignment {
        identifier { name: "x" }
        type_spec { identifier { name: "string" } }
        value {
          literal { str_value: "foo bar" original: "\"foo bar\"" }
          code_interval {
            begin { line: 2 column: 15 }
            end { line: 2 column: 24 }
          }
          code: "\"foo bar\""
        }
      }
      code_interval {
        begin { line: 2 column: 2 }
        end { line: 2 column: 25 }
      }
      code: "x : string = \"foo bar\";"
    }
    element {
      assignment {
        identifier { name: "z" }
        type_spec {
          identifier { name: "Array" }
          argument { type_spec { identifier { name: "int" } } }
        }
        value {
          array_def {
            element {
              literal { int_value: 1 original: "1" }
              code_interval {
                begin { line: 3 column: 20 }
                end { line: 3 column: 21 }
              }
              code: "1"
            }
            element {
              literal { int_value: 2345 original: "2345" }
              code_interval {
                begin { line: 4 column: 1 }
                end { line: 4 column: 5 }
              }
              code: "2345"
            }
            element {
              literal { int_value: 547 original: "547" }
              code_interval {
                begin { line: 4 column: 7 }
                end { line: 4 column: 10 }
              }
              code: "547"
            }
          }
          code_interval {
            begin { line: 3 column: 19 }
            end { line: 5 column: 1 }
          }
          code: "[1,\n 2345, 547\n]"
        }
      }
      code_interval {
        begin { line: 3 column: 2 }
        end { line: 5 column: 2 }
      }
      code: "z : Array<int> = [1,\n 2345, 547\n];"
    }
    element {
      function_def {
        name: "f"
        param { name: "a" }
        expression_block {
          expression {
            operator_expr {
              op: "+"
              argument {
                identifier { name: "a" }
                code_interval {
                  begin { line: 7 column: 3 }
                  end { line: 7 column: 4 }
                }
                code: "a"
              }
              argument {
                literal { int_value: 1 original: "1" }
                code_interval {
                  begin { line: 8 column: 5 }
                  end { line: 8 column: 6 }
                }
                code: "1"
              }
            }
            code_interval {
              begin { line: 7 column: 3 }
              end { line: 8 column: 6 }
            }
            code: "a\n   + 1"
          }
        }
      }
      code_interval {
        begin { line: 6 column: 2 }
        end { line: 9 column: 2 }
      }
      code: "def f(a) => {\n   a\n   + 1\n};"
    })pb"));
}

TEST(Parser, Pragmas) {
  CheckOkParse("pragma enable_foo",
               R"(
module(pragmaExpression(pragma enable_foo))
)",
               R"(
element {
  pragma_expr {
    name: "enable_foo"
  }
}
)");
  CheckOkParse("pragma write_out { x(25) } y = 10", R"(
module(
  pragmaExpression(pragma write_out {
    postfixExpression(x postfixValue(( 25 ))) }
  ) assignExpression(y = 10)
)
)",
               R"(
element {
  pragma_expr {
    name: "write_out"
    value {
      function_call {
        expr_spec { identifier { name: "x" } }
        argument {
          value {
            literal { int_value: 25 original: "25" }
          }
        }
      }
    }
  }
}
element {
  assignment {
    identifier { name: "y" }
    value {
      literal { int_value: 10 original: "10" }
    }
  }
}
)");
  CheckOkParse("def f(x) => { pragma enable_foo x + 1 }",
               R"(
module(
  functionDefinition(def f ( x ) =>
    expressionBlock({
      blockBody(pragmaExpression(pragma enable_foo) additiveExpression(x + 1))
      }
    )
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" }
    expression_block {
      expression {
        pragma_expr { name: "enable_foo" }
      }
      expression {
        operator_expr {
          op: "+"
          argument { identifier { name: "x" } }
          argument {
            literal { int_value: 1 original: "1" }
          }
        }
      }
    }
  }
})");
  CheckOkParse("def f(x) => { x + 1; pragma check_type { x(33) } }",
               R"(
module(
  functionDefinition(def f ( x ) =>
    expressionBlock({
      blockBody(additiveExpression(x + 1) ;
        pragmaExpression(pragma check_type {
          postfixExpression(x postfixValue(( 33 ))) }
        )
      ) }
    )
  )
)
)",
               R"(
element {
  function_def {
    name: "f"
    param { name: "x" }
    expression_block {
      expression {
        operator_expr {
          op: "+"
          argument { identifier { name: "x" } }
          argument { literal { int_value: 1 original: "1" } }
        }
      }
      expression {
        pragma_expr {
          name: "check_type"
          value {
            function_call {
              expr_spec { identifier { name: "x" } }
              argument {
                value {
                  literal { int_value: 33 original: "33" }
                }
              }
            }
          }
        }
      }
    }
  }
})");
}

TEST(Parser, TypeDefinition) {
  CheckOkParse(R"(
typedef Foobar = Function<Array<Int>, Map<{X}, String>, Other>;
x : Foober = z
)",
               R"(
module(
  moduleElement(
    typeDefinition(typedef Foobar =
      typeExpression(Function
        typeTemplate(< typeExpression(Array typeTemplate(< Int >)) ,
          typeExpression(Map typeTemplate(< typeNamedArgument({ X }) , String >))
          , Other >
        )
      )
    ) ;
  ) assignExpression(x typeAssignment(: Foober) = z)
)
)",
               R"(
element {
  type_def {
    name: "Foobar"
    type_spec {
      identifier { name: "Function" }
      argument {
        type_spec {
          identifier { name: "Array" }
          argument { type_spec { identifier { name: "Int" } } }
        }
      }
      argument {
        type_spec {
          identifier { name: "Map" }
          argument {
            type_spec { identifier { name: "X" } is_local_type: true }
          }
          argument { type_spec { identifier { name: "String" } } }
        }
      }
      argument {
        type_spec { identifier { name: "Other" } }
      }
    }
  }
}
element {
  assignment {
    identifier { name: "x" }
    type_spec { identifier { name: "Foober" } }
    value {
      identifier { name: "z" }
    }
  }
})");
  CheckOkParse(R"(
typedef Foobar = Array<Int>
$$pyimport
import nudl.types
$$end
$$pytype
nudl.Foobar
$$end
x = Foobar(232)
)",
               R"(
module(
  typeDefinition(typedef Foobar = typeExpression(Array typeTemplate(< Int >))
    $$pyimport
import nudl.types
$$end
    $$pytype
nudl.Foobar
$$end
  ) assignExpression(x = postfixExpression(Foobar postfixValue(( 232 ))))
)
)",
               R"(
element {
  type_def {
    name: "Foobar"
    type_spec {
      identifier {
        name: "Array"
      }
      argument {
        type_spec {
          identifier {
            name: "Int"
          }
        }
      }
    }
  }
}
element {
  assignment {
    identifier {
      name: "x"
    }
    value {
      function_call {
        expr_spec {
          identifier {
            name: "Foobar"
          }
        }
        argument {
          value {
            literal {
              int_value: 232
              original: "232"
            }
          }
        }
      }
    }
  }
}
)");
}

}  // namespace grammar
}  // namespace nudl

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);
  return RUN_ALL_TESTS();
}
