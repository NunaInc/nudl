name: "deep_call_object"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "deep_call_object::_init_object_Bar::_init_object_Bar__i0"
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
      name: "Bar"
    }
    function_name: "_init_object_Bar"
    qualified_name {
      full_name: "deep_call_object._init_object_Bar__i0"
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
      name: "deep_call_object::_init_copy_Bar::_init_copy_Bar__i0"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "obj"
      type_spec {
        name: "Bar"
      }
    }
    result_type {
      name: "Bar"
    }
    function_name: "_init_copy_Bar"
    qualified_name {
      full_name: "deep_call_object._init_copy_Bar__i0"
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
      name: "deep_call_object::_init_object_Foo::_init_object_Foo__i0"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "name"
      type_spec {
        name: "Bar"
      }
      default_value {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Bar"
        }
        call_spec {
          call_name {
            full_name: "deep_call_object._init_object_Bar__i0"
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
            name: "Function<Bar(subname: String*)>"
          }
        }
      }
    }
    result_type {
      name: "Foo"
    }
    function_name: "_init_object_Foo"
    qualified_name {
      full_name: "deep_call_object._init_object_Foo__i0"
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
      name: "deep_call_object::_init_copy_Foo::_init_copy_Foo__i0"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "obj"
      type_spec {
        name: "Foo"
      }
    }
    result_type {
      name: "Foo"
    }
    function_name: "_init_copy_Foo"
    qualified_name {
      full_name: "deep_call_object._init_copy_Foo__i0"
    }
    native_snippet {
      name: "__struct_copy_constructor__"
      body: "Foo"
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
      name: "deep_call_object::g::g__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Foo"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "g"
    qualified_name {
      full_name: "deep_call_object.g__i0"
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
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x.name.subname"
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
