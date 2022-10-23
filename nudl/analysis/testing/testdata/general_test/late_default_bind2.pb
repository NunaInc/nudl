name: "late_default_bind2"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "late_default_bind2::f::f"
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
          int_value: 20
        }
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "f"
    qualified_name {
      full_name: "late_default_bind2.f"
    }
    binding {
      scope_name {
        name: "late_default_bind2::f::f__bind_1"
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
            int_value: 20
          }
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "f"
      qualified_name {
        full_name: "late_default_bind2.f__bind_1"
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
                kind: EXPR_LITERAL
                literal {
                  int_value: 10
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
      name: "late_default_bind2::g::g"
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
      full_name: "late_default_bind2.g"
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
            full_name: "__add__"
          }
          argument {
            name: "x"
            value {
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
                      int_value: 20
                    }
                  }
                }
                binding_type {
                  name: "Function<Int(x: Int*)>"
                }
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
            name: "Function<Int(x: Int, y: Int)>"
          }
        }
      }
    }
  }
}
