name: "lambda_simple"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "lambda_simple::ProcessNames::ProcessNames"
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
      full_name: "lambda_simple.ProcessNames"
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
              full_name: "sum"
            }
          }
          argument {
            name: "l"
            value {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Array<UInt>"
              }
              call_spec {
                left_expression {
                  kind: EXPR_IDENTIFIER
                  identifier {
                    full_name: "map"
                  }
                }
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
                        name: "lambda_simple::ProcessNames::ProcessNames::__local_lambda_1::__local_lambda_1"
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
                      function_name: "__local_lambda_1"
                      qualified_name {
                        full_name: "lambda_simple.__local_lambda_1"
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
                  name: "Function<Array<UInt>(l: Array<String>, f: Function<UInt(arg_1: String)>)>"
                }
              }
            }
          }
          binding_type {
            name: "Function<UInt(l: Array<UInt>)>"
          }
        }
      }
    }
  }
}
