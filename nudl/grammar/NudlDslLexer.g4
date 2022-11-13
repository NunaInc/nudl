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

lexer grammar NudlDslLexer;

// Keywords

KW_IF: 'if';
KW_ELIF: 'elif';
KW_ELSE: 'else';
KW_DEF: 'def';
KW_RETURN: 'return';
KW_YIELD: 'yield';
KW_SCHEMA: 'schema';
KW_IMPORT: 'import';
KW_PASS: 'pass';
KW_PARAM: 'param';
KW_METHOD: 'method';
KW_CONSTRUCTOR: 'constructor';
KW_TYPEDEF: 'typedef';
KW_PRAGMA: 'pragma';
KW_WITH: 'with';
KW_MAIN_FUNCTION: 'main_function';

// Reserved Literals
KW_NULL: 'null';
KW_FALSE: 'false';
KW_TRUE: 'true';

// Operators

KW_OR: 'or';
KW_AND: 'and';
KW_XOR: 'xor';
KW_BETWEEN: 'between';
KW_NOT: 'not';
KW_IN: 'in';

fragment KW_WEEKS: 'weeks';
fragment KW_DAYS: 'days';
fragment KW_HOURS: 'hours';
fragment KW_MINUTES: 'minutes';
fragment KW_SECONDS: 'seconds';

fragment KW_TIMEINTERVAL_NAMES
    : KW_WEEKS
    | KW_DAYS
    | KW_HOURS
    | KW_MINUTES
    | KW_SECONDS
    ;

fragment IDENTIFIER_KEYWORDS
    : KW_TIMEINTERVAL_NAMES
    | KW_METHOD
    | KW_CONSTRUCTOR
    | KW_PARAM
    | KW_SCHEMA
    | KW_MAIN_FUNCTION
    ;

fragment LETTER: [a-zA-Z];
fragment DEC_DIGIT: [0-9];
fragment NONZERO_DEC_DIGIT: [1-9];
fragment HEX_DIGIT: [0-9a-fA-F];

IDENTIFIER
    : IDENTIFIER_NON_DIGIT (IDENTIFIER_NON_DIGIT | DEC_DIGIT)*
    | IDENTIFIER_KEYWORDS
    ;

fragment
IDENTIFIER_NON_DIGIT
    : LETTER | UNDERSCORE
    ;

LITERAL_DECIMAL
    : '0'
    | NONZERO_DEC_DIGIT DEC_DIGIT*
    ;

LITERAL_TIMERANGE
    : ('0'
    | NONZERO_DEC_DIGIT DEC_DIGIT*) (KW_TIMEINTERVAL_NAMES)
    ;

LITERAL_UNSIGNED_DECIMAL
    : '0u'
    | NONZERO_DEC_DIGIT DEC_DIGIT* ('u' | 'U')
    ;

LITERAL_HEXADECIMAL
    : '0' [xX] HEX_DIGIT+
    ;

LITERAL_UNSIGNED_HEXADECIMAL
    : '0' [xX] HEX_DIGIT+ ('u' | 'U')
    ;

LITERAL_FLOAT
    :   LITERAL_DEC_FLOAT 'f'
    ;

LITERAL_DOUBLE
    :   LITERAL_DEC_FLOAT
    ;

LITERAL_STRING
    : QUOTE_DOUBLE STRING_CHAR* QUOTE_DOUBLE
    ;
LITERAL_BYTES
    : 'b' QUOTE_DOUBLE BYTES_CHAR* QUOTE_DOUBLE
    ;

fragment LITERAL_DEC_FLOAT
    :   LITERAL_FRACTIONAL PART_EXPONENT?
    |   DEC_DIGIT+ PART_EXPONENT
    ;

fragment LITERAL_FRACTIONAL
    : DEC_DIGIT* DOT DEC_DIGIT+
    | DEC_DIGIT+ DOT
    ;

fragment PART_EXPONENT
    : [eE] (PLUS | MINUS)? DEC_DIGIT+
    ;

fragment STRING_CHAR
    :   ~["\\\r\n]      // "
    |   STRING_ESCAPE
    |   UNICODE_CHAR
    |   '\\\n'
    |   '\\\r\n'
    ;

fragment BYTES_CHAR
    : STRING_CHAR
    | '\\x' HEX_DIGIT HEX_DIGIT
    ;

fragment STRING_ESCAPE
    : '\\' ['"fnrt\\]  // '
    ;

fragment UNICODE_CHAR
    : '\\u' UNICODE_HEXA_BLOCK
    | '\\U' UNICODE_HEXA_BLOCK UNICODE_HEXA_BLOCK
    ;

fragment UNICODE_HEXA_BLOCK
    : HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT
    ;

fragment UNDERSCORE: '_';

fragment QUOTE_DOUBLE: '"';
// QUOTE_SINGLE: '\'';

LBRACE: '{';
LBRACKET: '[';
LPAREN: '(';
RBRACE: '}';
RBRACKET: ']';
RPAREN: ')';

SEMICOLON: ';';
COLON: ':';
DOT: '.';
COMMA: ',';

EQ_DOUBLE: '==';
EQ_SINGLE: '=';
LT: '<';
LE: '<=';
GT: '>';
GE: '>=';
NE: '!=';

PLUS: '+';
STAR: '*';
MINUS: '-';
SLASH: '/';
PERCENT: '%';
QUESTION: '?';
AMPERSAND: '&';
VBAR: '|';
NEGATE: '~';
CARET: '^';

PASS_TO: '=>';

INLINE_BODY: '[[' IDENTIFIER ']]' .*? '[[end]]';

MULTI_LINE_COMMENT: '/*' .*? '*/' -> skip;
SINFLE_LINE_COMMENT: '//' ~('\n'|'\r')* ('\n' | '\r' | EOF) -> skip;
WHITESPACE:   [ \t]+ -> skip;
NEWLINE: ('\r' '\n'? | '\n') -> skip;