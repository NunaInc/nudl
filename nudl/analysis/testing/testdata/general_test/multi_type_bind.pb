name: "multi_type_bind"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "multi_type_bind::f::f"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "f"
    qualified_name {
      full_name: "multi_type_bind.f"
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
            full_name: "__sub__"
          }
          argument {
            name: "x"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x"
              }
            }
          }
          argument {
            name: "y"
            value {
              kind: EXPR_LITERAL
              literal {
                int_value: 1
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
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "multi_type_bind::f::f__1__"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "UInt"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "f"
    qualified_name {
      full_name: "multi_type_bind.f__1__"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "UInt"
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
                full_name: "x"
              }
            }
          }
          argument {
            name: "y"
            value {
              kind: EXPR_LITERAL
              literal {
                int_value: 1
              }
            }
          }
          binding_type {
            name: "Function<UInt(x: UInt, y: UInt)>"
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
          full_name: "f"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 10
          }
        }
      }
      binding_type {
        name: "Function<Int(x: Int)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "UInt"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "f"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            uint_value: 10
          }
        }
      }
      binding_type {
        name: "Function<UInt(x: UInt)>"
      }
    }
  }
}
