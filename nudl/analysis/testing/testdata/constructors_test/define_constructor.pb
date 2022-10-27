name: "define_constructor"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "define_constructor::date_to_string::date_to_string"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "date"
      type_spec {
        name: "Date"
      }
    }
    result_type {
      name: "String"
    }
    function_name: "date_to_string"
    qualified_name {
      full_name: "define_constructor.date_to_string"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "String"
        }
        call_spec {
          call_name {
            full_name: "concat"
          }
          is_method: true
          argument {
            name: "x"
            value {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Generator<String>"
              }
              call_spec {
                call_name {
                  full_name: "map"
                }
                is_method: true
                argument {
                  name: "l"
                  value {
                    kind: EXPR_ARRAY_DEF
                    child {
                      kind: EXPR_FUNCTION_CALL
                      type_spec {
                        name: "Int"
                      }
                      call_spec {
                        call_name {
                          full_name: "year"
                        }
                        is_method: true
                        argument {
                          name: "date"
                          value {
                            kind: EXPR_IDENTIFIER
                            identifier {
                              full_name: "date"
                            }
                          }
                        }
                        binding_type {
                          name: "Function<Int(date: Date)>"
                        }
                      }
                    }
                    child {
                      kind: EXPR_FUNCTION_CALL
                      type_spec {
                        name: "Int"
                      }
                      call_spec {
                        call_name {
                          full_name: "month"
                        }
                        is_method: true
                        argument {
                          name: "date"
                          value {
                            kind: EXPR_IDENTIFIER
                            identifier {
                              full_name: "date"
                            }
                          }
                        }
                        binding_type {
                          name: "Function<Int(date: Date)>"
                        }
                      }
                    }
                    child {
                      kind: EXPR_FUNCTION_CALL
                      type_spec {
                        name: "Int"
                      }
                      call_spec {
                        call_name {
                          full_name: "day"
                        }
                        is_method: true
                        argument {
                          name: "date"
                          value {
                            kind: EXPR_IDENTIFIER
                            identifier {
                              full_name: "date"
                            }
                          }
                        }
                        binding_type {
                          name: "Function<Int(date: Date)>"
                        }
                      }
                    }
                  }
                }
                argument {
                  name: "f"
                  value {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "to_string"
                    }
                  }
                }
                binding_type {
                  name: "Function<Generator<String>(l: Array<Int>, f: Function<String(arg_1: Int)>)>"
                }
              }
            }
          }
          argument {
            name: "joiner"
            value {
              kind: EXPR_LITERAL
              literal {
                str_value: "/"
              }
            }
          }
          binding_type {
            name: "Function<String(x: Generator<String>, joiner: String)>"
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
      name: "String"
    }
    call_spec {
      call_name {
        full_name: "define_constructor.date_to_string"
      }
      is_method: true
      argument {
        name: "date"
        value {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Date"
          }
          call_spec {
            call_name {
              full_name: "date"
            }
            is_method: true
            binding_type {
              name: "Function<Date()>"
            }
          }
        }
      }
      binding_type {
        name: "Function<String(date: Date)>"
      }
    }
  }
}
