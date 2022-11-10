name: "default_bind_on_funtype"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "default_bind_on_funtype::f::f__i0"
    }
    kind: OBJ_FUNCTION
    result_type {
      name: "Function<Int(x: Int*, y: Int*)>"
    }
    function_name: "f"
    qualified_name {
      full_name: "default_bind_on_funtype.f__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_LAMBDA
        function_spec {
          scope_name {
            name: "default_bind_on_funtype::f::f__i0::_local_lambda_1::_local_lambda_1__i0"
          }
          kind: OBJ_LAMBDA
          parameter {
            name: "x"
            type_spec {
              name: "Int"
            }
            default_value {
              kind: EXPR_LITERAL
              literal {
                int_value: 10
              }
            }
          }
          parameter {
            name: "y"
            type_spec {
              name: "Int"
            }
            default_value {
              kind: EXPR_LITERAL
              literal {
                int_value: 20
              }
            }
          }
          result_type {
            name: "Int"
          }
          function_name: "_local_lambda_1"
          qualified_name {
            full_name: "default_bind_on_funtype._local_lambda_1__i0"
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
                      full_name: "x"
                    }
                  }
                }
                argument {
                  name: "y"
                  value {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "y"
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
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "default_bind_on_funtype::g::g__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "a"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "g"
    qualified_name {
      full_name: "default_bind_on_funtype.g__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Int"
        }
        call_spec {
          left_expression {
            kind: EXPR_FUNCTION_CALL
            type_spec {
              name: "Function<Int(x: Int*, y: Int*)>"
            }
            call_spec {
              left_expression {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "f"
                }
              }
              binding_type {
                name: "Function<Function<Int(x: Int*, y: Int*)>()>"
              }
            }
          }
          argument {
            name: "x"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "a"
              }
            }
          }
          argument {
            name: "y"
          }
          binding_type {
            name: "Function<Int(x: Int*, y: Int*)>"
          }
        }
      }
    }
  }
}
