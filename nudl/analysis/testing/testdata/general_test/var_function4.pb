name: "var_function4"
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_TUPLE_DEF
    child {
      kind: EXPR_LITERAL
      literal {
        int_value: 1
      }
    }
    child {
      kind: EXPR_LITERAL
      literal {
        double_value: 2.3
      }
    }
    child {
      kind: EXPR_LAMBDA
      function_spec {
        scope_name {
          name: "var_function4::_local_lambda_1::_local_lambda_1__i0"
        }
        kind: OBJ_LAMBDA
        parameter {
          name: "s"
          type_spec {
            name: "Any"
          }
        }
        parameter {
          name: "t"
          type_spec {
            name: "Any"
          }
        }
        result_type {
          name: "Any"
        }
        function_name: "_local_lambda_1"
        qualified_name {
          full_name: "var_function4._local_lambda_1__i0"
        }
        binding {
          scope_name {
            name: "var_function4::_local_lambda_1::_local_lambda_1__i0__bind_1"
          }
          kind: OBJ_LAMBDA
          parameter {
            name: "s"
            type_spec {
              name: "Int"
            }
          }
          parameter {
            name: "t"
            type_spec {
              name: "Int"
            }
          }
          result_type {
            name: "Int"
          }
          function_name: "_local_lambda_1"
          qualified_name {
            full_name: "var_function4._local_lambda_1__i0__bind_1"
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
                      full_name: "s"
                    }
                  }
                }
                argument {
                  name: "y"
                  value {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "t"
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
            name: "var_function4::_local_lambda_1::_local_lambda_1__i0__bind_2"
          }
          kind: OBJ_LAMBDA
          parameter {
            name: "s"
            type_spec {
              name: "Float64"
            }
          }
          parameter {
            name: "t"
            type_spec {
              name: "Float64"
            }
          }
          result_type {
            name: "Float64"
          }
          function_name: "_local_lambda_1"
          qualified_name {
            full_name: "var_function4._local_lambda_1__i0__bind_2"
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
                      full_name: "s"
                    }
                  }
                }
                argument {
                  name: "y"
                  value {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "t"
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
    type_spec {
      name: "Tuple<a: Int, b: Float64, c: Function<Any(s: Any, t: Any)>>"
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
    kind: EXPR_IDENTIFIER
    identifier {
      full_name: "g"
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
            full_name: "g"
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
        name: "s"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 333
          }
        }
      }
      argument {
        name: "t"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 22
          }
        }
      }
      binding_type {
        name: "Function<Any(s: Int, t: Int)>"
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
            full_name: "g"
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
        name: "s"
        value {
          kind: EXPR_LITERAL
          literal {
            double_value: 333.3
          }
        }
      }
      argument {
        name: "t"
        value {
          kind: EXPR_LITERAL
          literal {
            double_value: 3.5
          }
        }
      }
      binding_type {
        name: "Function<Any(s: Float64, t: Float64)>"
      }
    }
  }
}
