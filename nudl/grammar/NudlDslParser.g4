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

parser grammar NudlDslParser;

options {
    tokenVocab = NudlDslLexer;
}

literal
    : KW_NULL | KW_TRUE | KW_FALSE
    | LITERAL_DECIMAL
    | LITERAL_UNSIGNED_DECIMAL
    | LITERAL_HEXADECIMAL
    | LITERAL_UNSIGNED_HEXADECIMAL
    | LITERAL_FLOAT
    | LITERAL_DOUBLE
    | LITERAL_STRING
    | LITERAL_BYTES
    | LITERAL_TIMERANGE+
    ;

emptyStruct
    : LBRACKET RBRACKET
    ;

arrayDefinition
    : LBRACKET computeExpressions COMMA? RBRACKET
    ;

mapDefinition
    : LBRACKET mapElements COMMA? RBRACKET
    ;

mapElements
    : mapElement (COMMA mapElement)*
    ;

mapElement
    : computeExpression COLON computeExpression
    ;

namedTupleDefinition
    : LBRACE namedTupleElements COMMA? RBRACE
    ;

namedTupleElements
    : namedTupleElement (COMMA namedTupleElement)*
    ;

namedTupleElement
    : IDENTIFIER typeAssignment? EQ_SINGLE computeExpression
    ;

structDefinition
    : emptyStruct | arrayDefinition | mapDefinition | namedTupleDefinition
    ;

dotIdentifier
    : DOT IDENTIFIER;

composedIdentifier
    : IDENTIFIER dotIdentifier*;

ifExpression
    : KW_IF LPAREN computeExpression RPAREN
        expressionBlock
        (elifExpression | elseExpression)?
    ;

elifExpression
    : KW_ELIF LPAREN computeExpression RPAREN
        expressionBlock
        (elifExpression | elseExpression)?
    ;
elseExpression
    : KW_ELSE expressionBlock
    ;

withExpression
    : KW_WITH LPAREN computeExpression RPAREN
        expressionBlock
    ;

returnExpression
    : KW_RETURN computeExpression
    ;

yieldExpression
    : KW_YIELD computeExpression
    ;

passExpression
    : KW_PASS
    ;

typeAssignment
    : COLON typeExpression
    ;

typeExpression
    : composedIdentifier typeTemplate?
    | typeNamedArgument
    ;

typeTemplate
    : LT typeTemplateArgument (COMMA typeTemplateArgument)* GT
    ;

typeTemplateArgument
    : LITERAL_DECIMAL
    | typeExpression
    ;

typeNamedArgument
    : LBRACE IDENTIFIER typeAssignment? RBRACE
    ;

typeDefinition
    : KW_TYPEDEF IDENTIFIER EQ_SINGLE typeExpression
    inlineBody*
    ;

assignExpression
    : composedIdentifier typeAssignment? EQ_SINGLE computeExpression
    ;

inlineBody
    : INLINE_BODY
    ;

functionAnnotation
    : KW_METHOD | KW_CONSTRUCTOR | KW_MAIN_FUNCTION
    ;

functionDefinition
    : KW_DEF functionAnnotation? IDENTIFIER LPAREN paramsList? RPAREN
        typeAssignment? PASS_TO (expressionBlock | inlineBody+)
      SEMICOLON*
    ;

paramsList
    : paramDefinition (COMMA paramDefinition)*
    ;

paramDefinition
    : IDENTIFIER typeAssignment? (EQ_SINGLE computeExpression)?
    ;

functionCall
    : functionName LPAREN argumentList? RPAREN
    ;

functionName
    : composedIdentifier
    | typeExpression
    ;

argumentList
    : argumentSpec (COMMA argumentSpec)*
    ;

argumentSpec
    : (IDENTIFIER EQ_SINGLE)? computeExpression
    ;

lambdaExpression
    : ( paramDefinition
        | LPAREN paramDefinition (COMMA paramDefinition)* RPAREN typeAssignment? )
        PASS_TO expressionBlock
    ;

parenthesisedExpression
    : LPAREN computeExpression RPAREN
    ;

