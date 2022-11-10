name: "not_too_many_binds"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "not_too_many_binds::f::f__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Numeric"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "Int"
      }
    }
    result_type {
      name: "Numeric"
    }
    function_name: "f"
    qualified_name {
      full_name: "not_too_many_binds.f__i0"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Numeric"
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
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "y"
              }
            }
          }
          binding_type {
            name: "Function<Numeric(x: Numeric, y: Numeric)>"
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
      name: "not_too_many_binds::f::f__i1"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Int"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "Numeric"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "f"
    qualified_name {
      full_name: "not_too_many_binds.f__i1"
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
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "y"
              }
            }
          }
          binding_type {
            name: "Function<Int(x: Int, y: Int)>"
          }
        }
      }
    }
    binding {
      scope_name {
        name: "not_too_many_binds::f::f__i1__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "x"
        type_spec {
          name: "Int"
        }
      }
      parameter {
        name: "y"
        type_spec {
          name: "Int"
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "f"
      qualified_name {
        full_name: "not_too_many_binds.f__i1__bind_1"
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
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "y"
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
          full_name: "f"
        }
      }
      argument {
        name: "x"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 1
          }
        }
      }
      argument {
        name: "y"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 2
          }
        }
      }
      binding_type {
        name: "Function<Int(x: Int, y: Int)>"
      }
    }
  }
}
