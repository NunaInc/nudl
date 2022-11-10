name: "lambda_default_value_with_return"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "lambda_default_value_with_return::ProcessNames::ProcessNames__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "names"
      type_spec {
        name: "Array<String>"
      }
    }
    parameter {
      name: "min_len"
      type_spec {
        name: "UInt"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "ProcessNames"
    qualified_name {
      full_name: "lambda_default_value_with_return.ProcessNames__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "UInt"
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
                name: "Generator<UInt>"
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
                      full_name: "names"
                    }
                  }
                }
                argument {
                  name: "f"
                  value {
                    kind: EXPR_LAMBDA
                    function_spec {
                      scope_name {
                        name: "lambda_default_value_with_return::ProcessNames::ProcessNames__i0::_local_lambda_1::_local_lambda_1__i0"
                      }
                      kind: OBJ_LAMBDA
                      parameter {
                        name: "s"
                        type_spec {
                          name: "Any"
                        }
                      }
                      parameter {
                        name: "m"
                        type_spec {
                          name: "UInt"
                        }
                        default_value {
                          kind: EXPR_IDENTIFIER
                          identifier {
                            full_name: "min_len"
                          }
                        }
                      }
                      result_type {
                        name: "Any"
                      }
                      function_name: "_local_lambda_1"
                      qualified_name {
                        full_name: "lambda_default_value_with_return._local_lambda_1__i0"
                      }
                      binding {
                        scope_name {
                          name: "lambda_default_value_with_return::ProcessNames::ProcessNames__i0::_local_lambda_1::_local_lambda_1__i0__bind_1"
                        }
                        kind: OBJ_LAMBDA
                        parameter {
                          name: "s"
                          type_spec {
                            name: "String"
                          }
                        }
                        parameter {
                          name: "m"
                          type_spec {
                            name: "UInt"
                          }
                          default_value {
                            kind: EXPR_IDENTIFIER
                            identifier {
                              full_name: "min_len"
                            }
                          }
                        }
                        result_type {
                          name: "UInt"
                        }
                        function_name: "_local_lambda_1"
                        qualified_name {
                          full_name: "lambda_default_value_with_return._local_lambda_1__i0__bind_1"
                        }
                        body {
                          kind: EXPR_BLOCK
                          child {
                            kind: EXPR_FUNCTION_CALL
                            type_spec {
                              name: "UInt"
                            }
                            call_spec {
                              call_name {
                                full_name: "__sub____i0"
                              }
                              argument {
                                name: "x"
                                value {
                                  kind: EXPR_FUNCTION_CALL
                                  type_spec {
                                    name: "UInt"
                                  }
                                  call_spec {
                                    left_expression {
                                      kind: EXPR_IDENTIFIER
                                      identifier {
                                        full_name: "len"
                                      }
                                    }
                                    argument {
                                      name: "l"
                                      value {
                                        kind: EXPR_IDENTIFIER
                                        identifier {
                                          full_name: "s"
                                        }
                                      }
                                    }
                                    binding_type {
                                      name: "Function<UInt(l: String)>"
                                    }
                                  }
                                }
                              }
                              argument {
                                name: "y"
                                value {
                                  kind: EXPR_IDENTIFIER
                                  identifier {
                                    full_name: "m"
                                  }
                                }
                              }
                              binding_type {
                                name: "Function<UInt(x: UInt, y: UInt)>"
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
                binding_type {
                  name: "Function<Generator<UInt>(l: Array<String>, f: Function<UInt(s: String)>)>"
                }
              }
            }
          }
          binding_type {
            name: "Function<UInt(l: Generator<UInt>)>"
          }
        }
      }
    }
  }
}
