#
# Copyright 2022 Nuna inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
workspace(name = "nuna_nudl")

load(
    "@nuna_nudl//:external/workspace_load.bzl",
    "nuna_nudl_load_workspace",
)

nuna_nudl_load_workspace()

load(
    "@nuna_nudl//:external/workspace_setup.bzl",
    "nuna_nudl_setup_workspace",
)

nuna_nudl_setup_workspace()

load("@python3_9//:defs.bzl", "interpreter")
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "pip_deps",
    python_interpreter_target = interpreter,
    requirements_lock = "//:requirements_lock.txt",
)

load("@pip_deps//:requirements.bzl", "install_deps")

install_deps()
