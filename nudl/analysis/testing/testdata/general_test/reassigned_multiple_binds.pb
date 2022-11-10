name: "reassigned_multiple_binds"
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "reassigned_multiple_binds::ff::ff__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "x"
      type_spec {
        name: "{ T : Any }"
      }
    }
    parameter {
      name: "y"
      type_spec {
        name: "{ T : Any }"
      }
    }
    result_type {
      name: "{ T : Any }"
    }
    function_name: "ff"
    qualified_name {
      full_name: "reassigned_multiple_binds.ff__i0"
    }
    binding {
      scope_name {
        name: "reassigned_multiple_binds::ff::ff__i0__bind_1"
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
      function_name: "ff"
      qualified_name {
        full_name: "reassigned_multiple_binds.ff__i0__bind_1"
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
              full_name: "__mul____i0"
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
    binding {
      scope_name {
        name: "reassigned_multiple_binds::ff::ff__i0__bind_2"
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
      }
      result_type {
        name: "Float64"
      }
      function_name: "ff"
      qualified_name {
        full_name: "reassigned_multiple_binds.ff__i0__bind_2"
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
              full_name: "__mul____i0"
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
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "reassigned_multiple_binds::gg::gg__i0"
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
      name: "Int"
    }
    function_name: "gg"
    qualified_name {
      full_name: "reassigned_multiple_binds.gg__i0"
    }
    binding {
      scope_name {
        name: "reassigned_multiple_binds::gg::gg__i0__bind_1"
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
      function_name: "gg"
      qualified_name {
        full_name: "reassigned_multiple_binds.gg__i0__bind_1"
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
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "reassigned_multiple_binds::main::main__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "f"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "main"
    qualified_name {
      full_name: "reassigned_multiple_binds.main__i0"
    }
    binding {
      scope_name {
        name: "reassigned_multiple_binds::main::main__i0__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "f"
        type_spec {
          name: "Function<{ T : Any }(x: { T : Any }, y: { T : Any })>"
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "main"
      qualified_name {
        full_name: "reassigned_multiple_binds.main__i0__bind_1"
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
                kind: EXPR_LITERAL
                literal {
                  int_value: 2
                }
              }
            }
            argument {
              name: "y"
              value {
                kind: EXPR_LITERAL
                literal {
                  int_value: 3
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
    binding {
      scope_name {
        name: "reassigned_multiple_binds::main::main__i0__bind_2"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "f"
        type_spec {
          name: "Function<Int(x: Any, y: Any)>"
        }
      }
      result_type {
        name: "Int"
      }
      function_name: "main"
      qualified_name {
        full_name: "reassigned_multiple_binds.main__i0__bind_2"
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
                kind: EXPR_LITERAL
                literal {
                  int_value: 2
                }
              }
            }
            argument {
              name: "y"
              value {
                kind: EXPR_LITERAL
                literal {
                  int_value: 3
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
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "reassigned_multiple_binds::main2::main2__i0"
    }
    kind: OBJ_FUNCTION
    parameter {
      name: "f"
      type_spec {
        name: "Any"
      }
    }
    result_type {
      name: "Any"
    }
    function_name: "main2"
    qualified_name {
      full_name: "reassigned_multiple_binds.main2__i0"
    }
    binding {
      scope_name {
        name: "reassigned_multiple_binds::main2::main2__i0__bind_1"
      }
      kind: OBJ_FUNCTION
      parameter {
        name: "f"
        type_spec {
          name: "Function<{ T : Any }(x: { T : Any }, y: { T : Any })>"
        }
      }
      result_type {
        name: "Float64"
      }
      function_name: "main2"
      qualified_name {
        full_name: "reassigned_multiple_binds.main2__i0__bind_1"
      }
      body {
        kind: EXPR_BLOCK
        child {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Float64"
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
                  double_value: 2
                }
              }
            }
            argument {
              name: "y"
              value {
                kind: EXPR_LITERAL
                literal {
                  double_value: 3
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
          full_name: "main"
        }
      }
      argument {
        name: "f"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "ff"
          }
        }
      }
      binding_type {
        name: "Function<Int(f: Function<{ T : Any }(x: { T : Any }, y: { T : Any })>)>"
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
          full_name: "main"
        }
      }
      argument {
        name: "f"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "gg"
          }
        }
      }
      binding_type {
        name: "Function<Int(f: Function<Int(x: Any, y: Any)>)>"
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
          full_name: "main2"
        }
      }
      argument {
        name: "f"
        value {
          kind: EXPR_IDENTIFIER
          identifier {
            full_name: "ff"
          }
        }
      }
      binding_type {
        name: "Function<Float64(f: Function<{ T : Any }(x: { T : Any }, y: { T : Any })>)>"
      }
    }
  }
}
