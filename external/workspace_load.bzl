load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//external/cpplint:workspace.bzl", cpplint = "repo")

def nuna_nudl_load_workspace():
    # Bazel utility library
    http_archive(
        name = "bazel_skylib",
        sha256 = "97e70364e9249702246c0e9444bccdc4b847bed1eb03c5a3ece4f83dfe6abc44",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.0.2/bazel-skylib-1.0.2.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.0.2/bazel-skylib-1.0.2.tar.gz",
        ],
    )

    ## CC rules
    http_archive(
        name = "rules_cc",
        sha256 = "9d48151ea71b3e225adfb6867e6d2c7d0dce46cbdc8710d9a9a628574dfd40a0",
        strip_prefix = "rules_cc-818289e5613731ae410efb54218a4077fb9dbb03",
        urls = ["https://github.com/bazelbuild/rules_cc/archive/818289e5613731ae410efb54218a4077fb9dbb03.tar.gz"],
    )

    ## Proto rules
    http_archive(
        name = "rules_proto",
        sha256 = "a4382f78723af788f0bc19fd4c8411f44ffe0a72723670a34692ffad56ada3ac",
        strip_prefix = "rules_proto-f7a30f6f80006b591fa7c437fe5a951eb10bcbcf",
        urls = ["https://github.com/bazelbuild/rules_proto/archive/f7a30f6f80006b591fa7c437fe5a951eb10bcbcf.zip"],
    )

    ## Google protobuf
    http_archive(
        name = "com_google_protobuf",
        sha256 = "f6251f2d00aad41b34c1dfa3d752713cb1bb1b7020108168a4deaa206ba8ed42",
        strip_prefix = "protobuf-3.21.8",
        urls = ["https://github.com/protocolbuffers/protobuf/releases/download/v21.8/protobuf-cpp-3.21.8.tar.gz"],
    )

    ## Antlr
    native.new_local_repository(
        name = "rules_antlr",
        build_file = "external/rules_antlr/BUILD",
        path = "external/rules_antlr",
        workspace_file = "external/rules_antlr/WORKSPACE",
    )

    ## Google Abseil library
    http_archive(
        name = "com_google_absl",
        sha256 = "4208129b49006089ba1d6710845a45e31c59b0ab6bff9e5788a87f55c5abd602",
        strip_prefix = "abseil-cpp-20220623.0",
        urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.0.tar.gz"],
    )

    ## Google test & benchmark
    http_archive(
        name = "com_google_googletest",
        sha256 = "81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2",
        strip_prefix = "googletest-release-1.12.1",
        urls = ["https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz"],
    )

    http_archive(
        name = "com_github_google_benchmark",
        sha256 = "3aff99169fa8bdee356eaa1f691e835a6e57b1efeadb8a0f9f228531158246ac",
        strip_prefix = "benchmark-1.7.0",
        urls = ["https://github.com/google/benchmark/archive/refs/tags/v1.7.0.tar.gz"],
    )

    ## Google gflags
    http_archive(
        name = "com_github_gflags_gflags",
        sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
        strip_prefix = "gflags-2.2.2",
        urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
    )

    ## Google glog
    http_archive(
        name = "com_github_google_glog",
        sha256 = "122fb6b712808ef43fbf80f75c52a21c9760683dae470154f02bddfc61135022",
        strip_prefix = "glog-0.6.0",
        urls = ["https://github.com/google/glog/archive/v0.6.0.zip"],
    )

    ## Google RE2
    http_archive(
        name = "com_googlesource_code_re2",
        sha256 = "f89c61410a072e5cbcf8c27e3a778da7d6fd2f2b5b1445cd4f4508bee946ab0f",
        strip_prefix = "re2-2022-06-01",
        urls = ["https://github.com/google/re2/archive/refs/tags/2022-06-01.tar.gz"],
    )

    # Bazel platform rules.
    http_archive(
        name = "platforms",
        sha256 = "a879ea428c6d56ab0ec18224f976515948822451473a80d06c2e50af0bbe5121",
        strip_prefix = "platforms-da5541f26b7de1dc8e04c075c99df5351742a4a2",
        urls = ["https://github.com/bazelbuild/platforms/archive/da5541f26b7de1dc8e04c075c99df5351742a4a2.zip"],  # 2022-05-27
    )

    cpplint()
