name: "composed_constructs"
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Array<Timestamp>"
    }
    call_spec {
      call_name {
        full_name: "array__bind_1"
      }
      is_method: true
      argument {
        name: "t"
        value {
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
      binding_type {
        name: "Function<Array<Timestamp>(t: Timestamp)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Array<Date>"
    }
    call_spec {
      call_name {
        full_name: "array__bind_2"
      }
      is_method: true
      argument {
        name: "t"
        value {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Date"
          }
          call_spec {
            call_name {
              full_name: "date"
            }
            is_method: true
            binding_type {
              name: "Function<Date()>"
            }
          }
        }
      }
      binding_type {
        name: "Function<Array<Date>(t: Date)>"
      }
    }
  }
}
expression {
  kind: EXPR_ASSIGNMENT
  child {
    kind: EXPR_FUNCTION_CALL
    type_spec {
      name: "Array<Array<Date>>"
    }
    call_spec {
      call_name {
        full_name: "array__bind_3"
      }
      is_method: true
      argument {
        name: "t"
        value {
          kind: EXPR_FUNCTION_CALL
          type_spec {
            name: "Array<Date>"
          }
          call_spec {
            call_name {
              full_name: "array__bind_2__bind_1"
            }
            is_method: true
            argument {
              name: "t"
              value {
                kind: EXPR_FUNCTION_CALL
                type_spec {
                  name: "Date"
                }
                call_spec {
                  call_name {
                    full_name: "date"
                  }
                  is_method: true
                  binding_type {
                    name: "Function<Date()>"
                  }
                }
              }
            }
            binding_type {
              name: "Function<Array<Date>(t: Date)>"
            }
          }
        }
      }
      binding_type {
        name: "Function<Array<Array<Date>>(t: Array<Date>)>"
      }
    }
  }
}
