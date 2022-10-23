name: "deep_call_object"
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
      name: "deep_call_object::g::g"
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
      full_name: "deep_call_object.g"
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
