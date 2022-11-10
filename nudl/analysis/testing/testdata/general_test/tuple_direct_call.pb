name: "tuple_direct_call"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "tuple_direct_call::g::g__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "t"
      type_spec {
        name: "Tuple"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "g"
    qualified_name {
      full_name: "tuple_direct_call.g__i0"
    }
    binding {
      scope_name {
        name: "tuple_direct_call::g::g__i0__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "t"
        type_spec {
          name: "Tuple<Int, String, Float64>"
        }
      }
      result_type {
        name: "Float64"
      }
      function_name: "g"
      qualified_name {
        full_name: "tuple_direct_call.g__i0__bind_1"
      }
      body {
        kind: EXPR_BLOCK
        child {
          kind: EXPR_TUPLE_INDEX
          child {
            kind: EXPR_IDENTIFIER
            identifier {
              full_name: "t"
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
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Float64"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "g"
        }
      }
      argument {
        name: "t"
        value {
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
            kind: EXPR_LITERAL
            literal {
              double_value: 2.3
            }
          }
        }
      }
      binding_type {
        name: "Function<Float64(t: Tuple<Int, String, Float64>)>"
      }
    }
  }
}
