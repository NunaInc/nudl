//
// Copyright 2022 Nuna inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "nudl/conversion/python_converter.h"

ABSL_FLAG(bool, pyconvert_print_name_changes, false,
          "If we should output the name changes done for python conversion");

namespace nudl {
namespace conversion {

namespace {
std::string PythonSafeNameUnit(
    absl::string_view name,
    const absl::optional<analysis::NamedObject*>& object) {
  const bool is_python_special = IsPythonSpecialName(name);
  const bool is_python_keyword = IsPythonKeyword(name);
  const bool is_nudl_name = absl::EndsWith(name, kPythonRenameEnding);
  bool is_python_builtin = IsPythonBuiltin(name);
  if (object.has_value() &&
      object.value()->kind() == pb::ObjectKind::OBJ_FIELD) {
    is_python_builtin = false;
  }
  if (!is_python_special && !is_python_special && !is_nudl_name &&
      !is_python_builtin) {
    return std::string(name);
  }
  const std::string new_name(absl::StrCat(name, kPythonRenameEnding));
  if (absl::GetFlag(FLAGS_pyconvert_print_name_changes)) {
    std::cerr << "Renaming: `" << name << "` as `" << new_name << "`"
              << (is_python_special ? " Is Python special function name;" : "")
              << (is_python_keyword ? " Is Python keyword;" : "")
              << (is_nudl_name ? " Is a nudl python-safe name;" : "")
              << (is_python_builtin ? " Is Python builtin / standard name;"
                                    : "")
              << std::endl;
  }
  return new_name;
}
}  // namespace

// TODO(catalin): May want to tweak this one if creates too much damage:
std::string PythonSafeName(absl::string_view name,
                           absl::optional<analysis::NamedObject*> object) {
  if (object.has_value() &&
      analysis::Function::IsFunctionKind(*object.value()) &&
      (static_cast<analysis::Function*>(object.value())
           ->is_skip_conversion())) {
    return std::string(name);
  }
  std::vector<std::string> comp = absl::StrSplit(name, ".");
  std::reverse(comp.begin(), comp.end());
  std::vector<std::string> result;
  result.reserve(comp.size());
  absl::optional<analysis::NamedObject*> crt_object = object;
  for (const auto& s : comp) {
    result.emplace_back(PythonSafeNameUnit(s, crt_object));
    do {
      if (crt_object.has_value()) {
        crt_object = crt_object.value()->parent_store();
      }
    } while (crt_object.has_value() &&
             analysis::FunctionGroup::IsFunctionGroup(*crt_object.value()));
  }
  std::reverse(result.begin(), result.end());
  return absl::StrJoin(result, ".");
}

bool IsPythonKeyword(absl::string_view name) {
  static const auto kPythonKeywords = new absl::flat_hash_set<std::string>({
      // Keywords:
      "await",    "else",  "import", "pass",    "None",   "break",  "except",
      "in",       "raise", "class",  "finally", "is",     "return", "and",
      "continue", "for",   "lambda", "try",     "as",     "def",    "from",
      "nonlocal", "while", "assert", "del",     "global", "not",    "with",
      "async",    "elif",  "if",     "or",      "yield",  "False",  "True",
  });
  return kPythonKeywords->contains(name);
}

bool IsPythonBuiltin(absl::string_view name) {
  static const auto kPythonNames = new absl::flat_hash_set<std::string>({
      // Builtins
      "__name__",
      "__doc__",
      "__package__",
      "__loader__",
      "__spec__",
      "__build_class__",
      "__import__",
      "abs",
      "all",
      "any",
      "ascii",
      "bin",
      "breakpoint",
      "callable",
      "chr",
      "compile",
      "delattr",
      "dir",
      "divmod",
      "eval",
      "exec",
      "format",
      "getattr",
      "globals",
      "hasattr",
      "hash",
      "hex",
      "id",
      "input",
      "isinstance",
      "issubclass",
      "iter",
      "len",
      "locals",
      "max",
      "min",
      "next",
      "oct",
      "ord",
      "pow",
      "print",
      "repr",
      "round",
      "setattr",
      "sorted",
      "sum",
      "vars",
      "None",
      "Ellipsis",
      "NotImplemented",
      "bool",
      "memoryview",
      "bytearray",
      "bytes",
      "classmethod",
      "complex",
      "dict",
      "enumerate",
      "filter",
      "float",
      "frozenset",
      "property",
      "int",
      "list",
      "map",
      "object",
      "range",
      "reversed",
      "set",
      "slice",
      "staticmethod",
      "str",
      "super",
      "tuple",
      "type",
      "zip",
      "__debug__",
      "BaseException",
      "Exception",
      "TypeError",
      "StopAsyncIteration",
      "StopIteration",
      "GeneratorExit",
      "SystemExit",
      "KeyboardInterrupt",
      "ImportError",
      "ModuleNotFoundError",
      "OSError",
      "EnvironmentError",
      "IOError",
      "EOFError",
      "RuntimeError",
      "RecursionError",
      "NotImplementedError",
      "NameError",
      "UnboundLocalError",
      "AttributeError",
      "SyntaxError",
      "IndentationError",
      "TabError",
      "LookupError",
      "IndexError",
      "KeyError",
      "ValueError",
      "UnicodeError",
      "UnicodeEncodeError",
      "UnicodeDecodeError",
      "UnicodeTranslateError",
      "AssertionError",
      "ArithmeticError",
      "FloatingPointError",
      "OverflowError",
      "ZeroDivisionError",
      "SystemError",
      "ReferenceError",
      "MemoryError",
      "BufferError",
      "Warning",
      "UserWarning",
      "DeprecationWarning",
      "PendingDeprecationWarning",
      "SyntaxWarning",
      "RuntimeWarning",
      "FutureWarning",
      "ImportWarning",
      "UnicodeWarning",
      "BytesWarning",
      "ResourceWarning",
      "ConnectionError",
      "BlockingIOError",
      "BrokenPipeError",
      "ChildProcessError",
      "ConnectionAbortedError",
      "ConnectionRefusedError",
      "ConnectionResetError",
      "FileExistsError",
      "FileNotFoundError",
      "IsADirectoryError",
      "NotADirectoryError",
      "InterruptedError",
      "PermissionError",
      "ProcessLookupError",
      "TimeoutError",
      "open",
      "quit",
      "exit",
      "copyright",
      "credits",
      "license",
      "help",
      "_",
      // General module names:
      "__future__",
      "__main__",
      "_thread",
      "abc",
      "aifc",
      "argparse",
      "array",
      "ast",
      "asynchat",
      "asyncio",
      "asyncore",
      "atexit",
      "audioop",
      "base64",
      "bdb",
      "binascii",
      "bisect",
      "builtins",
      "bz2",
      "calendar",
      "cgi",
      "cgitb",
      "chunk",
      "cmath",
      "cmd",
      "code",
      "codecs",
      "codeop",
      "collections",
      "colorsys",
      "compileall",
      "concurrent",
      "contextlib",
      "contextvars",
      "copy",
      "copyreg",
      "cProfile",
      "csv",
      "ctypes",
      "curses",
      "dataclasses",
      "datetime",
      "dbm",
      "decimal",
      "difflib",
      "dis",
      "distutils",
      "doctest",
      "email",
      "encodings",
      "ensurepip",
      "enum",
      "errno",
      "faulthandler",
      "fcntl",
      "filecmp",
      "fileinput",
      "fnmatch",
      "fractions",
      "ftplib",
      "functools",
      "gc",
      "getopt",
      "getpass",
      "gettext",
      "glob",
      "graphlib",
      "grp",
      "gzip",
      "hashlib",
      "heapq",
      "hmac",
      "html",
      "http",
      "idlelib",
      "imaplib",
      "imghdr",
      "imp",
      "importlib",
      "inspect",
      "io",
      "ipaddress",
      "itertools",
      "json",
      "keyword",
      "lib2to3",
      "linecache",
      "locale",
      "logging",
      "lzma",
      "mailbox",
      "mailcap",
      "marshal",
      "math",
      "mimetypes",
      "mmap",
      "modulefinder",
      "msilib",
      "msvcrt",
      "multiprocessing",
      "netrc",
      "nis",
      "nntplib",
      "numbers",
      "operator",
      "optparse",
      "os",
      "ossaudiodev",
      "pathlib",
      "pdb",
      "pickle",
      "pickletools",
      "pipes",
      "pkgutil",
      "platform",
      "plistlib",
      "poplib",
      "posix",
      "pprint",
      "profile",
      "pstats",
      "pty",
      "pwd",
      "py_compile",
      "pyclbr",
      "pydoc",
      "queue",
      "quopri",
      "random",
      "re",
      "readline",
      "reprlib",
      "resource",
      "rlcompleter",
      "runpy",
      "sched",
      "secrets",
      "select",
      "selectors",
      "shelve",
      "shlex",
      "shutil",
      "signal",
      "site",
      "smtpd",
      "smtplib",
      "sndhdr",
      "socket",
      "socketserver",
      "spwd",
      "sqlite3",
      "ssl",
      "stat",
      "statistics",
      "string",
      "stringprep",
      "struct",
      "subprocess",
      "sunau",
      "symtable",
      "sys",
      "sysconfig",
      "syslog",
      "tabnanny",
      "tarfile",
      "telnetlib",
      "tempfile",
      "termios",
      "test",
      "textwrap",
      "threading",
      "time",
      "timeit",
      "tkinter",
      "token",
      "tokenize",
      "tomllib",
      "trace",
      "traceback",
      "tracemalloc",
      "tty",
      "turtle",
      "turtledemo",
      "types",
      "typing",
      "unicodedata",
      "unittest",
      "urllib",
      "uu",
      "uuid",
      "venv",
      "warnings",
      "wave",
      "weakref",
      "webbrowser",
      "winreg",
      "winsound",
      "wsgiref",
      "xdrlib",
      "xml",
      "xmlrpc",
      "zipapp",
      "zipfile",
      "zipimport",
      "zlib",
      "zoneinfo",
  });
  return kPythonNames->contains(name);
}

bool IsPythonSpecialName(absl::string_view name) {
  // Cut out all the __init__ and related python name.
  return absl::StartsWith(name, "__") && absl::EndsWith(name, "__");
}

}  // namespace conversion
}  // namespace nudl
