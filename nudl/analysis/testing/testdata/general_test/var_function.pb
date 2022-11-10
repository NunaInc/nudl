name: "var_function"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "var_function::ff::ff__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Any"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "ff"
    qualified_name {
      full_name: "var_function.ff__i0"
    }
    binding {
      scope_name {
        name: "var_function::ff::ff__i0__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "x"
        type_spec {
          name: "Int"
        }
      }
      parameter {
        name: "y"
        type_spec {
          name: "Int"
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "ff"
      qualified_name {
        full_name: "var_function.ff__i0__bind_1"
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
    binding {
      scope_name {
        name: "var_function::ff::ff__i0__bind_2"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "x"
        type_spec {
          name: "Float64"
        }
      }
      parameter {
        name: "y"
        type_spec {
          name: "Float64"
        }
      }
      result_type {
        name: "Float64"
      }
      function_name: "ff"
      qualified_name {
        full_name: "var_function.ff__i0__bind_2"
      }
      body {
        kind: EXPR_BLOCK
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Float64"
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
              name: "Function<Float64(x: Float64, y: Float64)>"
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
    kind: EXPR_IDENTIFIER
    identifier {
      full_name: "ff"
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
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "gg"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 1
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
        name: "Function<Any(x: Int, y: Int)>"
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
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "gg"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            double_value: 1.1
          }
        }
      }
      argument {
        name: "y"
        value {
          kind: EXPR_LITERAL
          literal {
            double_value: 2.2
          }
        }
      }
      binding_type {
        name: "Function<Any(x: Float64, y: Float64)>"
      }
    }
  }
}
