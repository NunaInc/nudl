name: "pragma_log_module_names"
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
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "pragma_log_module_names::f::f"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Any"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "f"
    qualified_name {
      full_name: "pragma_log_module_names.f"
    }
  }
}
expression {
  kind: EXPR_NOP
}
