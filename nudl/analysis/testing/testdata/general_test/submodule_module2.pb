name: "submodule_module2"
expression {
  kind: EXPR_IMPORT_STATEMENT
  import_spec {
    local_name: "submodule"
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Float64"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "submodule.area"
        }
      }
      argument {
        name: "radius"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 10
          }
        }
      }
      binding_type {
        name: "Function<Float64(radius: Float64)>"
      }
    }
  }
}
expression {
  kind: EXPR_IMPORT_STATEMENT
  import_spec {
    local_name: "submodule.compute"
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Float64"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "submodule.compute.square_circle_area"
        }
      }
      argument {
        name: "radius"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 10
          }
        }
      }
      binding_type {
        name: "Function<Float64(radius: Float64)>"
      }
    }
  }
}
