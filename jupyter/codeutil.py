#
# Copyright 2022 Nuna inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""Utilities for code processing, strings and such."""

import ast
import astunparse
import dataclasses
import datetime
import decimal
import inspect
import re
import typing
import traceback

from io import StringIO
from types import FunctionType
from typing import Any, Dict, List, Optional, Set, Tuple
from yapf.yapflib.yapf_api import FormatCode


def is_valid_identifier(name: str) -> bool:
    """If provided name is a valid python identifier."""
    return re.fullmatch(r'[a-zA-Z_]\w*', name)


def is_valid_module_name(name: str) -> bool:
    """If provided name is a valid python module name."""
    return name and all(is_valid_identifier(comp) for comp in name.split('.'))


def is_valid_step_name(name: str) -> bool:
    """If provided name is a valid pipe step name: lowercase w/ underscores."""
    return re.fullmatch(r'[a-z][a-z0-9_]*', name)


def check_valid_identifier(name: str) -> bool:
    """Raises an exception if name is not a valid identifier."""
    if not is_valid_identifier(name):
        raise ValueError(f'Invalid identifier: `{name}`.')


def check_valid_step_name(name: str) -> bool:
    """Raises an exception if name is not a valid step name."""
    if not is_valid_step_name(name):
        raise ValueError(f'Invalid step name: `{name}`. '
                         'Expecting lower_case_with_underscore format.')


def capitalize_step_name(name: str) -> bool:
    """Capitalizes a pipe_step_name into a PipeStepName."""
    check_valid_step_name(name)
    capitalize = True
    with StringIO() as s:
        for c in name:
            if c == '_':
                capitalize = True
            elif capitalize:
                s.write(c.upper())
                capitalize = False
            else:
                s.write(c)
        return s.getvalue()


def uncapitalize_identifier(name: str) -> bool:
    """Capitalizes SomeIdentifierName into some_identifier_name."""
    check_valid_identifier(name)
    first = True
    with StringIO() as s:
        for c in name:
            if c.lower() != c:
                if not first:
                    s.write('_')
                first = False
            s.write(c.lower())
        return s.getvalue()


def is_simple_expression(node: ast.AST):
    """If provided ast node is obtained by parsing a simple expression."""
    return (isinstance(node, ast.Module) and len(node.body) == 1 and
            isinstance(node.body[0], ast.Expr))


def is_simple_return(node: ast.AST):
    """If provided ast node is obtained by parsing a simple return expression."""
    return (isinstance(node, ast.Module) and len(node.body) == 1 and
            isinstance(node.body[0], ast.Return))


def is_all_imports(node: ast.AST):
    """Checks if the provided ast code consists only of imports."""
    return (isinstance(node, ast.Module) and all(
        isinstance(stmt, (ast.ImportFrom, ast.Import)) for stmt in node.body))


def line_strip(line: str) -> str:
    """Removes spaces at the end of a line, but keeps the \n."""
    if line.endswith('\n'):
        return line.rstrip() + '\n'
    return line.rstrip()


def code_strip(snippet: str) -> str:
    """Removes the spaces at the end of a code snippet."""
    return ''.join(
        line_strip(line) for line in snippet.splitlines(keepends=True))


def type_repr(obj: type) -> str:
    """Returns the string representation of a type."""
    # Note: GenericAlias added 3.8 - protecting for 3.7
    if (hasattr(typing, 'GenericAlias') and
            isinstance(obj, getattr(typing, 'GenericAlias'))):
        return repr(obj)
    if isinstance(obj, type):
        if obj.__module__ == 'builtins':
            return obj.__qualname__
        return f'{obj.__module__}.{obj.__qualname__}'
    if obj is ...:
        return ('...')
    if isinstance(obj, FunctionType):
        return obj.__name__
    return repr(obj)


INDENT = '    '


def _line_indent(line: str, indent: str):
    stripped_line = line_strip(line)
    if not stripped_line or stripped_line == '\n':
        return stripped_line
    return indent + stripped_line


def reindent(snippet, num_indent=1):
    """Reindents the provided code snippet."""
    if not snippet:
        return ''
    indent = INDENT * num_indent
    return ''.join(
        _line_indent(line, indent)
        for line in snippet.splitlines(keepends=True))


def get_line_indent(line):
    """Returns the indentation of passed lines by counting spaces."""
    count = 0
    for c in line:
        if c == ' ':
            count += 1
        else:
            return count
    return count


