name: "typedef_templated"
expression {
  kind: EXPR_TYPE_DEFINITION
  type_spec {
    name: "{ N : Numeric }"
  }
  type_def_name: "N"
}
expression {
  kind: EXPR_TYPE_DEFINITION
  type_spec {
    name: "{ I : Int }"
  }
  type_def_name: "I"
}
expression {
  kind: EXPR_TYPE_DEFINITION
  type_spec {
    name: "{ A : Array<{ N : Numeric }> }"
  }
  type_def_name: "A"
}
expression {
  kind: EXPR_TYPE_DEFINITION
  type_spec {
    name: "{ B : Array<{ I : Int }> }"
  }
  type_def_name: "B"
}
