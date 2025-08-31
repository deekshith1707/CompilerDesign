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

extern void addSymbol(const char* token_name, const char* token, const char* token_type);

void record_token(const char* token, const char* token_type) {
    if (strcmp(token, "identifier") != 0) {
        addSymbol(yytext, token, token_type);
    }
}

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

"if"            { update_position(); record_token("KEYWORD", "IF"); return IF; }
"else"          { update_position(); record_token("KEYWORD", "ELSE"); return ELSE; }
"while"         { update_position(); record_token("KEYWORD", "WHILE"); return WHILE; }
"for"           { update_position(); record_token("KEYWORD", "FOR"); return FOR; }
"do"            { update_position(); record_token("KEYWORD", "DO"); return DO; }
"switch"        { update_position(); record_token("KEYWORD", "SWITCH"); return SWITCH; }
"case"          { update_position(); record_token("KEYWORD", "CASE"); return CASE; }
"default"       { update_position(); record_token("KEYWORD", "DEFAULT"); return DEFAULT; }
"break"         { update_position(); record_token("KEYWORD", "BREAK"); return BREAK; }
"continue"      { update_position(); record_token("KEYWORD", "CONTINUE"); return CONTINUE; }
"return"        { update_position(); record_token("KEYWORD", "RETURN"); return RETURN; }
"int"           { update_position(); record_token("TYPE", "INT"); return INT; }
"char"          { update_position(); record_token("TYPE", "CHAR"); return CHAR_TOKEN; }
"float"         { update_position(); record_token("TYPE", "FLOAT"); return FLOAT; }
"double"        { update_position(); record_token("TYPE", "DOUBLE"); return DOUBLE; }
"long"          { update_position(); record_token("TYPE", "LONG"); return LONG; }
"short"         { update_position(); record_token("TYPE", "SHORT"); return SHORT; }
"unsigned"      { update_position(); record_token("TYPE", "UNSIGNED"); return UNSIGNED; }
"signed"        { update_position(); record_token("TYPE", "SIGNED"); return SIGNED; }
"void"          { update_position(); record_token("TYPE", "VOID"); return VOID; }
"struct"        { update_position(); record_token("KEYWORD", "STRUCT"); return STRUCT; }
"enum"          { update_position(); record_token("KEYWORD", "ENUM"); return ENUM; }
"union"         { update_position(); record_token("KEYWORD", "UNION"); return UNION; }
"typedef"       { update_position(); record_token("KEYWORD", "TYPEDEF"); return TYPEDEF; }
"static"        { update_position(); record_token("STORAGE_CLASS", "STATIC"); return STATIC; }
"extern"        { update_position(); record_token("STORAGE_CLASS", "EXTERN"); return EXTERN; }
"auto"          { update_position(); record_token("STORAGE_CLASS", "AUTO"); return AUTO; }
"register"      { update_position(); record_token("STORAGE_CLASS", "REGISTER"); return REGISTER; }
"const"         { update_position(); record_token("QUALIFIER", "CONST"); return CONST; }
"volatile"      { update_position(); record_token("QUALIFIER", "VOLATILE"); return VOLATILE; }
"goto"          { update_position(); record_token("KEYWORD", "GOTO"); return GOTO; }
"until"         { update_position(); record_token("KEYWORD", "UNTIL"); return UNTIL; }
"sizeof"        { update_position(); record_token("KEYWORD", "SIZEOF"); return SIZEOF; }

{INCLUDE_HEADER}  { update_position(); record_token("PREPROCESSOR", "INCLUDE"); return PREPROCESSOR; }
{INCLUDE_FILE}    { update_position(); record_token("PREPROCESSOR", "INCLUDE"); return PREPROCESSOR; }
{PREPROCESSOR}    { update_position(); record_token("PREPROCESSOR", "DIRECTIVE"); return PREPROCESSOR; }

{IDENTIFIER}    { 
    update_position(); 
    if (is_type_name(yytext)) {
        yylval.str = strdup(yytext);
        return TYPE_NAME;
    }
    yylval.str = strdup(yytext);
    return IDENTIFIER; 
}

{FLOAT}         { update_position(); record_token("CONSTANT", "FLOAT"); return FLOAT_CONSTANT; }
{HEX_INTEGER}   { update_position(); record_token("CONSTANT", "HEX"); return HEX_CONSTANT; }
{OCT_INTEGER}   { update_position(); record_token("CONSTANT", "OCTAL"); return OCTAL_CONSTANT; }
{BIN_INTEGER}   { update_position(); record_token("CONSTANT", "BINARY"); return BINARY_CONSTANT; }
{INTEGER}       { update_position(); record_token("CONSTANT", "INTEGER"); return INTEGER_CONSTANT; }

{STRING}        { update_position(); record_token("STRING", "LITERAL"); return STRING_LITERAL; }
{UNTERMINATED_STRING} { print_error_context("Unterminated string literal"); }

