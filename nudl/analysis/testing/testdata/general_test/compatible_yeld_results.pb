name: "compatible_yeld_results"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "compatible_yeld_results::foo::foo__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Generator<Int>"
    }
    function_name: "foo"
    qualified_name {
      full_name: "compatible_yeld_results.foo__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_IF
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Bool"
          }
          call_spec {
            call_name {
              full_name: "__eq____i0"
            }
            argument {
              name: "x"
              value {
                kind: EXPR_FUNCTION_CALL
                type_spec {
                  name: "Int"
                }
                call_spec {
                  call_name {
                    full_name: "__mod____i0"
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
                      kind: EXPR_LITERAL
                      literal {
                        int_value: 2
                      }
                    }
                  }
                  binding_type {
                    name: "Function<Int(x: Int, y: Int)>"
                  }
                }
              }
            }
            argument {
              name: "y"
              value {
                kind: EXPR_LITERAL
                literal {
                  int_value: 0
                }
              }
            }
            binding_type {
              name: "Function<Bool(x: Int, y: Int)>"
            }
          }
        }
        child {
          kind: EXPR_BLOCK
          child {
            kind: EXPR_FUNCTION_RESULT
          }
        }
      }
      child {
        kind: EXPR_FUNCTION_RESULT
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Int"
          }
          call_spec {
            call_name {
              full_name: "__div____i0"
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
                kind: EXPR_LITERAL
                literal {
                  int_value: 2
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
