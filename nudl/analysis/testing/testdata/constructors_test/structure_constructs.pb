name: "structure_constructs"
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Timestamp"
    }
    call_spec {
      call_name {
        full_name: "timestamp"
      }
      is_method: true
      binding_type {
        name: "Function<Timestamp()>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Date"
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
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Nullable<Date>"
          }
          call_spec {
            call_name {
              full_name: "date__1__"
            }
            is_method: true
            argument {
              name: "year"
              value {
                kind: EXPR_LITERAL
                literal {
                  int_value: 2022
                }
              }
            }
            argument {
              name: "month"
              value {
                kind: EXPR_LITERAL
                literal {
                  int_value: 10
                }
              }
            }
            argument {
              name: "day"
              value {
                kind: EXPR_LITERAL
                literal {
                  int_value: 3
                }
              }
            }
            binding_type {
              name: "Function<Nullable<Date>(year: Int, month: Int, day: Int)>"
            }
          }
        }
      }
      binding_type {
        name: "Function<Date(x: Nullable<Date>)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Date"
    }
    call_spec {
      call_name {
        full_name: "date__2__"
      }
      is_method: true
      argument {
        name: "ts"
        value {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Timestamp"
          }
          call_spec {
            left_expression {
              kind: EXPR_IDENTIFIER
              identifier {
                full_name: "timestamp_sec"
              }
            }
            argument {
              name: "utc_timestamp_sec"
              value {
                kind: EXPR_LITERAL
                literal {
                  int_value: 3300303
                }
              }
            }
            binding_type {
              name: "Function<Timestamp(utc_timestamp_sec: Float64)>"
            }
          }
        }
      }
      binding_type {
        name: "Function<Date(ts: Timestamp)>"
      }
    }
  }
}
expression {
  kind: EXPR_FUNCTION_DEF
  function_spec {
    scope_name {
      name: "structure_constructs::year::year"
    }
    kind: OBJ_METHOD
    parameter {
      name: "d"
      type_spec {
        name: "Date"
      }
    }
    result_type {
      name: "Int"
    }
    function_name: "year"
    qualified_name {
      full_name: "structure_constructs.year"
    }
    body {
      kind: EXPR_BLOCK
      child {
        kind: EXPR_LITERAL
        literal {
          int_value: 1
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
      call_name {
        full_name: "__sub__"
      }
      argument {
        name: "x"
        value {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Int"
          }
          call_spec {
            call_name {
              full_name: "structure_constructs.year"
            }
            is_method: true
            argument {
              name: "d"
              value {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "date1"
                }
              }
            }
            binding_type {
              name: "Function<Int(d: Date)>"
            }
          }
        }
      }
      argument {
        name: "y"
        value {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Int"
          }
          call_spec {
            call_name {
              full_name: "structure_constructs.year"
            }
            is_method: true
            argument {
              name: "d"
              value {
                kind: EXPR_IDENTIFIER
                identifier {
                  full_name: "date2"
                }
              }
            }
            binding_type {
              name: "Function<Int(d: Date)>"
            }
          }
        }
      }
      binding_type {
        name: "Function<Int(x: Int, y: Int)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Nullable<Date>"
    }
    call_spec {
      left_expression {
        kind: EXPR_IDENTIFIER
        identifier {
          full_name: "date"
        }
      }
      argument {
        name: "year"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 2022
          }
        }
      }
      argument {
        name: "month"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 10
          }
        }
      }
      argument {
        name: "day"
        value {
          kind: EXPR_LITERAL
          literal {
            int_value: 3
          }
        }
      }
      binding_type {
        name: "Function<Nullable<Date>(year: Int, month: Int, day: Int)>"
      }
    }
  }
}
