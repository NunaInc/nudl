name: "abstract_lambda_var"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "abstract_lambda_var::add::add__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "add"
    qualified_name {
      full_name: "abstract_lambda_var.add__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_ASSIGNMENT
        child {
          kind: EXPR_LAMBDA
          function_spec {
            scope_name {
              name: "abstract_lambda_var::add::add__i0::_local_lambda_1::_local_lambda_1__i0"
            }
            kind: OBJ_LAMBDA
            parameter {
              name: "s"
              type_spec {
                name: "Any"
              }
            }
            result_type {
              name: "Any"
            }
            function_name: "_local_lambda_1"
            qualified_name {
              full_name: "abstract_lambda_var._local_lambda_1__i0"
            }
            binding {
              scope_name {
                name: "abstract_lambda_var::add::add__i0::_local_lambda_1::_local_lambda_1__i0__bind_1"
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
                full_name: "abstract_lambda_var._local_lambda_1__i0__bind_1"
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
      }
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Any"
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
            name: "Function<Any(s: Int)>"
          }
        }
      }
    }
  }
}
