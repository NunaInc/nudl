name: "set_index"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "set_index::Access::Access"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "s"
      type_spec {
        name: "Set<String>"
      }
    }
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
      full_name: "set_index.Access"
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
