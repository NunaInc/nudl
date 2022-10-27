name: "typedef_submodule"
expression {
  kind: EXPR_IMPORT_STATEMENT
  import_spec {
    local_name: "submodule.compute"
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_ARRAY_DEF
    child {
      kind: EXPR_LITERAL
      literal {
        double_value: 1
      }
    }
    child {
      kind: EXPR_LITERAL
      literal {
        double_value: 2
      }
    }
    child {
      kind: EXPR_LITERAL
      literal {
        double_value: 3
      }
    }
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
          full_name: "submodule.compute.sum_area"
        }
      }
      argument {
        name: "radii"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "f"
          }
        }
      }
      binding_type {
        name: "Function<Float64(radii: { RadiusArray : Array<Float64> })>"
      }
    }
  }
}
