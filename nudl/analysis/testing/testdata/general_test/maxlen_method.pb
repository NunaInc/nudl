name: "maxlen_method"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "maxlen_method::maxlen::maxlen"
    }
    kind: OBJ_METHOD
    parameter {
      name: "l"
      type_spec {
        name: "Iterable<Container<{ X : Any }>>"
      }
    }
    result_type {
      name: "Nullable<UInt>"
    }
    function_name: "maxlen"
    qualified_name {
      full_name: "maxlen_method.maxlen"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Nullable<UInt>"
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
                name: "Generator<UInt>"
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
                  name: "Function<Generator<UInt>(l: Iterable<Container<{ X : Any }>>, f: Function<UInt(l: Container<{ X : Any }>)>)>"
                }
              }
            }
          }
          binding_type {
            name: "Function<Nullable<UInt>(l: Generator<UInt>)>"
          }
        }
      }
    }
    binding {
      scope_name {
        name: "maxlen_method::maxlen::maxlen__bind_1"
      }
      kind: OBJ_METHOD
      parameter {
        name: "l"
        type_spec {
          name: "Array<Array<String>>"
        }
      }
      result_type {
        name: "Nullable<UInt>"
      }
      function_name: "maxlen"
      qualified_name {
        full_name: "maxlen_method.maxlen__bind_1"
      }
      body {
        kind: EXPR_BLOCK
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Nullable<UInt>"
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
                  name: "Generator<UInt>"
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
                    name: "Function<Generator<UInt>(l: Array<Array<String>>, f: Function<UInt(l: Array<String>)>)>"
                  }
                }
              }
            }
            binding_type {
              name: "Function<Nullable<UInt>(l: Generator<UInt>)>"
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
      name: "maxlen_method::MaxTermination::MaxTermination"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "name"
      type_spec {
        name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
      }
    }
    result_type {
      name: "Nullable<UInt>"
    }
    function_name: "MaxTermination"
    qualified_name {
      full_name: "maxlen_method.MaxTermination"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Nullable<UInt>"
        }
        call_spec {
          call_name {
            full_name: "maxlen_method.maxlen__bind_1"
          }
          is_method: true
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
            name: "Function<Nullable<UInt>(l: Array<Array<String>>)>"
          }
        }
      }
    }
  }
}