// Note: in the way the grammar is built, at this point
// we never hit functionCall unless using a template type
// construct, instead we reach it through
// composedIdentifier => postfixExpression => postfixValue ( args )
// May want to make the type a regular primary expression and
// just remove functionCall.
primaryExpression
    : parenthesisedExpression
    | composedIdentifier
    | structDefinition
    | functionCall
    | literal
    ;

postfixExpression
    : primaryExpression postfixValue*
    ;

postfixValue
    : LBRACKET computeExpression RBRACKET
    | LPAREN argumentList? RPAREN
    | DOT (IDENTIFIER | functionCall)
    ;

unaryOperatorExpression
    : (postfixExpression
        | unaryOperator postfixExpression)
    ;
unaryOperator: PLUS | MINUS | NEGATE | KW_NOT;

// In operator precedence order:
multiplicativeExpression
    : unaryOperatorExpression (multiplicativeOperator unaryOperatorExpression)*
    ;

multiplicativeOperator
    : STAR | SLASH | PERCENT
    ;

additiveExpression
    : multiplicativeExpression (additiveOperator multiplicativeExpression)*
    ;

additiveOperator
    : PLUS | MINUS
    ;

shiftExpression
    : additiveExpression (shiftOperator additiveExpression)*
    ;

shiftOperator
    : LT LT | GT GT
    ;

relationalExpression
    : shiftExpression (relationalOperator shiftExpression)?
    ;

relationalOperator
    : LT | LE | GE | GT
    ;

equalityExpression
    : relationalExpression (equalityOperator relationalExpression)?
    ;

equalityOperator
    : EQ_DOUBLE | NE
    ;

andExpression
    : equalityExpression (AMPERSAND equalityExpression)*
    ;

xorExpression
    : andExpression (CARET andExpression)*
    ;

orExpression
    : xorExpression (VBAR xorExpression)*
    ;

betweenExpression
    : orExpression (KW_BETWEEN LPAREN orExpression
            COMMA orExpression RPAREN)?
    ;

inExpression
    : betweenExpression (KW_IN betweenExpression)?
    ;

logicalAndExpression
    : inExpression (KW_AND inExpression)*
    ;

logicalXorExpression
    : logicalAndExpression (KW_XOR logicalAndExpression)*
    ;

logicalOrExpression
    : logicalXorExpression (KW_OR logicalXorExpression)*
    ;

conditionalExpression
    : logicalOrExpression (QUESTION LPAREN logicalOrExpression
            COMMA logicalOrExpression RPAREN)?
    ;

computeExpression
    : conditionalExpression
    | lambdaExpression
    | assignExpression
    ;

computeExpressions
    : computeExpression (COMMA computeExpression)*
    ;

expressionBlock
    : LBRACE blockBody RBRACE
    | blockElement
    ;

blockBody
    : blockElement+ (SEMICOLON blockElement+)* SEMICOLON*
    ;

pragmaExpression
    : KW_PRAGMA IDENTIFIER (LBRACE computeExpression RBRACE)?
    ;

blockElement
    : (computeExpression
    | ifExpression
    | withExpression
    | returnExpression
    | yieldExpression
    | passExpression
    | pragmaExpression)
    ;

schemaDefinition
    : KW_SCHEMA IDENTIFIER EQ_SINGLE LBRACE
        fieldsDefinition?
        RBRACE
        SEMICOLON*
    ;

fieldsDefinition
    : fieldDefinition (SEMICOLON+ fieldDefinition)* SEMICOLON*
    ;

fieldDefinition
    : IDENTIFIER typeAssignment fieldOptions?
    ;

fieldOptions
    : LBRACKET fieldOption (SEMICOLON+ fieldOption)* SEMICOLON* RBRACKET
    ;

fieldOption
    : IDENTIFIER EQ_SINGLE literal
    ;

importStatement
    : KW_IMPORT importSpecification (COMMA importSpecification)* SEMICOLON*
    ;

importSpecification
    : (IDENTIFIER EQ_SINGLE)? composedIdentifier
    ;

moduleAssignment
    : assignQualifier* assignExpression SEMICOLON*
    ;

assignQualifier
    : KW_PARAM
    ;

moduleElement
    : importStatement
    | schemaDefinition
    | functionDefinition
    | moduleAssignment
    | pragmaExpression
    | typeDefinition
    SEMICOLON*
    ;

module
    : moduleElement* EOF
    ;
