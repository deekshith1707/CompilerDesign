%{
#include "../obj/parser.tab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern YYSTYPE yylval;
extern int is_type_name(const char* name);

int column = 1;
char line_buffer[1024];
int line_length = 0;
int token_processed = 0;

void update_position() {
    for(int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            column = 1;
            line_length = 0;
            memset(line_buffer, 0, sizeof(line_buffer));
        } else {
            column++;
            if (line_length < sizeof(line_buffer) - 1) {
                line_buffer[line_length++] = yytext[i];
                line_buffer[line_length] = '\0';
            }
        }
    }
    token_processed = 1;
}

void print_error_context(const char* message) {
    if (token_processed) return;

    int error_column = column - strlen(yytext);
    printf("Lexical Error on line %d, column %d: %s\n", yylineno, error_column, message);
    printf("%s\n", line_buffer);
    for (int i = 0; i < error_column - 1; i++) {
        printf(" ");
    }
    printf("^\n");
}
%}

%option yylineno
%option noyywrap nounput noinput

DIGIT       [0-9]
LETTER      [a-zA-Z_]
IDENTIFIER  {LETTER}({LETTER}|{DIGIT})*
HEX_DIGIT   [0-9a-fA-F]
OCT_DIGIT   [0-7]
BIN_DIGIT   [01]

INTEGER         {DIGIT}+[uUlL]*
HEX_INTEGER     0[xX]{HEX_DIGIT}+[uUlL]*
OCT_INTEGER     0{OCT_DIGIT}+[uUlL]*
BIN_INTEGER     0[bB]{BIN_DIGIT}+[uUlL]*

FLOAT_1         {DIGIT}+\.{DIGIT}+([eE][+-]?{DIGIT}+)?[fF]?
FLOAT_2         {DIGIT}+[eE][+-]?{DIGIT}+[fF]?
FLOAT_3         \.{DIGIT}+([eE][+-]?{DIGIT}+)?[fF]?
FLOAT_4         {DIGIT}+\.([eE][+-]?{DIGIT}+)?[fF]?
FLOAT_5         {DIGIT}+[fF]
FLOAT           {FLOAT_1}|{FLOAT_2}|{FLOAT_3}|{FLOAT_4}|{FLOAT_5}

ESCAPE_SEQ      \\([nrtbfv\\'"]|[0-7]{1,3}|x{HEX_DIGIT}{1,2})
STRING          \"({ESCAPE_SEQ}|[^"\\\n])*\"
UNTERMINATED_STRING  \"({ESCAPE_SEQ}|[^"\\\n])*
CHAR            '({ESCAPE_SEQ}|[^'\\\n])'
UNTERMINATED_CHAR    '({ESCAPE_SEQ}|[^'\\\n])

INCLUDE_HEADER  ^[ \t]*#[ \t]*include[ \t]*<[^>]+>
INCLUDE_FILE    ^[ \t]*#[ \t]*include[ \t]*\"[^\"]+\"
PREPROCESSOR    ^[ \t]*#[ \t]*[a-zA-Z]+

%%

"//".* { update_position(); }
"/*"([^*]|\*+[^*/])*\*+"/" { update_position(); }

"if"            { update_position(); return IF; }
"else"          { update_position(); return ELSE; }
"while"         { update_position(); return WHILE; }
"for"           { update_position(); return FOR; }
"do"            { update_position(); return DO; }
"switch"        { update_position(); return SWITCH; }
"case"          { update_position(); return CASE; }
"default"       { update_position(); return DEFAULT; }
"break"         { update_position(); return BREAK; }
"continue"      { update_position(); return CONTINUE; }
"return"        { update_position(); return RETURN; }
"int"           { update_position(); return INT; }
"char"          { update_position(); return CHAR_TOKEN; }
"float"         { update_position(); return FLOAT_TOKEN; }
"double"        { update_position(); return DOUBLE; }
"long"          { update_position(); return LONG; }
"short"         { update_position(); return SHORT; }
"unsigned"      { update_position(); return UNSIGNED; }
"signed"        { update_position(); return SIGNED; }
"void"          { update_position(); return VOID; }
"bool"          { update_position(); return BOOL; }
"true"          { update_position(); yylval.node = createNode(NODE_CONSTANT, "1"); return INTEGER_CONSTANT; }
"false"         { update_position(); yylval.node = createNode(NODE_CONSTANT, "0"); return INTEGER_CONSTANT; }
"struct"        { update_position(); return STRUCT; }
"enum"          { update_position(); return ENUM; }
"union"         { update_position(); return UNION; }
"typedef"       { update_position(); return TYPEDEF; }
"static"        { update_position(); return STATIC; }
"extern"        { update_position(); return EXTERN; }
"auto"          { update_position(); return AUTO; }
"register"      { update_position(); return REGISTER; }
"const"         { update_position(); return CONST; }
"volatile"      { update_position(); return VOLATILE; }
"goto"          { update_position(); return GOTO; }
"until"         { update_position(); return UNTIL; }
"sizeof"        { update_position(); return SIZEOF; }

