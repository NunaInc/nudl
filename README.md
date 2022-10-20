# nudl

NUna Data Language

This repo is under development.

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
   to disable `clang-format` in a section of a file
