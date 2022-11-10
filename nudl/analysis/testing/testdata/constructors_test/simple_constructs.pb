name: "simple_constructs"
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Int"
    }
    call_spec {
      call_name {
        full_name: "int__i1"
      }
      is_method: true
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            double_value: 3.2
          }
        }
      }
      binding_type {
        name: "Function<Int(x: Float64)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Bool"
    }
    call_spec {
      call_name {
        full_name: "bool__i1"
      }
      is_method: true
      argument {
        name: "x"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "x"
          }
        }
      }
      binding_type {
        name: "Function<Bool(x: Int)>"
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
      call_name {
        full_name: "int__i0"
      }
      is_method: true
      binding_type {
        name: "Function<Int()>"
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
      call_name {
        full_name: "int__i2"
      }
      is_method: true
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            str_value: "2"
          }
        }
      }
      argument {
        name: "default"
        value {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Int"
          }
          call_spec {
            call_name {
              full_name: "int__i0"
            }
            is_method: true
            binding_type {
              name: "Function<Int()>"
            }
          }
        }
      }
      binding_type {
        name: "Function<Int(x: String, default: Int)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Nullable<Int>"
    }
    call_spec {
      call_name {
        full_name: "int__i3"
      }
      is_method: true
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            str_value: "2"
          }
        }
      }
      binding_type {
        name: "Function<Nullable<Int>(x: String)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Nullable<Int>"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "int"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            str_value: "2"
          }
        }
      }
      binding_type {
        name: "Function<Nullable<Int>(x: String)>"
      }
    }
  }
}
