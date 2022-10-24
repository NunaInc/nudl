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
      name: "import_alias::f::f"
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
      full_name: "import_alias.f"
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
      name: "import_alias::g::g"
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
      full_name: "import_alias.g"
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
      name: "import_alias::h::h"
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
      full_name: "import_alias.h"
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
