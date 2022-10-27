name: "typedef_imported"
expression {
  kind: EXPR_IMPORT_STATEMENT
  import_spec {
    local_name: "typedef_templated"
  }
}
expression {
  kind: EXPR_TYPE_DEFINITION
  type_spec {
    name: "{ C : Array<{ I : Int }> }"
  }
  type_def_name: "C"
}
expression {
  kind: EXPR_ASSIGNMENT
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
}