{CHAR}          { update_position(); record_token("CONSTANT", "CHAR"); return CHAR_CONSTANT; }
{UNTERMINATED_CHAR}  { print_error_context("Unterminated character literal"); }

"+="            { update_position(); record_token("OPERATOR", "PLUS_ASSIGN"); return PLUS_ASSIGN; }
"-="            { update_position(); record_token("OPERATOR", "MINUS_ASSIGN"); return MINUS_ASSIGN; }
"*="            { update_position(); record_token("OPERATOR", "MUL_ASSIGN"); return MUL_ASSIGN; }
"/="            { update_position(); record_token("OPERATOR", "DIV_ASSIGN"); return DIV_ASSIGN; }
"%="            { update_position(); record_token("OPERATOR", "MOD_ASSIGN"); return MOD_ASSIGN; }
"&="            { update_position(); record_token("OPERATOR", "AND_ASSIGN"); return AND_ASSIGN; }
"|="            { update_position(); record_token("OPERATOR", "OR_ASSIGN"); return OR_ASSIGN; }
"^="            { update_position(); record_token("OPERATOR", "XOR_ASSIGN"); return XOR_ASSIGN; }
"<<="           { update_position(); record_token("OPERATOR", "LSHIFT_ASSIGN"); return LSHIFT_ASSIGN; }
">>="           { update_position(); record_token("OPERATOR", "RSHIFT_ASSIGN"); return RSHIFT_ASSIGN; }
"="             { update_position(); record_token("OPERATOR", "ASSIGN"); return ASSIGN; }

"=="            { update_position(); record_token("OPERATOR", "EQ"); return EQ; }
"!="            { update_position(); record_token("OPERATOR", "NE"); return NE; }
"<="            { update_position(); record_token("OPERATOR", "LE"); return LE; }
">="            { update_position(); record_token("OPERATOR", "GE"); return GE; }
"<"             { update_position(); record_token("OPERATOR", "LT"); return LT; }
">"             { update_position(); record_token("OPERATOR", "GT"); return GT; }

"&&"            { update_position(); record_token("OPERATOR", "LOGICAL_AND"); return LOGICAL_AND; }
"||"            { update_position(); record_token("OPERATOR", "LOGICAL_OR"); return LOGICAL_OR; }
"!"             { update_position(); record_token("OPERATOR", "LOGICAL_NOT"); return LOGICAL_NOT; }

"<<"            { update_position(); record_token("OPERATOR", "LSHIFT"); return LSHIFT; }
">>"            { update_position(); record_token("OPERATOR", "RSHIFT"); return RSHIFT; }
"&"             { update_position(); record_token("OPERATOR", "BITWISE_AND"); return BITWISE_AND; }
"|"             { update_position(); record_token("OPERATOR", "BITWISE_OR"); return BITWISE_OR; }
"^"             { update_position(); record_token("OPERATOR", "BITWISE_XOR"); return BITWISE_XOR; }
"~"             { update_position(); record_token("OPERATOR", "BITWISE_NOT"); return BITWISE_NOT; }

"++"            { update_position(); record_token("OPERATOR", "INCREMENT"); return INCREMENT; }
"--"            { update_position(); record_token("OPERATOR", "DECREMENT"); return DECREMENT; }
"+"             { update_position(); record_token("OPERATOR", "PLUS"); return PLUS; }
"-"             { update_position(); record_token("OPERATOR", "MINUS"); return MINUS; }
"*"             { update_position(); record_token("OPERATOR", "MULTIPLY"); return MULTIPLY; }
"/"             { update_position(); record_token("OPERATOR", "DIVIDE"); return DIVIDE; }
"%"             { update_position(); record_token("OPERATOR", "MODULO"); return MODULO; }

"->"            { update_position(); record_token("OPERATOR", "ARROW"); return ARROW; }
"?"             { update_position(); record_token("OPERATOR", "QUESTION"); return QUESTION; }

"("             { update_position(); record_token("PUNCTUATION", "LPAREN"); return LPAREN; }
")"             { update_position(); record_token("PUNCTUATION", "RPAREN"); return RPAREN; }
"{"             { update_position(); record_token("PUNCTUATION", "LBRACE"); return LBRACE; }
"}"             { update_position(); record_token("PUNCTUATION", "RBRACE"); return RBRACE; }
"["             { update_position(); record_token("PUNCTUATION", "LBRACKET"); return LBRACKET; }
"]"             { update_position(); record_token("PUNCTUATION", "RBRACKET"); return RBRACKET; }
";"             { update_position(); record_token("PUNCTUATION", "SEMICOLON"); return SEMICOLON; }
","             { update_position(); record_token("PUNCTUATION", "COMMA"); return COMMA; }
"."             { update_position(); record_token("PUNCTUATION", "DOT"); return DOT; }
":"             { update_position(); record_token("PUNCTUATION", "COLON"); return COLON; }

[ \t]+          { update_position(); }
\n              { update_position(); }

.               { 
    print_error_context("Unrecognized character");
}

%%