name: "lambda_vars"
expression {
  kind: EXPR_IMPORT_STATEMENT
  import_spec {
    local_name: "cdm"
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_LITERAL
    literal {
      int_value: 20
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "lambda_vars::foo::foo"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "p"
      type_spec {
        name: "Any"
      }
    }
    parameter {
      name: "q"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "foo"
    qualified_name {
      full_name: "lambda_vars.foo"
    }
    binding {
      scope_name {
        name: "lambda_vars::foo::foo__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "p"
        type_spec {
          name: "Int"
        }
      }
      parameter {
        name: "q"
        type_spec {
          name: "Int"
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "foo"
      qualified_name {
        full_name: "lambda_vars.foo__bind_1"
      }
      body {
        kind: EXPR_BLOCK
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Int"
          }
          call_spec {
            call_name {
              full_name: "__add__"
            }
            argument {
              name: "x"
              value {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "p"
                }
              }
            }
            argument {
              name: "y"
              value {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "q"
                }
              }
            }
            binding_type {
              name: "Function<Int(x: Int, y: Int)>"
            }
          }
        }
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Int"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "foo"
        }
      }
      argument {
        name: "p"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "x"
          }
        }
      }
      argument {
        name: "q"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 30
          }
        }
      }
      binding_type {
        name: "Function<Int(p: Int, q: Int)>"
      }
    }
  }
}
