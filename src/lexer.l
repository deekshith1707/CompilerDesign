%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int column = 1;
char line_buffer[1024];
int line_length = 0;

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
}

void print_error_context(const char* message) {
    int error_column = column - strlen(yytext);
    printf("Lexical Error on line %d, column %d: %s\n", yylineno, error_column, message);
    printf("%s\n", line_buffer);
    for (int i = 0; i < error_column - 1; i++) {
        printf(" ");
    }
    printf("^\n");
}

void print_token(const char* token_type) {
    printf("%-20s %-20s\n", yytext, token_type);
    update_position();
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

INTEGER         {DIGIT}+
HEX_INTEGER     0[xX]{HEX_DIGIT}+
OCT_INTEGER     0{OCT_DIGIT}+
BIN_INTEGER     0[bB]{BIN_DIGIT}+

ESCAPE_SEQ      \\([nrtbfv\\'"]|[0-7]{1,3}|x{HEX_DIGIT}{1,2})
STRING          \"({ESCAPE_SEQ}|[^"\\\n])*\"
UNTERMINATED_STRING  \"({ESCAPE_SEQ}|[^"\\\n])*
CHAR            '({ESCAPE_SEQ}|[^'\\\n])'
UNTERMINATED_CHAR    '({ESCAPE_SEQ}|[^'\\\n])

PREPROCESSOR    ^[ \t]*#[ \t]*[a-zA-Z]+

%%

"//".* { }
"/*"([^*]|\*+[^*/])*\*+"/" { }

"if"            { print_token("keyword"); }
"else"          { print_token("keyword"); }
"while"         { print_token("keyword"); }
"for"           { print_token("keyword"); }
"do"            { print_token("keyword"); }
"switch"        { print_token("keyword"); }
"case"          { print_token("keyword"); }
"default"       { print_token("keyword"); }
"break"         { print_token("keyword"); }
"continue"      { print_token("keyword"); }
"return"        { print_token("keyword"); }
"int"           { print_token("keyword"); }
"char"          { print_token("keyword"); }
"void"          { print_token("keyword"); }
"struct"        { print_token("keyword"); }
"enum"          { print_token("keyword"); }
"union"         { print_token("keyword"); }
"typedef"       { print_token("keyword"); }
"static"        { print_token("keyword"); }
"goto"          { print_token("keyword"); }
"until"         { print_token("keyword"); }

{PREPROCESSOR}    { print_token("preprocessor"); }

{IDENTIFIER}    { print_token("identifier"); }

{HEX_INTEGER}   { print_token("hex_constant"); }
{OCT_INTEGER}   { print_token("octal_constant"); }
{BIN_INTEGER}   { print_token("binary_constant"); }
{INTEGER}       { print_token("integer_constant"); }

{STRING}        { print_token("string_literal"); }
{UNTERMINATED_STRING} { print_error_context("Unterminated string literal"); }

{CHAR}          { print_token("char_constant"); }
{UNTERMINATED_CHAR}  { print_error_context("Unterminated character literal"); }

"+"             { print_token("operator"); }
"-"             { print_token("operator"); }
"*"             { print_token("operator"); }
"/"             { print_token("operator"); }
"%"             { print_token("operator"); }
"++"            { print_token("operator"); }
"--"            { print_token("operator"); }
"=="            { print_token("operator"); }
"!="            { print_token("operator"); }
">"             { print_token("operator"); }
"<"             { print_token("operator"); }
">="            { print_token("operator"); }
"<="            { print_token("operator"); }
"&&"            { print_token("operator"); }
"||"            { print_token("operator"); }
"!"             { print_token("operator"); }
"&"             { print_token("operator"); }
"|"             { print_token("operator"); }
"^"             { print_token("operator"); }
"~"             { print_token("operator"); }
"<<"            { print_token("operator"); }
">>"            { print_token("operator"); }
"="             { print_token("operator"); }
"+="            { print_token("operator"); }
"-="            { print_token("operator"); }
"*="            { print_token("operator"); }
"/="            { print_token("operator"); }
"%="            { print_token("operator"); }
"->"            { print_token("operator"); }

"("             { print_token("punctuation"); }
")"             { print_token("punctuation"); }
"{"             { print_token("punctuation"); }
"}"             { print_token("punctuation"); }
"["             { print_token("punctuation"); }
"]"             { print_token("punctuation"); }
";"             { print_token("punctuation"); }
","             { print_token("punctuation"); }
"."             { print_token("punctuation"); }
":"             { print_token("punctuation"); }

[ \t]+          { update_position(); }
\n              { update_position(); }

.               { print_error_context("Unrecognized character"); }

%%

void yyerror(const char* msg) {
    print_error_context(msg);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) {
        fprintf(stderr, "Error: Could not open file '%s'\n", argv[1]);
        return 1;
    }

    printf("%-20s %-20s\n", "Lexeme", "Token");
    yylex();

    fclose(yyin);
    return 0;
}
