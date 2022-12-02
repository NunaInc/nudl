load("@bazel_skylib//lib:paths.bzl", "paths")
load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")
load("@nuna_nudl//bazel/pyright:pyright.bzl", "pyright_test")

def _nudl_py_target(
        target_type,
        name,
        srcs,
        main,
        deps,
        builtins,
        imports = [],
        visibility = [],
        py_deps = []):
    native.filegroup(
        name = name + "_ndl_src",
        srcs = srcs,
    )
    if len(srcs) == 1 and main == None:
        main = srcs[0].removesuffix(".ndl") + ".py"
    elif target_type == "binary" or target_type == "test":
        fail("Please specify a `main` when passing multiple sources")
    outs = [
        src.removesuffix(".ndl") + ".py"
        for src in srcs
    ]
    input_paths = ["$(locations " + src + ")" for src in srcs]
    import_paths = ["$(locations " + imp + ")" for imp in imports]
    search_paths = ["$(locations " + dep + "_ndl_src)" for dep in deps] + input_paths
    nudl_deps = []
    nudl_deps.extend([dep + "_ndl_src" for dep in deps])
    nudl_deps.append(builtins)
    if target_type == "binary":
        main = main.removesuffix(".py") + "_main.py"
        outs.append(main)

    native.genrule(
        name = name + "_py_src",
        srcs = srcs + nudl_deps,
        outs = outs,
        cmd = "$(execpath @nuna_nudl//nudl/conversion:convert) " +
              " --builtin_path=$(locations " + builtins + ")" +
              " \"--search_paths=.," + ",".join(search_paths) + "\"" +
              " \"--input_paths=" + ",".join(input_paths) + "\"" +
              " \"--imports=" + ",".join(imports) + "\"" +
              " --output_dir=$(RULEDIR) " +
              " --direct_output --lang=python --bindings_on_use --write_only_input",
        tools = ["@nuna_nudl//nudl/conversion:convert"],
    )
    full_py_deps = deps + py_deps + [
        "@nuna_nudl//nudl/conversion/pylib:nudl",
    ]
    if target_type == "library":
        py_library(
            name = name,
            srcs = outs,
            deps = full_py_deps,
            imports = imports,
            visibility = visibility,
        )
    elif target_type == "binary":
        py_binary(
            name = name,
            main = main,
            srcs = outs,
            deps = full_py_deps,
            imports = imports,
            visibility = visibility,
        )
    elif target_type == "test":
        py_test(
            name = name,
            main = main,
            srcs = outs,
            deps = full_py_deps,
            imports = imports,
            visibility = visibility,
        )
    else:
        fail("Invalid target type: " + target_type)
    pyright_test(
        name = "%s_pyright_test" % name,
        srcs = [":" + name],
        deps = full_py_deps,
        imports = imports,
        extra_pyright_args = """{
           "pythonVersion": "3.9"
        }""",
    )

def nudl_py_library(
        name,
        srcs,
        deps = [],
        imports = [],
        visibility = [],
        builtins = "@nuna_nudl//nudl/conversion/pylib:nudl_builtins_ndl",
        py_deps = []):
    _nudl_py_target(
        "library",
        name,
        srcs,
        None,
        deps,
        builtins,
        imports,
        visibility,
        py_deps,
    )

def nudl_py_binary(
        name,
        srcs,
        main = None,
        deps = [],
        imports = [],
        visibility = [],
        builtins = "@nuna_nudl//nudl/conversion/pylib:nudl_builtins_ndl",
        py_deps = []):
    _nudl_py_target(
        "binary",
        name,
        srcs,
        main,
        deps,
        builtins,
        imports,
        visibility,
        py_deps,
    )

def nudl_py_test(
        name,
        srcs,
        main = None,
        deps = [],
        imports = [],
        visibility = [],
        builtins = "@nuna_nudl//nudl/conversion/pylib:nudl_builtins_ndl",
        py_deps = []):
    _nudl_py_target(
        "test",
        name,
        srcs,
        main,
        deps,
        builtins,
        imports,
        visibility,
        py_deps,
    )
