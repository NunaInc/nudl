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
    : LBRACKET computeExpressions RBRACKET
    ;

mapDefinition
    : LBRACKET mapElements RBRACKET
    ;

mapElements
    : mapElement (COMMA mapElement)*
    ;

mapElement
    : computeExpression COLON computeExpression
    ;

structDefinition
    : emptyStruct | arrayDefinition | mapDefinition
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

assignExpression
    : composedIdentifier typeAssignment? EQ_SINGLE computeExpression
    ;

inlineBody
    : INLINE_BODY
    ;

functionDefinition
    : KW_DEF KW_METHOD? IDENTIFIER LPAREN paramsList? RPAREN
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
    ;

module
    : moduleElement* EOF
    ;
