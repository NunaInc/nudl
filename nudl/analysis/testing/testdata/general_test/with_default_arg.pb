name: "with_default_arg"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "with_default_arg::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Array<Int>"
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
          int_value: 3
        }
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "f"
    qualified_name {
      full_name: "with_default_arg.f__i0"
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
            full_name: "sum__i0"
          }
          is_method: true
          argument {
            name: "l"
            value {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Generator<Int>"
              }
              call_spec {
                call_name {
                  full_name: "map__i0"
                }
                is_method: true
                argument {
                  name: "l"
                  value {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "x"
                    }
                  }
                }
                argument {
                  name: "f"
                  value {
                    kind: EXPR_LAMBDA
                    function_spec {
                      scope_name {
                        name: "with_default_arg::f::f__i0::_local_lambda_1::_local_lambda_1__i0"
                      }
                      kind: OBJ_LAMBDA
                      parameter {
                        name: "p"
                        type_spec {
                          name: "Any"
                        }
                      }
                      parameter {
                        name: "q"
                        type_spec {
                          name: "Int"
                        }
                        default_value {
                          kind: EXPR_IDENTIFIER
                          identifier {
                            full_name: "y"
                          }
                        }
                      }
                      result_type {
                        name: "Any"
                      }
                      function_name: "_local_lambda_1"
                      qualified_name {
                        full_name: "with_default_arg._local_lambda_1__i0"
                      }
                      binding {
                        scope_name {
                          name: "with_default_arg::f::f__i0::_local_lambda_1::_local_lambda_1__i0__bind_1"
                        }
                        kind: OBJ_LAMBDA
                        parameter {
                          name: "p"
                          type_spec {
                            name: "Int"
                          }
                        }
                        parameter {
                          name: "q"
                          type_spec {
                            name: "Int"
                          }
                          default_value {
                            kind: EXPR_IDENTIFIER
                            identifier {
                              full_name: "y"
                            }
                          }
                        }
                        result_type {
                          name: "Int"
                        }
                        function_name: "_local_lambda_1"
                        qualified_name {
                          full_name: "with_default_arg._local_lambda_1__i0__bind_1"
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
                                    full_name: "p"
                                  }
                                }
                              }
                              argument {
                                name: "y"
                                value {
                                  kind: EXPR_IDENTIFIER
                                  identifier {
                                    full_name: "q"
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
                binding_type {
                  name: "Function<Generator<Int>(l: Array<Int>, f: Function<Int(p: Int)>)>"
                }
              }
            }
          }
          binding_type {
            name: "Function<Int(l: Generator<Int>)>"
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
          child {
            kind: EXPR_LITERAL
            literal {
              int_value: 4
            }
          }
        }
      }
      argument {
        name: "y"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 3
          }
        }
      }
      binding_type {
        name: "Function<Int(x: Array<Int>, y: Int*)>"
      }
    }
  }
}
