name: "simple_index"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "simple_index::Access::Access"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "l"
      type_spec {
        name: "Array<String>"
      }
    }
    parameter {
      name: "n"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Nullable<String>"
    }
    function_name: "Access"
    qualified_name {
      full_name: "simple_index.Access"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_INDEX
        child {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "l"
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
