name: "lambda_default_value"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "lambda_default_value::FilterName::FilterName__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "names"
      type_spec {
        name: "Array<String>"
      }
    }
    parameter {
      name: "extra"
      type_spec {
        name: "String"
      }
    }
    result_type {
      name: "Bool"
    }
    function_name: "FilterName"
    qualified_name {
      full_name: "lambda_default_value.FilterName__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_ASSIGNMENT
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Generator<String>"
          }
          call_spec {
            call_name {
              full_name: "filter__i0"
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
                    name: "lambda_default_value::FilterName::FilterName__i0::_local_lambda_1::_local_lambda_1__i0"
                  }
                  kind: OBJ_LAMBDA
                  parameter {
                    name: "name"
                    type_spec {
                      name: "Any"
                    }
                  }
                  parameter {
                    name: "arg"
                    type_spec {
                      name: "String"
                    }
                    default_value {
                      kind: EXPR_IDENTIFIER
                      identifier {
                        full_name: "extra"
                      }
                    }
                  }
                  result_type {
                    name: "Any"
                  }
                  function_name: "_local_lambda_1"
                  qualified_name {
                    full_name: "lambda_default_value._local_lambda_1__i0"
                  }
                  binding {
                    scope_name {
                      name: "lambda_default_value::FilterName::FilterName__i0::_local_lambda_1::_local_lambda_1__i0__bind_1"
                    }
                    kind: OBJ_LAMBDA
                    parameter {
                      name: "name"
                      type_spec {
                        name: "String"
                      }
                    }
                    parameter {
                      name: "arg"
                      type_spec {
                        name: "String"
                      }
                      default_value {
                        kind: EXPR_IDENTIFIER
                        identifier {
                          full_name: "extra"
                        }
                      }
                    }
                    result_type {
                      name: "Bool"
                    }
                    function_name: "_local_lambda_1"
                    qualified_name {
                      full_name: "lambda_default_value._local_lambda_1__i0__bind_1"
                    }
                    body {
                      kind: EXPR_BLOCK
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
                                      full_name: "name"
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
                                      full_name: "arg"
                                    }
                                  }
                                }
                                binding_type {
                                  name: "Function<UInt(l: String)>"
                                }
                              }
                            }
                          }
                          binding_type {
                            name: "Function<Bool(x: UInt, y: UInt)>"
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
            binding_type {
              name: "Function<Generator<String>(l: Array<String>, f: Function<Bool(name: String)>)>"
            }
          }
        }
      }
      child {
        kind: EXPR_FUNCTION_RESULT
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Bool"
          }
          call_spec {
            call_name {
              full_name: "__not____i0"
            }
            argument {
              name: "x"
              value {
                kind: EXPR_FUNCTION_CALL
                type_spec {
                  name: "Bool"
                }
                call_spec {
                  call_name {
                    full_name: "empty__i1"
                  }
                  is_method: true
                  argument {
                    name: "l"
                    value {
                      kind: EXPR_IDENTIFIER
                      identifier {
                        full_name: "filtered"
                      }
                    }
                  }
                  binding_type {
                    name: "Function<Bool(l: Generator<String>)>"
                  }
                }
              }
            }
            binding_type {
              name: "Function<Bool(x: Bool)>"
            }
          }
        }
      }
    }
  }
}
