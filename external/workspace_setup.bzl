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
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")
load(
    "@rules_proto//proto:repositories.bzl",
    "rules_proto_dependencies",
    "rules_proto_toolchains",
)
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@rules_antlr//antlr:lang.bzl", "CPP", "PYTHON")
load("@rules_antlr//antlr:repositories.bzl", "rules_antlr_dependencies")
load("@build_bazel_rules_nodejs//:index.bzl", "npm_install")
load("@rules_python//python:repositories.bzl", "python_register_toolchains")
load("@nuna_nudl//external/py:python_configure.bzl", "python_configure")

def nuna_nudl_setup_workspace():
    rules_cc_dependencies()
    rules_proto_dependencies()
    rules_proto_toolchains()
    protobuf_deps()
    rules_antlr_dependencies("4.10.1", CPP, PYTHON)
    npm_install(
        name = "npm",
        package_json = "@nuna_nudl//bazel/pyright:package.json",
        package_lock_json = "@nuna_nudl//bazel/pyright:package-lock.json",
        symlink_node_modules = False,
    )
    python_register_toolchains(
        name = "python3_9",
        python_version = "3.9",
    )
    python_configure(name = "local_config_python")
    native.bind(
        name = "python_headers",
        actual = "@local_config_python//:python_headers",
    )
