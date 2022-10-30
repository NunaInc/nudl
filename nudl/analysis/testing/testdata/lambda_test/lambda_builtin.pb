name: "lambda_builtin"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "lambda_builtin::ProcessNames::ProcessNames"
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
      full_name: "lambda_builtin.ProcessNames"
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
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "len"
                    }
                  }
                }
                binding_type {
                  name: "Function<Generator<UInt>(l: Array<String>, f: Function<UInt(l: String)>)>"
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
