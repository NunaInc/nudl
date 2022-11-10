name: "call_lambda"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "call_lambda::add::add__i0"
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
      full_name: "call_lambda.add__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_ASSIGNMENT
        child {
          kind: EXPR_LAMBDA
          function_spec {
            scope_name {
              name: "call_lambda::add::add__i0::_local_lambda_1::_local_lambda_1__i0"
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
            function_name: "_local_lambda_1"
            qualified_name {
              full_name: "call_lambda._local_lambda_1__i0"
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
                    full_name: "__add____i0"
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
