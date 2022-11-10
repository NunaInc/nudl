name: "tuple_build"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "tuple_build::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "n"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "f"
    qualified_name {
      full_name: "tuple_build.f__i0"
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
                full_name: "n"
              }
            }
          }
          argument {
            name: "y"
            value {
              kind: EXPR_LITERAL
              literal {
                int_value: 1
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
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_ARRAY_DEF
    child {
      kind: EXPR_LITERAL
      literal {
        int_value: 1
      }
    }
    child {
      kind: EXPR_LITERAL
      literal {
        str_value: "foo"
      }
    }
    child {
      kind: EXPR_IDENTIFIER
      identifier {
        full_name: "f"
      }
    }
  }
}
