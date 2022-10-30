name: "deep_call"
expression {
  kind: EXPR_SCHEMA_DEF
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "deep_call::_init_object_Bar::_init_object_Bar"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "subname"
      type_spec {
        name: "String"
      }
      default_value {
        kind: EXPR_LITERAL
        literal {
          str_value: ""
        }
      }
    }
    result_type {
      name: "{ Bar : Bar<String> }"
    }
    function_name: "_init_object_Bar"
    qualified_name {
      full_name: "deep_call._init_object_Bar"
    }
    native_snippet {
      name: "__struct_object_constructor__"
      body: "Bar"
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "deep_call::_init_copy_Bar::_init_copy_Bar"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "obj"
      type_spec {
        name: "{ Bar : Bar<String> }"
      }
    }
    result_type {
      name: "{ Bar : Bar<String> }"
    }
    function_name: "_init_copy_Bar"
    qualified_name {
      full_name: "deep_call._init_copy_Bar"
    }
    native_snippet {
      name: "__struct_copy_constructor__"
      body: "Bar"
    }
  }
}
expression {
  kind: EXPR_SCHEMA_DEF
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "deep_call::_init_object_Foo::_init_object_Foo"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "name"
      type_spec {
        name: "{ Bar : Bar<String> }"
      }
      default_value {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "{ Bar : Bar<String> }"
        }
        call_spec {
          call_name {
            full_name: "deep_call._init_object_Bar"
          }
          is_method: true
          argument {
            name: "subname"
            value {
              kind: EXPR_LITERAL
              literal {
                str_value: ""
              }
            }
          }
          binding_type {
            name: "Function<{ Bar : Bar<String> }(subname: String*)>"
          }
        }
      }
    }
    result_type {
      name: "{ Foo : Foo<{ Bar : Bar<String> }> }"
    }
    function_name: "_init_object_Foo"
    qualified_name {
      full_name: "deep_call._init_object_Foo"
    }
    native_snippet {
      name: "__struct_object_constructor__"
      body: "Foo"
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "deep_call::_init_copy_Foo::_init_copy_Foo"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "obj"
      type_spec {
        name: "{ Foo : Foo<{ Bar : Bar<String> }> }"
      }
    }
    result_type {
      name: "{ Foo : Foo<{ Bar : Bar<String> }> }"
    }
    function_name: "_init_copy_Foo"
    qualified_name {
      full_name: "deep_call._init_copy_Foo"
    }
    native_snippet {
      name: "__struct_copy_constructor__"
      body: "Foo"
    }
  }
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
