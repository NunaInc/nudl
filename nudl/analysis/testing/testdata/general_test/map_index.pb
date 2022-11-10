name: "map_index"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "map_index::Access::Access__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "s"
      type_spec {
        name: "Map<String, Int>"
      }
    }
    parameter {
      name: "n"
      type_spec {
        name: "String"
      }
    }
    result_type {
      name: "Nullable<Int>"
    }
    function_name: "Access"
    qualified_name {
      full_name: "map_index.Access__i0"
    }
    body {
      kind: EXPR_BLOCK
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
