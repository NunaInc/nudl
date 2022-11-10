name: "named_tuples2"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "named_tuples2::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "f"
    qualified_name {
      full_name: "named_tuples2.f__i0"
    }
    binding {
      scope_name {
        name: "named_tuples2::f::f__i0__bind_1"
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
        full_name: "named_tuples2.f__i0__bind_1"
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
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_TUPLE_DEF
    child {
      kind: EXPR_LITERAL
      literal {
        int_value: 3
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
    type_spec {
      name: "Tuple<a: Int, b: String, c: Function<Any(x: Any)>>"
    }
    tuple_def {
      element {
        name: "a"
      }
      element {
        name: "b"
      }
      element {
        name: "c"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Any"
    }
    call_spec {
      left_expression {
        kind: EXPR_TUPLE_INDEX
        child {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "x"
          }
        }
        child {
          kind: EXPR_LITERAL
          literal {
            int_value: 2
          }
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 4
          }
        }
      }
      binding_type {
        name: "Function<Any(x: Int)>"
      }
    }
  }
}
