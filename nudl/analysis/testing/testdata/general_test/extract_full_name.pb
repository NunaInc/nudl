name: "extract_full_name"
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
      name: "extract_full_name::ExtractFullName::ExtractFullName"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "name"
      type_spec {
        name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
      }
    }
    result_type {
      name: "String"
    }
    function_name: "ExtractFullName"
    qualified_name {
      full_name: "extract_full_name.ExtractFullName"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "String"
        }
        call_spec {
          left_expression {
            kind: EXPR_IDENTIFIER
            identifier {
              full_name: "concat"
            }
          }
          argument {
            name: "x"
            value {
              kind: EXPR_ARRAY_DEF
              child {
                kind: EXPR_FUNCTION_CALL
                type_spec {
                  name: "String"
                }
                call_spec {
                  left_expression {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "concat"
                    }
                  }
                  argument {
                    name: "x"
                    value {
                      kind: EXPR_IDENTIFIER
                      identifier {
                        full_name: "name.prefix"
                      }
                    }
                  }
                  argument {
                    name: "joiner"
                    value {
                      kind: EXPR_LITERAL
                      literal {
                        str_value: " "
                      }
                    }
                  }
                  binding_type {
                    name: "Function<String(x: Array<String>, joiner: String)>"
                  }
                }
              }
              child {
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
              child {
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
              child {
                kind: EXPR_FUNCTION_CALL
                type_spec {
                  name: "String"
                }
                call_spec {
                  left_expression {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "concat"
                    }
                  }
                  argument {
                    name: "x"
                    value {
                      kind: EXPR_IDENTIFIER
                      identifier {
                        full_name: "name.suffix"
                      }
                    }
                  }
                  argument {
                    name: "joiner"
                    value {
                      kind: EXPR_LITERAL
                      literal {
                        str_value: " "
                      }
                    }
                  }
                  binding_type {
                    name: "Function<String(x: Array<String>, joiner: String)>"
                  }
                }
              }
            }
          }
          argument {
            name: "joiner"
            value {
              kind: EXPR_LITERAL
              literal {
                str_value: " "
              }
            }
          }
          binding_type {
            name: "Function<String(x: Array<String>, joiner: String)>"
          }
        }
      }
    }
  }
}
