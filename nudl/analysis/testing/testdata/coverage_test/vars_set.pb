name: "vars_set"
expression {
  kind: EXPR_IMPORT_STATEMENT
  import_spec {
    local_name: "cdm"
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "vars_set::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "name"
      type_spec {
        name: "HumanName"
      }
    }
    result_type {
      name: "Nullable<String>"
    }
    function_name: "f"
    qualified_name {
      full_name: "vars_set.f__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_ASSIGNMENT
        child {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "name"
          }
        }
      }
      child {
        kind: EXPR_ASSIGNMENT
        child {
          kind: EXPR_LITERAL
          literal {
            str_value: "Hammurabi"
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
      name: "vars_set::foo::foo__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "HumanName"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "HumanName"
      }
    }
    result_type {
      name: "UInt"
    }
    function_name: "foo"
    qualified_name {
      full_name: "vars_set.foo__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_ASSIGNMENT
        child {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "y.prefix"
          }
        }
      }
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "UInt"
        }
        call_spec {
          left_expression {
            kind: EXPR_IDENTIFIER
            identifier {
              full_name: "len"
            }
          }
          argument {
            name: "l"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x.prefix"
              }
            }
          }
          binding_type {
            name: "Function<UInt(l: Array<String>)>"
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
      name: "vars_set::g::g__i0"
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
    function_name: "g"
    qualified_name {
      full_name: "vars_set.g__i0"
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
            full_name: "__add____i0"
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
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "HumanName"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "_ensured"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "y"
          }
        }
      }
      binding_type {
        name: "Function<HumanName(x: Nullable<HumanName>)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_LITERAL
    literal {
      int_value: 10
    }
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