def unindent_doc(doc):
    """Removes the indentation in subsequent lines of docstrings."""
    lines = doc.split('\n')
    if not lines:
        return doc
    out = [lines[0]]
    for line in lines[1:]:
        if line.startswith(INDENT):
            out.append(line[len(INDENT):])
        else:
            out.append(line)
    return '\n'.join(out)


def unindent_function(doc):
    """Removes the indentation in subsequent lines of docstrings."""
    lines = doc.split('\n')
    if not lines:
        return doc
    indent_count = get_line_indent(lines[0])
    indent = ' ' * indent_count
    out = [lines[0][indent_count:]]
    for line in lines[1:]:
        if line.startswith(indent):
            out.append(line[indent_count:])
        else:
            out.append(line)
    return '\n'.join(out)


def escaped_str(s: str):
    """Escapes a string as a python code-safe string."""
    if s is None:
        return 'None'
    return "'" + repr('"' + s)[2:]


def escape_doc(s: str):
    """Escapes a string as a triple quote code-safe string."""
    if not s:
        return ''
    body = s.translate(
        str.maketrans({
            '\\': '\\\\',
            '"': '\\"',
            '\b': '\\b',
            '\r': '\\r'
        }))
    return f'"""{body}"""'


def escape_json(s: str):
    """Escapes a json string - similar with escape_doc, but no " escaping."""
    body = s.translate(str.maketrans({'\\': '\\\\', '\b': '\\b', '\r': '\\r'}))
    return f'"""{body}"""'


def code_format(snippet: str) -> str:
    """Formats code with yapf."""
    return FormatCode(code_strip(snippet), style_config='google')[0]


def unparse(node: ast.AST) -> str:
    """Extracts the code in an ast node and formats it."""
    return code_format(astunparse.unparse(node))


class NameVisitor(ast.NodeVisitor):
    """Visits an AST to find the variables that are set, names used."""

    def __init__(self):
        self.stored = []
        self.stored_set = set()
        self.loaded = []
        self.loaded_set = set()
        self.defined = []
        self.defined_set = set()
        self.classes = []
        self.classes_set = set()

    def visit_Name(self, node):  # pylint: disable=invalid-name
        if isinstance(node.ctx, ast.Store):
            # print(f'Store {ast.dump(node)}')
            if node.id not in self.stored_set:
                self.stored.append(node.id)
                self.stored_set.add(node.id)
        elif isinstance(node.ctx, ast.Load):
            if (node.id not in self.loaded_set and
                    node.id not in self.defined_set):
                self.loaded.append(node.id)
                self.loaded_set.add(node.id)

    def visit_FunctionDef(self, node):  # pylint: disable=invalid-name
        if node.name not in self.defined_set:
            self.defined.append(node.name)
            self.defined_set.add(node.name)

    def visit_ClassDef(self, node):  # pylint: disable=invalid-name
        if node.name not in self.classes_set:
            self.classes.append(node.name)
            self.classes_set.add(node.name)


def code_names(code: str) -> Tuple[Set[str], Set[str]]:
    """Returns the names loaded and stored in a code snippet"""
    code_ast = ast.parse(code)
    visitor = NameVisitor()
    visitor.visit(code_ast)
    return (visitor.loaded_set, visitor.stored_set)


@dataclasses.dataclass
class ParamDef:
    """Parameters for a notebook / pipeline module."""
    name: str = None
    attr: str = None
    annotation: Optional[str] = None
    value: str = None


def extract_params_from_list(body: List[ast.AST],
                             accept_attr: bool = False) -> List[ParamDef]:
    """Extract parameters from a list of assignments."""
    params = []
    for statement in body:
        if (not isinstance(statement, ast.Assign) and
                not isinstance(statement, ast.AnnAssign)):
            raise ValueError('Expecting only assignments in params code: '
                             f'{unparse(statement)}')

        annotation = None
        if isinstance(statement, ast.AnnAssign):
            target = statement.target
            annotation = unparse(statement.annotation).strip()
        else:
            if len(statement.targets) != 1:
                raise ValueError('Assignments in params code expected to '
                                 'assign just a variable for: '
                                 f'{unparse(statement)}')
            target = statement.targets[0]
        attr = None
        raise_error = False
        if isinstance(target, ast.Attribute):
            if not accept_attr:
                raise_error = True
            else:
                attr = target.attr
                target = target.value
        if not isinstance(target, ast.Name) or raise_error:
            raise ValueError('Assignments in params code expected to '
                             'assign just a unattributed variables for: '
                             f'{unparse(statement)}')
        params.append(
            ParamDef(name=target.id,
                     attr=attr,
                     annotation=annotation,
                     value=unparse(statement.value).strip()))
    return params


