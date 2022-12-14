name: "max_termination"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "max_termination::MaxTermination::MaxTermination__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "name"
      type_spec {
        name: "HumanName"
      }
    }
    result_type {
      name: "Nullable<UInt>"
    }
    function_name: "MaxTermination"
    qualified_name {
      full_name: "max_termination.MaxTermination__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Nullable<UInt>"
        }
        call_spec {
          left_expression {
            kind: EXPR_IDENTIFIER
            identifier {
              full_name: "max"
            }
          }
          argument {
            name: "l"
            value {
              kind: EXPR_ARRAY_DEF
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
                        full_name: "name.prefix"
                      }
                    }
                  }
                  binding_type {
                    name: "Function<UInt(l: Array<String>)>"
                  }
                }
              }
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
                        full_name: "name.suffix"
                      }
                    }
                  }
                  binding_type {
                    name: "Function<UInt(l: Array<String>)>"
                  }
                }
              }
            }
          }
          binding_type {
            name: "Function<Nullable<UInt>(l: Array<UInt>)>"
          }
        }
      }
    }
  }
}
