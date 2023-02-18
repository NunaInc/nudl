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
    "@nuna_nudl//external:workspace_load.bzl",
    "nuna_nudl_load_workspace",
)

nuna_nudl_load_workspace()

load(
    "@nuna_nudl//external:workspace_setup.bzl",
    "nuna_nudl_setup_workspace",
)

nuna_nudl_setup_workspace()

################################################################################

## Python PIP dependencies:
load("@python3_9//:defs.bzl", "interpreter")
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "pip_deps",
    python_interpreter_target = interpreter,
    requirements_lock = "@nuna_nudl//:requirements_lock.txt",
)

load("@pip_deps//:requirements.bzl", "install_deps")

install_deps()

pip_parse(
    name = "pip_interactive_deps",
    python_interpreter_target = interpreter,
    requirements_lock = "@nuna_nudl//:requirements_interactive_lock.txt",
)

load(
    "@pip_interactive_deps//:requirements.bzl",
    install_interactive_deps = "install_deps",
)

install_interactive_deps()

pip_parse(
    name = "pip_beam_deps",
    python_interpreter_target = interpreter,
    requirements_lock = "@nuna_nudl//:requirements_beam_lock.txt",
)

load(
    "@pip_beam_deps//:requirements.bzl",
    install_beam_deps = "install_deps",
)

install_beam_deps()

################################################################################

## JS rules dependencies
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "aspect_rules_js",
    sha256 = "c4a5766a45dff25b2eb1789d7a2decfda81b281fc88350d24687620c37fefb25",
    strip_prefix = "rules_js-1.14.0",
    url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.14.0.tar.gz",
)

load("@aspect_rules_js//js:repositories.bzl", "rules_js_dependencies")

rules_js_dependencies()

load("@rules_nodejs//nodejs:repositories.bzl", "DEFAULT_NODE_VERSION", "nodejs_register_toolchains")

nodejs_register_toolchains(
    name = "nodejs",
    node_version = DEFAULT_NODE_VERSION,
)

load("@aspect_rules_js//npm:npm_import.bzl", "npm_translate_lock")

npm_translate_lock(
    name = "npm",
    data = [
        "//:pnpm-workspace.yaml",
        "//bazel/pyright:package.json",
    ],
    pnpm_lock = "//:pnpm-lock.yaml",
    update_pnpm_lock = True,
    verify_node_modules_ignored = "//:.bazelignore",
)

load("@npm//:repositories.bzl", "npm_repositories")

npm_repositories()
