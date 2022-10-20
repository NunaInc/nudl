name: "filter_name"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "filter_name::IsDillinger::IsDillinger"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "name"
      type_spec {
        name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
      }
    }
    result_type {
      name: "Bool"
    }
    function_name: "IsDillinger"
    qualified_name {
      full_name: "filter_name.IsDillinger"
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
            full_name: "__and__"
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
                  full_name: "__eq__"
                }
                argument {
                  name: "x"
                  value {
                    kind: EXPR_FUNCTION_CALL
                    type_spec {
                      name: "String"
                    }
                    call_spec {
                      left_expression {
                        kind: EXPR_IDENTIFIER
                        identifier {
                          full_name: "ensure"
                        }
                      }
                      argument {
                        name: "x"
                        value {
                          kind: EXPR_IDENTIFIER
                          identifier {
                            full_name: "name.family"
                          }
                        }
                      }
                      argument {
                        name: "val"
                        value {
                          kind: EXPR_LITERAL
                          literal {
                            str_value: ""
                          }
                        }
                      }
                      binding_type {
                        name: "Function<String(x: Nullable<String>, val: String*)>"
                      }
                    }
                  }
                }
                argument {
                  name: "y"
                  value {
                    kind: EXPR_LITERAL
                    literal {
                      str_value: "Dillinger"
                    }
                  }
                }
                binding_type {
                  name: "Function<Bool(x: String, y: String)>"
                }
              }
            }
          }
          argument {
            name: "y"
            value {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Bool"
              }
              call_spec {
                call_name {
                  full_name: "__eq__"
                }
                argument {
                  name: "x"
                  value {
                    kind: EXPR_FUNCTION_CALL
                    type_spec {
                      name: "String"
                    }
                    call_spec {
                      left_expression {
                        kind: EXPR_IDENTIFIER
                        identifier {
                          full_name: "ensure"
                        }
                      }
                      argument {
                        name: "x"
                        value {
                          kind: EXPR_IDENTIFIER
                          identifier {
                            full_name: "name.given"
                          }
                        }
                      }
                      argument {
                        name: "val"
                        value {
                          kind: EXPR_LITERAL
                          literal {
                            str_value: ""
                          }
                        }
                      }
                      binding_type {
                        name: "Function<String(x: Nullable<String>, val: String*)>"
                      }
                    }
                  }
                }
                argument {
                  name: "y"
                  value {
                    kind: EXPR_LITERAL
                    literal {
                      str_value: "John"
                    }
                  }
                }
                binding_type {
                  name: "Function<Bool(x: String, y: String)>"
                }
              }
            }
          }
          binding_type {
            name: "Function<Bool(x: Bool, y: Bool)>"
          }
        }
      }
    }
  }
}