{INCLUDE_HEADER}  { update_position(); yylval.node = createNode(NODE_PREPROCESSOR, yytext); return PREPROCESSOR; }
{INCLUDE_FILE}    { update_position(); yylval.node = createNode(NODE_PREPROCESSOR, yytext); return PREPROCESSOR; }
{PREPROCESSOR}    { update_position(); yylval.node = createNode(NODE_PREPROCESSOR, yytext); return PREPROCESSOR; }

{IDENTIFIER}    {
    update_position();
    // Check if it's a typedef'd type name or a regular identifier
    if (is_type_name(yytext)) {
        // Create a node of type TYPE_NAME for the parser
        yylval.node = createNode(NODE_TYPE_NAME, yytext);
        return TYPE_NAME;
    } else {
        // Create a node of type IDENTIFIER for the parser
        yylval.node = createNode(NODE_IDENTIFIER, yytext);
        return IDENTIFIER;
    }
}

{FLOAT}         { 
    update_position(); 
    yylval.node = createNode(NODE_CONSTANT, yytext);
    return FLOAT_CONSTANT;
}
{HEX_INTEGER}   { 
    update_position(); 
    yylval.node = createNode(NODE_CONSTANT, yytext);
    return HEX_CONSTANT; 
}
{OCT_INTEGER}   { 
    update_position(); 
    yylval.node = createNode(NODE_CONSTANT, yytext);
    return OCTAL_CONSTANT;
}
{BIN_INTEGER}   { 
    update_position(); 
    yylval.node = createNode(NODE_CONSTANT, yytext);
    return BINARY_CONSTANT; 
}
{INTEGER}       { 
    update_position(); 
    yylval.node = createNode(NODE_CONSTANT, yytext);
    return INTEGER_CONSTANT;
}

{STRING}        { 
    update_position(); 
    yylval.node = createNode(NODE_STRING_LITERAL, yytext);
    return STRING_LITERAL;
}
{UNTERMINATED_STRING} { print_error_context("Unterminated string literal"); }

{CHAR}          { 
    update_position(); 
    yylval.node = createNode(NODE_CONSTANT, yytext);
    return CHAR_CONSTANT;
}
{UNTERMINATED_CHAR}  { print_error_context("Unterminated character literal"); }
"+="            { update_position(); return PLUS_ASSIGN; }
"-="            { update_position(); return MINUS_ASSIGN; }
"*="            { update_position(); return MUL_ASSIGN; }
"/="            { update_position(); return DIV_ASSIGN; }
"%="            { update_position(); return MOD_ASSIGN; }
"&="            { update_position(); return AND_ASSIGN; }
"|="            { update_position(); return OR_ASSIGN; }
"^="            { update_position(); return XOR_ASSIGN; }
"<<="           { update_position(); return LSHIFT_ASSIGN; }
">>="           { update_position(); return RSHIFT_ASSIGN; }
"="             { update_position(); return ASSIGN; }

"=="            { update_position(); return EQ; }
"!="            { update_position(); return NE; }
"<="            { update_position(); return LE; }
">="            { update_position(); return GE; }
"<"             { update_position(); return LT; }
">"             { update_position(); return GT; }

"&&"            { update_position(); return LOGICAL_AND; }
"||"            { update_position(); return LOGICAL_OR; }
"!"             { update_position(); return LOGICAL_NOT; }

"<<"            { update_position(); return LSHIFT; }
">>"            { update_position(); return RSHIFT; }
"&"             { update_position(); return BITWISE_AND; }
"|"             { update_position(); return BITWISE_OR; }
"^"             { update_position(); return BITWISE_XOR; }
"~"             { update_position(); return BITWISE_NOT; }

"++"            { update_position(); return INCREMENT; }
"--"            { update_position(); return DECREMENT; }
"+"             { update_position(); return PLUS; }
"-"             { update_position(); return MINUS; }
"*"             { update_position(); return MULTIPLY; }
"/"             { update_position(); return DIVIDE; }
"%"             { update_position(); return MODULO; }

"->"            { update_position(); return ARROW; }
"?"             { update_position(); return QUESTION; }

"("             { update_position(); return LPAREN; }
")"             { update_position(); return RPAREN; }
"{"             { update_position(); return LBRACE; }
"}"             { update_position(); return RBRACE; }
"["             { update_position(); return LBRACKET; }
"]"             { update_position(); return RBRACKET; }
";"             { update_position(); return SEMICOLON; }
","             { update_position(); return COMMA; }
"."             { update_position(); return DOT; }
":"             { update_position(); return COLON; }

[ \t]+          { update_position(); }
\n              { update_position(); }

.               {
    print_error_context("Unrecognized character");
}

%%