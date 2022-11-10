name: "dot_access_function"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "dot_access_function::f::f__i0"
    }
    kind: OBJ_FUNCTION
    result_type {
      name: "Array<Int>"
    }
    function_name: "f"
    qualified_name {
      full_name: "dot_access_function.f__i0"
    }
    body {
      kind: EXPR_BLOCK
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
            int_value: 2
          }
        }
        child {
          kind: EXPR_LITERAL
          literal {
            int_value: 3
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
      name: "dot_access_function::g::g__i0"
    }
    kind: OBJ_FUNCTION
    result_type {
      name: "UInt"
    }
    function_name: "g"
    qualified_name {
      full_name: "dot_access_function.g__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "UInt"
        }
        call_spec {
          call_name {
            full_name: "len__i0"
          }
          is_method: true
          argument {
            name: "l"
            value {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Array<Int>"
              }
              call_spec {
                left_expression {
                  kind: EXPR_IDENTIFIER
                  identifier {
                    full_name: "f"
                  }
                }
                binding_type {
                  name: "Function<Array<Int>()>"
                }
              }
            }
          }
          binding_type {
            name: "Function<UInt(l: Array<Int>)>"
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
      name: "dot_access_function::h::h__i0"
    }
    kind: OBJ_FUNCTION
    result_type {
      name: "Nullable<Int>"
    }
    function_name: "h"
    qualified_name {
      full_name: "dot_access_function.h__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_INDEX
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Array<Int>"
          }
          call_spec {
            left_expression {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "f"
              }
            }
            binding_type {
              name: "Function<Array<Int>()>"
            }
          }
        }
        child {
          kind: EXPR_LITERAL
          literal {
            int_value: 2
          }
        }
      }
    }
  }
}
