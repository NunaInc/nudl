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
-y -io -x -p -afun

 -m - sets the module to convert
 -lang - the language to convert to
 -pp - path with python base nudl common modules
 -sp - search path(s) for nudl modules (comma sepparated)
 -bp - where the builtin.ndl resides
 -o - output all artifacts to this directory
 -y - run yapf on python generated sources
      (yapf binary must be in the path)
 -io - write only one file - the input only
 -p - runs pyright on the generated module (if -o specified)
 -x - executes the generated module (if -o specified)
 -afun - accept "abstract" function variables
 -bu - convert bindings on use modules only

__EOF__
     exit;
}

py_path="$(pwd)/nudl/conversion/pylib"
search_paths="${py_path}"
builtin_path="${search_paths}/nudl_builtins.ndl"
output_dir=
module_name=
run_yapf=
write_only_input=
abstracts=
lang=python
run_pyright=
run_module=
bindings_on_use=

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
          -p)
              run_pyright="true"
              shift
              ;;
          -x)
              run_module="true"
              shift
              ;;
          -io)
              write_only_input=--write_only_input
              shift
              ;;
          -afun)
              abstracts=--nudl_accept_abstract_function_objects
              shift
              ;;
          -bu)
              bindings_on_use=--bindings_on_use
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
    ${abstracts} ${write_only_input} ${bindings_on_use} $@ || exit 1

if [ ${output_dir} ]; then
    pushd ${output_dir}
    if [ ${run_pyright} ]; then
        echo ">>>>>>>>>>>>>>>>>>>> "\
             "Running pyright under ${output_dir} ..."
        pyright || exit 1
        echo "<<<<<<<<<<<<<<<<<<<< Done"
    fi
    if [ ${run_module} ]; then
        echo ">>>>>>>>>>>>>>>>>>>> "\
             "Running ${module_name} under ${output_dir} ..."
        python3.9 -m "${module_name}" || exit 1
        echo "<<<<<<<<<<<<<<<<<<<< Done"
    fi
    popd
fi
