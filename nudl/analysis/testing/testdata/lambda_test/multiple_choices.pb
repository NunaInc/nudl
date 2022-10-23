name: "multiple_choices"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "multiple_choices::f::f"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Function<{ X : Any }(arg_1: { X : Any })>"
      }
    }
    parameter {
      name: "val"
      type_spec {
        name: "{ X : Any }"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "f"
    qualified_name {
      full_name: "multiple_choices.f"
    }
    binding {
      scope_name {
        name: "multiple_choices::f::f__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "x"
        type_spec {
          name: "Function<Int(arg_1: Int)>"
        }
      }
      parameter {
        name: "val"
        type_spec {
          name: "Int"
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "f"
      qualified_name {
        full_name: "multiple_choices.f__bind_1"
      }
      body {
        kind: EXPR_BLOCK
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Int"
          }
          call_spec {
            left_expression {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x"
              }
            }
            argument {
              name: "arg_1"
              value {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "val"
                }
              }
            }
            binding_type {
              name: "Function<Int(arg_1: Int)>"
            }
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
      name: "multiple_choices::g::g"
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
      default_value {
        kind: EXPR_LITERAL
        literal {
          int_value: 1
        }
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "g"
    qualified_name {
      full_name: "multiple_choices.g"
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
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "multiple_choices::g::g__1__"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "Float64"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "Float64"
      }
      default_value {
        kind: EXPR_LITERAL
        literal {
          double_value: 1.2
        }
      }
    }
    result_type {
      name: "Float64"
    }
    function_name: "g"
    qualified_name {
      full_name: "multiple_choices.g__1__"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_FUNCTION_CALL
        type_spec {
          name: "Float64"
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
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "y"
              }
            }
          }
          binding_type {
            name: "Function<Float64(x: Float64, y: Float64)>"
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
      name: "multiple_choices::h::h"
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
    function_name: "h"
    qualified_name {
      full_name: "multiple_choices.h"
    }
    body {
      kind: EXPR_BLOCK
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
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "g.g"
              }
            }
          }
          argument {
            name: "val"
            value {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "x"
              }
            }
          }
          binding_type {
            name: "Function<Int(x: Function<Int(arg_1: Int)>, val: Int)>"
          }
        }
      }
    }
  }
}
