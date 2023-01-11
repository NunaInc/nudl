#
# Copyright 2022 Nuna inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import os
import typing
import _convert_nudl

_NEXT_MODULE_ID = 0


def _NextModuleName():
    global _NEXT_MODULE_ID
    _NEXT_MODULE_ID += 1
    return f"convert_{_NEXT_MODULE_ID}"


_BASE_DIR = os.path.dirname(_convert_nudl.__file__)


def DefaultBuiltinPath():
    return os.path.join(_BASE_DIR, "pylib", "nudl_builtins.ndl")


def DefaultSearchPath():
    return os.path.join(_BASE_DIR, "pylib")


def ConvertWithDefaults(
    code: str,
    extra_search_paths: typing.Optional[typing.List[str]] = None
) -> typing.Tuple[str, typing.List[str]]:
    search_paths = [DefaultSearchPath()]
    if extra_search_paths:
        search_paths.extend(extra_search_paths)
    return _convert_nudl.Convert(_NextModuleName(), code, DefaultBuiltinPath(),
                                 search_paths)
