name: "pragma_log_scope_names"
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_LITERAL
    literal {
      int_value: 33
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "pragma_log_scope_names::f::f"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Int"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "f"
    qualified_name {
      full_name: "pragma_log_scope_names.f"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_ASSIGNMENT
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
      child {
        kind: EXPR_NOP
      }
      child {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "z"
        }
      }
    }
  }
}
