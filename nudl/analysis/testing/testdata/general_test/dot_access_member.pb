name: "dot_access_member"
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
      name: "dot_access_member::f::f"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "names"
      type_spec {
        name: "Array<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }>"
      }
    }
    result_type {
      name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
    }
    function_name: "f"
    qualified_name {
      full_name: "dot_access_member.f"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
        }
        call_spec {
          left_expression {
            kind: EXPR_IDENTIFIER
            identifier {
              full_name: "_ensured"
            }
          }
          argument {
            name: "x"
            value {
              kind: EXPR_FUNCTION_CALL
              type_spec {
                name: "Nullable<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }>"
              }
              call_spec {
                call_name {
                  full_name: "front"
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
                binding_type {
                  name: "Function<Nullable<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }>(l: Array<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }>)>"
                }
              }
            }
          }
          binding_type {
            name: "Function<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }(x: Nullable<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }>)>"
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
      name: "dot_access_member::g::g"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "names"
      type_spec {
        name: "Array<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }>"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "g"
    qualified_name {
      full_name: "dot_access_member.g"
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
            full_name: "len"
          }
          is_method: true
          argument {
            name: "l"
            value {
              kind: EXPR_DOT_ACCESS
              child {
                kind: EXPR_FUNCTION_CALL
                type_spec {
                  name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
                }
                call_spec {
                  left_expression {
                    kind: EXPR_IDENTIFIER
                    identifier {
                      full_name: "f"
                    }
                  }
                  argument {
                    name: "names"
                    value {
                      kind: EXPR_IDENTIFIER
                      identifier {
                        full_name: "names"
                      }
                    }
                  }
                  binding_type {
                    name: "Function<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }(names: Array<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }>)>"
                  }
                }
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
}
