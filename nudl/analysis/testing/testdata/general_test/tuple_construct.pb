name: "tuple_construct"
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Tuple<Int, String, Float64>"
    }
    call_spec {
      call_name {
        full_name: "tuple__bind_1"
      }
      is_method: true
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
        name: "Function<Tuple<Int, String, Float64>(t: Tuple<Int, String, Float64>)>"
      }
    }
  }
}
