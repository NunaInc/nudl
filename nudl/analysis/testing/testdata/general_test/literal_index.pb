name: "literal_index"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "literal_index::Access::Access__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "n"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Nullable<Int>"
    }
    function_name: "Access"
    qualified_name {
      full_name: "literal_index.Access__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_INDEX
        child {
          kind: EXPR_ARRAY_DEF
          child {
            kind: EXPR_LITERAL
            literal {
              int_value: 1
            }
          }
          child {
            kind: EXPR_LITERAL
            literal {
              int_value: 2
            }
          }
          child {
            kind: EXPR_LITERAL
            literal {
              int_value: 3
            }
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
