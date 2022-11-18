# nudl

NUna Data Language

This repo is under development - more to follow.

## Code formatting:

* Follow Google C++ style guide:
https://google.github.io/styleguide/cppguide.html
* Format bazel file using buildifier: `buildifier -r .`
https://formulae.brew.sh/formula/buildifier
* For new code add `cpplint()` targets, for autimatic linting
as a test step (follow examples)
* Install `clang-format` and format code with wither
`git clang-format` for the entire repo, or
`clang-format -style=Google -i <file>` for specific file.
https://formulae.brew.sh/formula/clang-format
  - Note: use `// clang-format off` and `// clang-format on`
  to disable `clang-format` in a section of a file.

## Code coverage:
This runs ok on linux (ie. start a docker container or such),
was not able to get meaningful results on oxs.

 * Install tool: `brew install lcov` / `apt-get install lcov`
 * Run coverage: `bazel coverage --combined_report=lcov -- //... -//nudl/conversion/pylib/...`
 * Build the report `genhtml --output genhtml "$(bazel info output_path)/_coverage/_coverage_report.dat"`
 * Open `genhtml/index.html` for the report

Coverage report: https://nunainc.github.io/nudl/ / https://nunahealth.pages.nuna.cloud/nudl


## To Run the Demo:

```
# Build the converter:
bazel build //nudl/conversion:convert

# Run analyze (-y takes a little longer, as beautifies w/ yapf)
./analyze.sh -m examples.assign_example -l python -o /tmp/demo -y

# Note: need to install in your python env all requirements.txt

# Run the test:
cd /tmp/demo && python -m examples.assign_example

# Lint it ?
cd /tmp/demo && pyright
cd /tmp/demo && mypy
```
