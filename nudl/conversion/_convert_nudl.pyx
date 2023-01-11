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

from libcpp.string cimport string
from libcpp.vector cimport vector

import typing

cdef extern from "nudl/conversion/convert_tool.h" namespace 'nudl':
    string ConvertPythonSource(
        const string& module_name,
        const string& code,
        const string& builtin_path,
        const vector[string]& search_paths,
        vector[string]* errors);


def Convert(module_name: str,
            code: str,
            builtin_path: str,
            search_paths: typing.List[str]
            ) -> typing.Tuple[str, typing.List[str]]:
    cdef vector[string] errors
    result = ConvertPythonSource(module_name.encode('utf-8'),
                                 code.encode('utf-8'),
                                 builtin_path.encode('utf-8'),
                                 [path.encode('utf-8') for path in search_paths],
                                 &errors)
    return (result.decode('utf-8'),
            [error.decode('utf-8') for error in errors])