def extract_params(code: ast.AST) -> List[ParamDef]:
    """Parses parameters from a code snippet ast.
    We expect a bunch of variable assignments."""
    if not isinstance(code, ast.Module):
        raise ValueError('Params code did not parse to a module.')
    return extract_params_from_list(code.body, False)


def get_params_values(paramscls: type, fname: str):
    """Returns the members of a class set in a member function."""
    values = {}
    if not hasattr(paramscls, fname):
        return values
    fun = getattr(paramscls, fname)
    if not callable(fun):
        return values
    source = unindent_function(inspect.getsource(fun))
    try:
        code_ast = ast.parse(source)
    except Exception as e:
        raise RuntimeError(
            f'Error parsing function: {fname} of {paramscls}') from e
    if (not isinstance(code_ast, ast.Module) or len(code_ast.body) != 1 or
            not isinstance(code_ast.body[0], ast.FunctionDef)):
        return {}
    stmts = [
        stmt for stmt in code_ast.body[0].body
        if isinstance(stmt, (ast.AnnAssign, ast.Assign))
    ]
    for param in extract_params_from_list(stmts, True):
        if param.name == 'self' and param.attr:
            values[param.attr] = param
    return values


class FunctionParser:
    """Extracts body and arguments from a function AST."""

    def __init__(self, fun):
        if not callable(fun):
            raise ValueError(f'Provided object `{fun}` is not a function')
        self.fun = fun
        self.source = unindent_function(inspect.getsource(fun))
        self.source_lines = self.source.splitlines()
        try:
            self.code_ast = ast.parse(self.source)
        except Exception as e:
            raise RuntimeError(
                f'Error parsing snippet:\n{self.source}`)') from e
        if (not isinstance(self.code_ast, ast.Module) or
                len(self.code_ast.body) != 1 or
                not isinstance(self.code_ast.body[0], ast.FunctionDef)):
            raise ValueError(f'Provided callable {fun} source does not '
                             'compile function definition')

    @property
    def body(self):
        return self.code_ast.body[0].body

    @property
    def args(self):
        return [arg.arg for arg in self.code_ast.body[0].args.args]

    @property
    def unparsed_body(self):
        unparsed_ast = ast.parse('')
        unparsed_ast.body.extend(self.body)
        return unparse(unparsed_ast)

    @property
    def is_simple_return(self):
        return len(self.body) == 1 and isinstance(self.body[0], ast.Return)

    @property
    def unparsed_body_no_return(self):
        unparsed_ast = ast.parse('')
        if self.body and isinstance(self.body[-1], ast.Return):
            unparsed_ast.body.extend(self.body[:-1])
        else:
            unparsed_ast.body.extend(self.body)
        return unparse(unparsed_ast)

    @property
    def unparsed_return_expression(self):
        if not self.is_simple_return:
            raise ValueError('Expecting simple expression return in function '
                             f'`{self.fun}`')
        unparsed_ast = ast.parse('')
        unparsed_ast.body.append(self.body[0].value)
        return unparse(unparsed_ast)


def snippet_at_line(codelines: List[str], line: int, context_len: int = 2):
    """Extracts a code snippet from provided code lines with some context."""
    out = []
    for line_num in range(line - context_len, line + context_len + 1):
        if 0 <= line_num < len(codelines):
            if line_num == line:
                prefix = '--> '
            else:
                prefix = '    '
            out.append(f'{prefix}{line_num + 1:4}    {codelines[line_num]}')
    return '\n'.join(out)


def treat_code_exec_error(code: str, e: Exception):
    """Pulls the proper message from executing a code snippet."""
    te = traceback.TracebackException.from_exception(e)
    codelines = code.splitlines()
    messages = []
    for fe in te.stack:
        if fe.filename == '<string>':
            messages.append(snippet_at_line(codelines, fe.lineno - 1))
    trace = (f'Cell execution error:\n{type(e).__name__}: {e}\n' +
             '\n'.join(messages) + ('-' * 80))
    raise e from RuntimeError(trace)
