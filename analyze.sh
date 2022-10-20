#!/usr/bin/env bash

module=$1
dir=$2

if [ -z "$dir" ]; then
  dir=$(pwd)/nudl/analysis/testing/testdata
fi

./bazel-bin/nudl/analysis/convert \
    --builtin_path=${dir}/builtin.ndl \
    --search_paths=${dir}/ \
    --input=${module}
