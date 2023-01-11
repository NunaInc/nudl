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
"""Tests the code utilities module."""

import ast
import dataclasses
import datetime
import decimal
import unittest
from nuna_nudl.jupyter import codeutil
from typing import List, Optional


def testfun(x: int, y: int):
    """Stupid function for testing."""
    z = x + y
    return z + x * y if x < 20 else z


def testreturnfun(x: int, y: int):
    return x * y


@dataclasses.dataclass
class Foo:
    x: Optional[int] = 4
    y: List[str] = None

    def default_values(self):
        self.x: Optional[int] = 43
        self.y: List[str] = ['foo', 'bar']


class CodeUtilTest(unittest.TestCase):

    def testValid(self):
        self.assertTrue(codeutil.is_valid_identifier('FooBar'))
        self.assertTrue(codeutil.is_valid_identifier('Foo_Bar'))
        self.assertTrue(codeutil.is_valid_identifier('_Foo_Bar'))
        self.assertTrue(codeutil.is_valid_identifier('_foo1_bar2'))
        self.assertFalse(codeutil.is_valid_identifier('1FooBar'))
        self.assertFalse(codeutil.is_valid_identifier('F@o_Bar'))
        self.assertFalse(codeutil.is_valid_identifier('_Foo Bar'))

        self.assertTrue(codeutil.is_valid_step_name('foo_bar'))
        self.assertTrue(codeutil.is_valid_step_name('foo_bar2'))
        self.assertTrue(codeutil.is_valid_step_name('foo_bar2_'))
        self.assertFalse(codeutil.is_valid_step_name('_foo_bar'))
        self.assertFalse(codeutil.is_valid_step_name('foo_Bar'))
        self.assertFalse(codeutil.is_valid_step_name('2foo_Bar'))
        self.assertFalse(codeutil.is_valid_step_name('FooBar'))

        self.assertEqual(codeutil.capitalize_step_name('foo_bar'), 'FooBar')
        self.assertEqual(codeutil.capitalize_step_name('foo1_bar_'), 'Foo1Bar')
        self.assertEqual(codeutil.capitalize_step_name('f_b_c_1'), 'FBC1')

    def testAstCheckExpr(self):
        self.assertTrue(codeutil.is_simple_expression(ast.parse('a + b')))
        self.assertTrue(codeutil.is_simple_expression(ast.parse('"foo bar"')))
        self.assertTrue(codeutil.is_simple_expression(ast.parse('[1, 2]')))
        self.assertTrue(
            codeutil.is_simple_expression(ast.parse('1 if foo else 2')))
        self.assertTrue(
            codeutil.is_simple_expression(ast.parse('{"x": 1, "y": "foo"}')))
        self.assertFalse(
            codeutil.is_simple_expression(ast.parse('return a + b')))
        self.assertFalse(
            codeutil.is_simple_expression(ast.parse('if a == b: x = 20')))
        self.assertFalse(
            codeutil.is_simple_expression(ast.parse('for f in x:\n  y += f')))

    def testAstCheckReturn(self):
        self.assertTrue(codeutil.is_simple_return(ast.parse('return a + b')))
        self.assertTrue(codeutil.is_simple_return(
            ast.parse('return "foo bar"')))
        self.assertTrue(
            codeutil.is_simple_return(ast.parse('return (1 if a > b else 33)')))
        self.assertFalse(
            codeutil.is_simple_return(ast.parse('{"x": 1, "y": "foo"}')))
        self.assertFalse(
            codeutil.is_simple_return(ast.parse('if a == b: x = 20')))
        self.assertFalse(
            codeutil.is_simple_return(ast.parse('for f in x:\n  y += f')))

    def testLineStrip(self):
        self.assertEqual(codeutil.line_strip('  foo'), '  foo')
        self.assertEqual(codeutil.line_strip('  foo \t'), '  foo')
        self.assertEqual(codeutil.line_strip('  foo \t\n'), '  foo\n')
        self.assertEqual(codeutil.line_strip('  foo bar  \n'), '  foo bar\n')

    def testCodeStrip(self):
        self.assertEqual(codeutil.code_strip('  foo\n  bar\n'),
                         '  foo\n  bar\n')
        self.assertEqual(codeutil.code_strip('  foo \t\n  bar \n'),
                         '  foo\n  bar\n')

    def testReindent(self):
        self.assertEqual(codeutil.reindent('foo\n    bar  \nbaz\n'), '    foo\n'
                         '        bar\n'
                         '    baz\n')
        self.assertEqual(codeutil.reindent('foo\n    bar  \nbaz\n', 2),
                         '        foo\n'
                         '            bar\n'
                         '        baz\n')

    def testEscapedStr(self):
        self.assertEqual(codeutil.escaped_str('foo'), '\'foo\'')
        self.assertEqual(codeutil.escaped_str('f"o\no'), '\'f"o\\no\'')
        self.assertEqual(codeutil.escaped_str('f"o\no b\'a\x02'),
                         '\'f"o\\no b\\\'a\\x02\'')

    def testEscapeDoc(self):
        self.assertEqual(codeutil.escape_doc('foo'), '"""foo"""')
        self.assertEqual(codeutil.escape_doc('fo\'o"\n\r\bbar'),
                         '"""fo\'o\\"\n\\r\\bbar"""')

    def testCodeFormat(self):
        self.assertEqual(
            codeutil.code_format('[{\n  "a": 400,  \n    "B": "bar"}]'),
            '[{"a": 400, "B": "bar"}]\n')

    def testUnparse(self):
        self.assertEqual(
            codeutil.unparse(ast.parse('[{\n  "a": 400,  \n    "B": "bar"}]')),
            '[{\'a\': 400, \'B\': \'bar\'}]\n')

    def testVisitor(self):
        pt = ast.parse("""
a = foo + 20
def f(x):
    y = x + 20
    return y
b = a + f(bar)
""")
        visitor = codeutil.NameVisitor()
        visitor.visit(pt)
        self.assertEqual(visitor.stored, ['a', 'b'])
        self.assertEqual(visitor.loaded, ['foo', 'a', 'bar'])
        self.assertEqual(visitor.defined, ['f'])

    def testParams(self):
        pt = ast.parse("""
a = 20 + 30
c: str = 'foo bar'
y = a + c
""")
        params = codeutil.extract_params(pt)
        self.assertEqual(params, [
            codeutil.ParamDef('a', None, None, '(20 + 30)'),
            codeutil.ParamDef('c', None, 'str', '\'foo bar\''),
            codeutil.ParamDef('y', None, None, '(a + c)')
        ])

    def testFunctionParser(self):
        fp = codeutil.FunctionParser(testfun)
        body = """'Stupid function for testing.'
z = (x + y)
return ((z + (x * y)) if (x < 20) else z)
"""
        self.assertEqual(codeutil.unparse(fp.body), body)
        self.assertEqual(fp.args, ['x', 'y'])
        self.assertEqual(fp.unparsed_body, body)
        fpret = codeutil.FunctionParser(testreturnfun)
        self.assertEqual(fpret.unparsed_return_expression, '(x * y)\n')

    def test_get_params_values(self):
        self.x: Optional[int] = 43
        self.y: List[str] = ['foo', 'bar']
        self.assertEqual(
            codeutil.get_params_values(Foo, 'default_values'), {
                'x':
                    codeutil.ParamDef(name='self',
                                      attr='x',
                                      annotation='Optional[int]',
                                      value='43'),
                'y':
                    codeutil.ParamDef(name='self',
                                      attr='y',
                                      annotation='List[str]',
                                      value='[\'foo\', \'bar\']')
            })


if __name__ == '__main__':
    unittest.main()
