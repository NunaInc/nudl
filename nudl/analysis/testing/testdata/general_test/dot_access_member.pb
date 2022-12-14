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
      name: "dot_access_member::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "names"
      type_spec {
        name: "Array<HumanName>"
      }
    }
    result_type {
      name: "HumanName"
    }
    function_name: "f"
    qualified_name {
      full_name: "dot_access_member.f__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "HumanName"
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
                name: "Nullable<HumanName>"
              }
              call_spec {
                call_name {
                  full_name: "front__i0"
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
                  name: "Function<Nullable<HumanName>(l: Array<HumanName>)>"
                }
              }
            }
          }
          binding_type {
            name: "Function<HumanName(x: Nullable<HumanName>)>"
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
      name: "dot_access_member::g::g__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "names"
      type_spec {
        name: "Array<HumanName>"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "g"
    qualified_name {
      full_name: "dot_access_member.g__i0"
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
            full_name: "len__i0"
          }
          is_method: true
          argument {
            name: "l"
            value {
              kind: EXPR_DOT_ACCESS
              child {
                kind: EXPR_FUNCTION_CALL
                type_spec {
                  name: "HumanName"
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
                    name: "Function<HumanName(names: Array<HumanName>)>"
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
