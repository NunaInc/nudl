name: "native_function"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "native_function::f::f__i0"
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
      full_name: "native_function.f__i0"
    }
    native_snippet {
      name: "pyinline"
      body: "${x}"
    }
  }
}
