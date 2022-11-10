name: "named_tuples"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "named_tuples::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "f"
    qualified_name {
      full_name: "named_tuples.f__i0"
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_TUPLE_DEF
    child {
      kind: EXPR_LITERAL
      literal {
        int_value: 3
      }
    }
    child {
      kind: EXPR_LITERAL
      literal {
        str_value: "foo"
      }
    }
    child {
      kind: EXPR_IDENTIFIER
      identifier {
        full_name: "f"
      }
    }
    type_spec {
      name: "Tuple<a: Int, b: String, c: Function<Any(x: Any)>>"
    }
    tuple_def {
      element {
        name: "a"
      }
      element {
        name: "b"
      }
      element {
        name: "c"
      }
    }
  }
}
