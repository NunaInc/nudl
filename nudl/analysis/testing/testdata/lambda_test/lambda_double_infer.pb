name: "lambda_double_infer"
expression {
  kind: EXPR_IMPORT_STATEMENT
  import_spec {
    local_name: "cdm"
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "lambda_double_infer::ProcessNames::ProcessNames"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "names"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "ProcessNames"
    qualified_name {
      full_name: "lambda_double_infer.ProcessNames"
    }
    binding {
      scope_name {
        name: "lambda_double_infer::ProcessNames::ProcessNames__bind_1"
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
        full_name: "lambda_double_infer.ProcessNames__bind_1"
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
                  name: "Generator<UInt>"
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
                          name: "lambda_double_infer::ProcessNames::ProcessNames__bind_1::__local_lambda_1::__local_lambda_1"
                        }
                        kind: OBJ_LAMBDA
                        parameter {
                          name: "s"
                          type_spec {
                            name: "Any"
                          }
                        }
                        result_type {
                          name: "Any"
                        }
                        function_name: "__local_lambda_1"
                        qualified_name {
                          full_name: "lambda_double_infer.__local_lambda_1"
                        }
                        binding {
                          scope_name {
                            name: "lambda_double_infer::ProcessNames::ProcessNames__bind_1::__local_lambda_1::__local_lambda_1__bind_1"
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
                            full_name: "lambda_double_infer.__local_lambda_1__bind_1"
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
                  }
                  binding_type {
                    name: "Function<Generator<UInt>(l: Array<String>, f: Function<UInt(arg_1: String)>)>"
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
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "lambda_double_infer::UseProcessNames::UseProcessNames"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "name"
      type_spec {
        name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "UseProcessNames"
    qualified_name {
      full_name: "lambda_double_infer.UseProcessNames"
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
              full_name: "ProcessNames"
            }
          }
          argument {
            name: "names"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "name.prefix"
              }
            }
          }
          binding_type {
            name: "Function<UInt(names: Array<String>)>"
          }
        }
      }
    }
  }
}
