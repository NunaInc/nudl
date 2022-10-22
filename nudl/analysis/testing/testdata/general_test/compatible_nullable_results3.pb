name: "compatible_nullable_results3"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "compatible_nullable_results3::foo::foo"
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
    function_name: "foo"
    qualified_name {
      full_name: "compatible_nullable_results3.foo"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_IF
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Int"
          }
          call_spec {
            call_name {
              full_name: "__mod__"
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
        child {
          kind: EXPR_BLOCK
          child {
            kind: EXPR_FUNCTION_RESULT
            child {
              kind: EXPR_LITERAL
              literal {
                null_value: NULL_VALUE
              }
            }
          }
        }
      }
      child {
        kind: EXPR_FUNCTION_RESULT
        child {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "x"
          }
        }
      }
    }
  }
}
