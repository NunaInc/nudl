name: "function_member"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "function_member::f::f"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "name"
      type_spec {
        name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
      }
    }
    result_type {
      name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
    }
    function_name: "f"
    qualified_name {
      full_name: "function_member.f"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "name"
        }
      }
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "function_member::g::g"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "name"
      type_spec {
        name: "{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }"
      }
    }
    result_type {
      name: "Nullable<String>"
    }
    function_name: "g"
    qualified_name {
      full_name: "function_member.g"
    }
    body {
      kind: EXPR_BLOCK
      child {
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
              name: "name"
              value {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "name"
                }
              }
            }
            binding_type {
              name: "Function<{ HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> }(name: { HumanName : HumanName<Nullable<String>, Nullable<String>, Nullable<String>, Array<String>, Array<String>> })>"
            }
          }
        }
      }
    }
  }
}
