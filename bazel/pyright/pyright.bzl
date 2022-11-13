"""
This is an implementation of using the pyright typechecker
with bazel - for now we run as test
TODO(kush) - this can be optimized - for now it works
"""

load("@bazel_skylib//lib:shell.bzl", "shell")

def _get_extra_paths_deps(deps):
    transitive_file_paths = []
    for dep in deps:
        if PyInfo in dep:
            transitive_file_paths.extend(
                dep[PyInfo].transitive_sources.to_list(),
            )
    extra_paths_dict = {}
    for file in transitive_file_paths:
        workspace_root = file.owner.workspace_root
        if workspace_root.startswith("external/"):
            workspace_root = "../" + workspace_root[len("external/"):] + "/site-packages"
        extra = workspace_root
        extra_paths_dict[extra] = None
    return extra_paths_dict.keys(), transitive_file_paths

def _pyright_test_impl(ctx):
    extra_paths = []
    dpset_files = []
    if hasattr(ctx.attr, "deps"):
        paths, dpset_files = _get_extra_paths_deps(ctx.attr.deps)
        extra_paths.extend(paths)

    pyright_args_dict = {}
    if hasattr(ctx.attr, "extra_pyright_args"):
        decoded = json.decode(ctx.attr.extra_pyright_args)
        pyright_args_dict.update(decoded)

    if hasattr(ctx.attr, "imports"):
        if ctx.attr.imports:
            extra_paths.extend(ctx.attr.imports)

    extra_paths_dict = {
        "extraPaths": extra_paths,
        # we create a fake venv so pyright will not search for
        # site-packages in the default python interpreter
        # we symlink this later to the workspace root
        "venv": "fake_venv",
        "venvPath": ".",
    }

    pyright_args_dict.update(extra_paths_dict)
    encoding = json.encode(pyright_args_dict)

    # we create a pyrightconfig.json file, and then symlink it to
    # the workspace root in the runfiles, so it is side by side
    # the pyright binary
    pyright_config = ctx.actions.declare_file(ctx.label.name + "pyrightconfig.json")

    ctx.actions.write(
        output = pyright_config,
        content = """{encoding}""".format(
            encoding = encoding,
        ),
    )

    exe = ctx.actions.declare_file(ctx.label.name)
    file_paths = [shell.quote(f.short_path) for f in ctx.files.srcs]
    ctx.actions.write(
        output = exe,
        content = """{command} {file_paths}""".format(
            command = ctx.executable._pyright_executor.short_path,
            file_paths = " ".join(file_paths),
        ),
        is_executable = True,
    )
    runfiles = ctx.runfiles(
        files = ctx.files.srcs + [pyright_config] + dpset_files,
        symlinks = {
            "pyrightconfig.json": pyright_config,
            "fake_venv/lib/site-packages/pajama": pyright_config,
        },
    )
    runfiles = runfiles.merge(
        ctx.attr._pyright_executor[DefaultInfo].default_runfiles,
    )
    return [
        DefaultInfo(
            runfiles = runfiles,
            executable = exe,
        ),
    ]

pyright_test = rule(
    implementation = _pyright_test_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "deps": attr.label_list(allow_files = True),
        "imports": attr.string_list(),
        "_pyright_executor": attr.label(
            executable = True,
            cfg = "exec",
            default = Label("@nuna_nudl//bazel/pyright:pyright_launcher"),
        ),
        "extra_pyright_args": attr.string(
            doc = "json string for pyrightconfig.json",
        ),
    },
    test = True,
)
