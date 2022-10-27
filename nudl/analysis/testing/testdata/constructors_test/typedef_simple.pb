name: "typedef_simple"
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
    name: "{ AI : Array<Int> }"
  }
  type_def_name: "AI"
}
expression {
  kind: EXPR_TYPE_DEFINITION
  type_spec {
    name: "{ F : Function<{ I : Int }(arg_1: { AI : Array<Int> })> }"
  }
  type_def_name: "F"
}
