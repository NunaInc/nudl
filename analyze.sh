#!/usr/bin/env bash

help() {
     cat << __EOF__

Analyzes Nudl code

${0} \
-m <module_name> \
-l <python|pseudo> \
-pp <py_path> \
-sp <search_path(s)> \
-bp <builtin_module_path> \
-o <output_dir> \
-y \
-io

 -m - sets the module to convert
 -lang - the language to convert to
 -pp - path with python base nudl common modules
 -sp - search path(s) for nudl modules (comma sepparated)
 -bp - where the builtin.ndl resides
 -o - output all artifacts to this directory
 -y - run yapf on python generated sources
      (yapf binary must be in the path)
 -io - write only one file - the input only

__EOF__
     exit;
}

py_path="$(pwd)/nudl/conversion/pylib"
search_paths="${py_path}"
builtin_path="${search_paths}/builtins.ndl"
output_dir=
module_name=
run_yapf=
write_only_input=
lang=python

while [[ $# -gt 0 ]]; do
      case ${1} in
          -o|--output)
              output_dir="${2}"
              shift; shift
              ;;
          -m|--module)
              module_name="${2}"
              shift; shift
              ;;
          -sp|--search_path)
              search_paths="${2}"
              shift; shift
              ;;
          -pp|--py_path)
              py_path="${2}"
              shift; shift
              ;;
          -bp|--builtin_path)
              builtin_path="${2}"
              shift; shift
              ;;
          -l|--lang)
              lang="${2}"
              shift; shift
              ;;
          -y|--yapf)
              run_yapf="$(which yapf)" || echo "Cannot find yapf - skipping"
              shift
              ;;
          -io)
              write_only_input=--write_only_input
              shift
              ;;
          --)
              shift
              break
              ;;
          *)
              echo "Unknown option ${1}"
              help
              ;;
      esac
done

./bazel-bin/nudl/conversion/convert \
    "--builtin_path=${builtin_path}" \
    "--search_paths=${search_paths}" \
    "--lang=${lang}" \
    "--input=${module_name}" \
    "--py_path=${py_path}" \
    "--output_dir=${output_dir}" \
    "--run_yapf=${run_yapf}" \
    ${write_only_input} $@ \
