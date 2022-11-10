name: "import_alias"
expression {
  kind: EXPR_IMPORT_STATEMENT
  import_spec {
    local_name: "foo"
    is_alias: true
  }
}
expression {
  kind: EXPR_SCHEMA_DEF
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "import_alias::_init_object_Foo::_init_object_Foo__i0"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "bar"
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
    parameter {
      name: "baz"
      type_spec {
        name: "Nullable<Int>"
      }
      default_value {
        kind: EXPR_LITERAL
        literal {
          null_value: NULL_VALUE
        }
      }
    }
    parameter {
      name: "qux"
      type_spec {
        name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
      }
      default_value {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
        }
        call_spec {
          call_name {
            full_name: "cdm._init_object_HumanName__i0"
          }
          is_method: true
          argument {
            name: "use"
            value {
              kind: EXPR_LITERAL
              literal {
                null_value: NULL_VALUE
              }
            }
          }
          argument {
            name: "family"
            value {
              kind: EXPR_LITERAL
              literal {
                null_value: NULL_VALUE
              }
            }
          }
          argument {
            name: "given"
            value {
              kind: EXPR_LITERAL
              literal {
                null_value: NULL_VALUE
              }
            }
          }
          argument {
            name: "prefix"
            value {
              kind: EXPR_EMPTY_STRUCT
            }
          }
          argument {
            name: "suffix"
            value {
              kind: EXPR_EMPTY_STRUCT
            }
          }
          binding_type {
            name: "Function<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }(use: Null, family: Null, given: Null, prefix: Array<String>, suffix: Array<String>)>"
          }
        }
      }
    }
    result_type {
      name: "{ Foo : Foo<String, Nullable<Int>, { HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }> }"
    }
    function_name: "_init_object_Foo"
    qualified_name {
      full_name: "import_alias._init_object_Foo__i0"
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
      name: "import_alias::_init_copy_Foo::_init_copy_Foo__i0"
    }
    kind: OBJ_CONSTRUCTOR
    parameter {
      name: "obj"
      type_spec {
        name: "{ Foo : Foo<String, Nullable<Int>, { HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }> }"
      }
    }
    result_type {
      name: "{ Foo : Foo<String, Nullable<Int>, { HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }> }"
    }
    function_name: "_init_copy_Foo"
    qualified_name {
      full_name: "import_alias._init_copy_Foo__i0"
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
      name: "import_alias::f::f__i0"
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
    function_name: "f"
    qualified_name {
      full_name: "import_alias.f__i0"
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
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "import_alias::g::g__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "{ Foo : Foo<String, Nullable<Int>, { HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }> }"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "g"
    qualified_name {
      full_name: "import_alias.g__i0"
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
              full_name: "len"
            }
          }
          argument {
            name: "l"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x.qux.prefix"
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
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "import_alias::h::h__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "{ Foo : Foo<String, Nullable<Int>, { HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }> }"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "h"
    qualified_name {
      full_name: "import_alias.h__i0"
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
              full_name: "len"
            }
          }
          argument {
            name: "l"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x.qux.prefix"
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
