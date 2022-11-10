name: "var_function3"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "var_function3::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "{ T : Any }"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "{ T : Any }"
      }
    }
    parameter {
      name: "fun"
      type_spec {
        name: "Function<{ T : Any }(arg_1: { T : Any })>"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "f"
    qualified_name {
      full_name: "var_function3.f__i0"
    }
    binding {
      scope_name {
        name: "var_function3::f::f__i0__bind_1"
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
      parameter {
        name: "fun"
        type_spec {
          name: "Function<Int(x: Int)>"
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "f"
      qualified_name {
        full_name: "var_function3.f__i0__bind_1"
      }
      body {
        kind: EXPR_BLOCK
        child {
          kind: EXPR_IF
          child {
            kind: EXPR_FUNCTION_CALL
            type_spec {
              name: "Bool"
            }
            call_spec {
              call_name {
                full_name: "__gt____i0"
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
                name: "Function<Bool(x: Int, y: Int)>"
              }
            }
          }
          child {
            kind: EXPR_BLOCK
            child {
              kind: EXPR_ASSIGNMENT
              child {
                kind: EXPR_LAMBDA
                function_spec {
                  scope_name {
                    name: "var_function3::f::f__i0__bind_1::_local_ifexpr_1::_local_lambda_1::_local_lambda_1__i0__bind_1"
                  }
                  kind: OBJ_LAMBDA
                  parameter {
                    name: "x"
                    type_spec {
                      name: "Int"
                    }
                  }
                  result_type {
                    name: "Int"
                  }
                  function_name: "_local_lambda_1"
                  qualified_name {
                    full_name: "var_function3._local_lambda_1__i0__bind_1"
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
                          full_name: "__mul____i0"
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
                              full_name: "x"
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
          }
        }
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Int"
          }
          call_spec {
            left_expression {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "fun"
              }
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
            binding_type {
              name: "Function<Int(x: Int)>"
            }
          }
        }
      }
    }
    binding {
      scope_name {
        name: "var_function3::f::f__i0__bind_2"
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
      parameter {
        name: "fun"
        type_spec {
          name: "Function<Float64(x: Float64)>"
        }
      }
      result_type {
        name: "Float64"
      }
      function_name: "f"
      qualified_name {
        full_name: "var_function3.f__i0__bind_2"
      }
      body {
        kind: EXPR_BLOCK
        child {
          kind: EXPR_IF
          child {
            kind: EXPR_FUNCTION_CALL
            type_spec {
              name: "Bool"
            }
            call_spec {
              call_name {
                full_name: "__gt____i0"
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
                name: "Function<Bool(x: Float64, y: Float64)>"
              }
            }
          }
          child {
            kind: EXPR_BLOCK
            child {
              kind: EXPR_ASSIGNMENT
              child {
                kind: EXPR_LAMBDA
                function_spec {
                  scope_name {
                    name: "var_function3::f::f__i0__bind_2::_local_ifexpr_1::_local_lambda_2::_local_lambda_2__i0__bind_1"
                  }
                  kind: OBJ_LAMBDA
                  parameter {
                    name: "x"
                    type_spec {
                      name: "Float64"
                    }
                  }
                  result_type {
                    name: "Float64"
                  }
                  function_name: "_local_lambda_2"
                  qualified_name {
                    full_name: "var_function3._local_lambda_2__i0__bind_1"
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
                          full_name: "__mul____i0"
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
                              full_name: "x"
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
          }
        }
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Float64"
          }
          call_spec {
            left_expression {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "fun"
              }
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
            binding_type {
              name: "Function<Float64(x: Float64)>"
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
      name: "var_function3::g::g__i0"
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
    function_name: "g"
    qualified_name {
      full_name: "var_function3.g__i0"
    }
    binding {
      scope_name {
        name: "var_function3::g::g__i0__bind_1"
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
      function_name: "g"
      qualified_name {
        full_name: "var_function3.g__i0__bind_1"
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
                  full_name: "x"
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
        name: "var_function3::g::g__i0__bind_2"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "x"
        type_spec {
          name: "Float64"
        }
      }
      result_type {
        name: "Float64"
      }
      function_name: "g"
      qualified_name {
        full_name: "var_function3.g__i0__bind_2"
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
                  full_name: "x"
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
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "var_function3::h::h__i0"
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
    function_name: "h"
    qualified_name {
      full_name: "var_function3.h__i0"
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
            int_value: 100
          }
        }
      }
      argument {
        name: "y"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 20
          }
        }
      }
      argument {
        name: "fun"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "ff"
          }
        }
      }
      binding_type {
        name: "Function<Int(x: Int, y: Int, fun: Function<Int(x: Int)>)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_IDENTIFIER
    identifier {
      full_name: "h"
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
          full_name: "f"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            double_value: 20
          }
        }
      }
      argument {
        name: "y"
        value {
          kind: EXPR_LITERAL
          literal {
            double_value: 100
          }
        }
      }
      argument {
        name: "fun"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "ff"
          }
        }
      }
      binding_type {
        name: "Function<Float64(x: Float64, y: Float64, fun: Function<Float64(x: Float64)>)>"
      }
    }
  }
}
