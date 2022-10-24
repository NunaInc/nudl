name: "literal_set_index"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "literal_set_index::Access::Access"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "n"
      type_spec {
        name: "String"
      }
    }
    result_type {
      name: "Bool"
    }
    function_name: "Access"
    qualified_name {
      full_name: "literal_set_index.Access"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_ASSIGNMENT
        child {
          kind: EXPR_ARRAY_DEF
          child {
            kind: EXPR_LITERAL
            literal {
              str_value: "a"
            }
          }
          child {
            kind: EXPR_LITERAL
            literal {
              str_value: "b"
            }
          }
          child {
            kind: EXPR_LITERAL
            literal {
              str_value: "c"
            }
          }
        }
      }
      child {
        kind: EXPR_INDEX
        child {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "s"
          }
        }
        child {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "n"
          }
        }
      }
    }
  }
}
