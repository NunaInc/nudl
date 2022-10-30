name: "if_binding"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "if_binding::f::f"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Nullable<Int>"
    }
    function_name: "f"
    qualified_name {
      full_name: "if_binding.f"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Nullable<Int>"
        }
        call_spec {
          call_name {
            full_name: "__if__"
          }
          argument {
            name: "cond"
            value {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Bool"
              }
              call_spec {
                call_name {
                  full_name: "__gt__"
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
                      int_value: 10
                    }
                  }
                }
                binding_type {
                  name: "Function<Bool(x: Int, y: Int)>"
                }
              }
            }
          }
          argument {
            name: "val_true"
            value {
              kind: EXPR_LITERAL
              literal {
                null_value: NULL_VALUE
              }
            }
          }
          argument {
            name: "val_false"
            value {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Int"
              }
              call_spec {
                call_name {
                  full_name: "__sub__"
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
          binding_type {
            name: "Function<Nullable<Int>(cond: Bool, val_true: Nullable<Int>, val_false: Nullable<Int>)>"
          }
        }
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Nullable<Int>"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "f"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 20
          }
        }
      }
      binding_type {
        name: "Function<Nullable<Int>(x: Int)>"
      }
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "if_binding::g::g"
    }
    kind: OBJ_FUNCTION
    result_type {
      name: "Nullable<Int>"
    }
    function_name: "g"
    qualified_name {
      full_name: "if_binding.g"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "y"
        }
      }
    }
  }
}
