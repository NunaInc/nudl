name: "literal_nullable"
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_LITERAL
    literal {
      null_value: NULL_VALUE
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_LITERAL
    literal {
      int_value: 123
    }
  }
}
