name: "member_call"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "member_call::inc::inc__i0"
    }
    kind: OBJ_METHOD
    parameter {
      name: "x"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "inc"
    qualified_name {
      full_name: "member_call.inc__i0"
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
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "member_call::f::f__i0"
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
    function_name: "f"
    qualified_name {
      full_name: "member_call.f__i0"
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
            full_name: "member_call.inc__i0"
          }
          is_method: true
          argument {
            name: "x"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x"
              }
            }
          }
          binding_type {
            name: "Function<Int(x: Int)>"
          }
        }
      }
    }
  }
}
