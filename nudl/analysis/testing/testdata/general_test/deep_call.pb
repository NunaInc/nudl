name: "deep_call"
expression {
  kind: EXPR_SCHEMA_DEF
}
expression {
  kind: EXPR_SCHEMA_DEF
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "deep_call::f::f"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "{ Foo : Foo<{ Bar : Bar<String> }> }"
      }
    }
    result_type {
      name: "{ Foo : Foo<{ Bar : Bar<String> }> }"
    }
    function_name: "f"
    qualified_name {
      full_name: "deep_call.f"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "x"
        }
      }
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "deep_call::g::g"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "{ Foo : Foo<{ Bar : Bar<String> }> }"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "g"
    qualified_name {
      full_name: "deep_call.g"
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
                kind: EXPR_DOT_ACCESS
                child {
                  kind: EXPR_FUNCTION_CALL
                  type_spec {
                    name: "{ Foo : Foo<{ Bar : Bar<String> }> }"
                  }
                  call_spec {
                    left_expression {
                      kind: EXPR_IDENTIFIER
                      identifier {
                        full_name: "f"
                      }
                    }
                    argument {
                      name: "x"
                      value {
                        kind: EXPR_IDENTIFIER
                        identifier {
                          full_name: "x"
                        }
                      }
                    }
                    binding_type {
                      name: "Function<{ Foo : Foo<{ Bar : Bar<String> }> }(x: { Foo : Foo<{ Bar : Bar<String> }> })>"
                    }
                  }
                }
              }
            }
          }
          binding_type {
            name: "Function<UInt(l: String)>"
          }
        }
      }
    }
  }
}
