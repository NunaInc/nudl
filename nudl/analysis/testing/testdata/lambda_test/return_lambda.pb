name: "return_lambda"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "return_lambda::get_fun::get_fun"
    }
    kind: OBJ_FUNCTION
    result_type {
      name: "Function<Int(s: Int)>"
    }
    function_name: "get_fun"
    qualified_name {
      full_name: "return_lambda.get_fun"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_LAMBDA
        function_spec {
          scope_name {
            name: "return_lambda::get_fun::get_fun::__local_lambda_1::__local_lambda_1"
          }
          kind: OBJ_LAMBDA
          parameter {
            name: "s"
            type_spec {
              name: "Int"
            }
          }
          result_type {
            name: "Int"
          }
          function_name: "__local_lambda_1"
          qualified_name {
            full_name: "return_lambda.__local_lambda_1"
          }
          body {
            kind: EXPR_BLOCK
            child {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Int"
              }
              call_spec {
                call_name {
                  full_name: "__add__"
                }
                argument {
                  name: "x"
                  value {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "s"
                    }
                  }
                }
                argument {
                  name: "y"
                  value {
                    kind: EXPR_LITERAL
                    literal {
                      int_value: 10
                    }
                  }
                }
                binding_type {
                  name: "Function<Int(x: Int, y: Int)>"
                }
              }
            }
          }
        }
      }
    }
  }
}
