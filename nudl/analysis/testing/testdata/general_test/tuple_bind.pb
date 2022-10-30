name: "tuple_bind"
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
      kind: EXPR_LITERAL
      literal {
        double_value: 3.4
      }
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "tuple_bind::g::g"
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
      full_name: "tuple_bind.g"
    }
    binding {
      scope_name {
        name: "tuple_bind::g::g__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "t"
        type_spec {
          name: "Tuple<Int, String, Float64>"
        }
      }
      result_type {
        name: "String"
      }
      function_name: "g"
      qualified_name {
        full_name: "tuple_bind.g__bind_1"
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
              int_value: 1
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
      name: "String"
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
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "x"
          }
        }
      }
      binding_type {
        name: "Function<String(t: Tuple<Int, String, Float64>)>"
      }
    }
  }
}
