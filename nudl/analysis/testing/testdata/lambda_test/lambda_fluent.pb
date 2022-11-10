name: "lambda_fluent"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "lambda_fluent::ProcessNames::ProcessNames__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "names"
      type_spec {
        name: "Array<String>"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "ProcessNames"
    qualified_name {
      full_name: "lambda_fluent.ProcessNames__i0"
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
                        name: "lambda_fluent::ProcessNames::ProcessNames__i0::_local_lambda_1::_local_lambda_1__i0__bind_1"
                      }
                      kind: OBJ_LAMBDA
                      parameter {
                        name: "s"
                        type_spec {
                          name: "String"
                        }
                      }
                      result_type {
                        name: "UInt"
                      }
                      function_name: "_local_lambda_1"
                      qualified_name {
                        full_name: "lambda_fluent._local_lambda_1__i0__bind_1"
                      }
                      body {
                        kind: EXPR_BLOCK
                        child {
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
