name: "maxlen_untype"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "maxlen_untype::maxlen::maxlen"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "l"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "maxlen"
    qualified_name {
      full_name: "maxlen_untype.maxlen"
    }
    binding {
      scope_name {
        name: "maxlen_untype::maxlen::maxlen__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "l"
        type_spec {
          name: "Array<Array<String>>"
        }
      }
      result_type {
        name: "UInt"
      }
      function_name: "maxlen"
      qualified_name {
        full_name: "maxlen_untype.maxlen__bind_1"
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
              full_name: "max"
            }
            is_method: true
            argument {
              name: "l"
              value {
                kind: EXPR_FUNCTION_CALL
                type_spec {
                  name: "Array<UInt>"
                }
                call_spec {
                  call_name {
                    full_name: "map"
                  }
                  is_method: true
                  argument {
                    name: "l"
                    value {
                      kind: EXPR_IDENTIFIER
                      identifier {
                        full_name: "l"
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
                    name: "Function<Array<UInt>(l: Array<Array<String>>, f: Function<UInt(arg_1: Array<String>)>)>"
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
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "maxlen_untype::MaxTermination::MaxTermination"
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
    function_name: "MaxTermination"
    qualified_name {
      full_name: "maxlen_untype.MaxTermination"
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
              full_name: "maxlen"
            }
          }
          argument {
            name: "l"
            value {
              kind: EXPR_ARRAY_DEF
              child {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "name.prefix"
                }
              }
              child {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "name.suffix"
                }
              }
            }
          }
          binding_type {
            name: "Function<UInt(l: Array<Array<String>>)>"
          }
        }
      }
    }
  }
}
