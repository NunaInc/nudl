import absl.flags
import typing


def define_int_flag(name: str, value: int, description: str) -> str:
    absl.flags.DEFINE_int(name, value, description)
    return name


def define_str_flag(name: str, value: str, description: str) -> str:
    absl.flags.DEFINE_string(name, value, description)
    return name


def define_bool_flag(name: str, value: bool, description: str) -> str:
    absl.flags.DEFINE_boolean(name, value, description)
    return name


def define_list_flag(name: str, value: typing.List[str],
                     description: str) -> str:
    absl.flags.DEFINE_list(name, value, description)
    return name
