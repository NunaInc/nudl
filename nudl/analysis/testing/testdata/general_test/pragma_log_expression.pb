name: "pragma_log_expression"
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_LITERAL
    literal {
      int_value: 33
    }
  }
}
expression {
  kind: EXPR_NOP
  child {
    kind: EXPR_IDENTIFIER
    identifier {
      full_name: "my_const"
    }
  }
}
