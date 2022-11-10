name: "tuple_index_uint"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "tuple_index_uint::Access::Access__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "s"
      type_spec {
        name: "Tuple<String, Int>"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "Access"
    qualified_name {
      full_name: "tuple_index_uint.Access__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_TUPLE_INDEX
        child {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "s"
          }
        }
        child {
          kind: EXPR_LITERAL
          literal {
            uint_value: 1
          }
        }
      }
    }
  }
}
