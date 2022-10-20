name: "call_lambda"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "call_lambda::add::add"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "add"
    qualified_name {
      full_name: "call_lambda.add"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_ASSIGNMENT
        child {
          kind: EXPR_LAMBDA
          function_spec {
            scope_name {
              name: "call_lambda::add::add::__local_lambda_1::__local_lambda_1"
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
              full_name: "call_lambda.__local_lambda_1"
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
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Int"
        }
        call_spec {
          left_expression {
            kind: EXPR_IDENTIFIER
            identifier {
              full_name: "adder"
            }
          }
          argument {
            name: "s"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x"
              }
            }
          }
          binding_type {
            name: "Function<Int(s: Int)>"
          }
        }
      }
    }
  }
}
