name: "late_default_bind3"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "late_default_bind3::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "{ X : Any }"
      }
      default_value {
        kind: EXPR_LITERAL
        literal {
          int_value: 10
        }
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "{ X : Any }"
      }
      default_value {
        kind: EXPR_LITERAL
        literal {
          int_value: 20
        }
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "f"
    qualified_name {
      full_name: "late_default_bind3.f__i0"
    }
    binding {
      scope_name {
        name: "late_default_bind3::f::f__i0__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "x"
        type_spec {
          name: "Int"
        }
        default_value {
          kind: EXPR_LITERAL
          literal {
            int_value: 10
          }
        }
      }
      parameter {
        name: "y"
        type_spec {
          name: "Int"
        }
        default_value {
          kind: EXPR_LITERAL
          literal {
            int_value: 20
          }
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "f"
      qualified_name {
        full_name: "late_default_bind3.f__i0__bind_1"
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
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "late_default_bind3::g::g__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "a"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "g"
    qualified_name {
      full_name: "late_default_bind3.g__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Int"
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
                int_value: 10
              }
            }
          }
          argument {
            name: "y"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "a"
              }
            }
          }
          binding_type {
            name: "Function<Int(x: Int*, y: Int*)>"
          }
        }
      }
    }
  }
}
