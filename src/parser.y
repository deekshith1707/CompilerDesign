%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yylex();
extern int yylineno;
extern char* yytext;
extern FILE* yyin;

void yyerror(const char* msg);

int error_count = 0;
int recovering_from_error = 0;

typedef struct {
    char token_name[100];
    char token[100];
    char token_type[100];
} SymbolEntry;

SymbolEntry symbol_table[1000];
int symbolCount = 0;

char currentType[100] = "";

int in_typedef = 0;

int is_type_name(const char* name) {
    for (int i = 0; i < symbolCount; i++) {
        if (strcmp(symbol_table[i].token_name, name) == 0 &&
            strcmp(symbol_table[i].token_type, "TYPEDEF") == 0) {
            return 1;
        }
    }
    return 0;
}

void addSymbol(const char* token_name, const char* token, const char* token_type) {
    if (strcmp(token_type, "TYPEDEF") == 0) {
        if (is_type_name(token_name)) return;
    }
    strcpy(symbol_table[symbolCount].token_name, token_name);
    strcpy(symbol_table[symbolCount].token, token);
    strcpy(symbol_table[symbolCount].token_type, token_type);
    symbolCount++;
}

void insertSymbol(const char* name, const char* type, int is_function) {
    if (is_function) {
        addSymbol(name, "identifier", "PROCEDURE");
    } else {
        char upperType[100];
        strcpy(upperType, type);
        for (int i = 0; upperType[i]; i++) {
            if (upperType[i] >= 'a' && upperType[i] <= 'z') {
                upperType[i] = upperType[i] - 'a' + 'A';
            }
        }
        addSymbol(name, "identifier", upperType);
    }
}

void setCurrentType(const char* type) {
    strcpy(currentType, type);
}
%}

%union {
    char* str;
}

%token <str> IDENTIFIER
%token <str> TYPE_NAME
%token INTEGER_CONSTANT HEX_CONSTANT OCTAL_CONSTANT BINARY_CONSTANT
%token FLOAT_CONSTANT
%token STRING_LITERAL CHAR_CONSTANT
%token PREPROCESSOR

%token IF ELSE WHILE FOR DO SWITCH CASE DEFAULT BREAK CONTINUE RETURN
%token INT CHAR_TOKEN FLOAT DOUBLE LONG SHORT UNSIGNED SIGNED VOID
%token STRUCT ENUM UNION TYPEDEF STATIC EXTERN AUTO REGISTER
%token CONST VOLATILE GOTO UNTIL SIZEOF

%token ASSIGN PLUS_ASSIGN MINUS_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token AND_ASSIGN OR_ASSIGN XOR_ASSIGN LSHIFT_ASSIGN RSHIFT_ASSIGN
%token EQ NE LT GT LE GE
%token LOGICAL_AND LOGICAL_OR
%token LSHIFT RSHIFT
%token INCREMENT DECREMENT
%token ARROW
%token PLUS MINUS MULTIPLY DIVIDE MODULO
%token BITWISE_AND BITWISE_OR BITWISE_XOR BITWISE_NOT
%token LOGICAL_NOT
%token QUESTION

%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET
%token SEMICOLON COMMA DOT COLON

%type <str> declarator direct_declarator

%right ASSIGN PLUS_ASSIGN MINUS_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN AND_ASSIGN OR_ASSIGN XOR_ASSIGN LSHIFT_ASSIGN RSHIFT_ASSIGN
%right QUESTION COLON
%left LOGICAL_OR
%left LOGICAL_AND
%left BITWISE_OR
%left BITWISE_XOR
%left BITWISE_AND
%left EQ NE
%left LT GT LE GE
%left LSHIFT RSHIFT
%left PLUS MINUS
%left MULTIPLY DIVIDE MODULO
%right UNARY_MINUS UNARY_PLUS LOGICAL_NOT BITWISE_NOT INCREMENT DECREMENT SIZEOF
%left LPAREN RPAREN LBRACKET RBRACKET ARROW DOT

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%start program

%%

program:
    translation_unit
    ;

translation_unit:
    external_declaration
    | translation_unit external_declaration
    ;

external_declaration:
    function_definition
    | declaration
    | preprocessor_directive
    | error SEMICOLON {
        printf("PARSE: Recovered from error, found semicolon\n");
        yyerrok;
        recovering_from_error = 0;
    }
    | error RBRACE {
        printf("PARSE: Recovered from error, found closing brace\n");
        yyerrok;
        recovering_from_error = 0;
    }
    ;

preprocessor_directive:
    PREPROCESSOR
    ;

function_definition:
    declaration_specifiers declarator compound_statement {
        if ($2 != NULL) {
            insertSymbol($2, currentType, 1);
            free($2);
        }
        recovering_from_error = 0;
    }
    | declaration_specifiers declarator error {
        yyerror("Error in function definition - missing or invalid compound statement");
        if ($2 != NULL) {
            insertSymbol($2, currentType, 1);
            free($2);
        }
        yyerrok;
        recovering_from_error = 0;
    }
    ;

declaration:
    declaration_specifiers SEMICOLON {
        in_typedef = 0;
        recovering_from_error = 0;
    }
    | declaration_specifiers init_declarator_list SEMICOLON {
        in_typedef = 0;
        recovering_from_error = 0;
    }
    | declaration_specifiers init_declarator_list error {
        yyerror("Missing semicolon in declaration");
        in_typedef = 0;
        yyerrok;
        recovering_from_error = 0;
    }
    | declaration_specifiers error SEMICOLON {
        yyerror("Error in declaration");
        in_typedef = 0;
        yyerrok;
        recovering_from_error = 0;
    }
    ;

declaration_specifiers:
    specifier
    | declaration_specifiers specifier
    ;

specifier:
    storage_class_specifier
    | type_specifier
    | type_qualifier
    ;

storage_class_specifier:
    TYPEDEF { in_typedef = 1; }
    | EXTERN
    | STATIC
    | AUTO
    | REGISTER
    ;

type_specifier:
    VOID { setCurrentType("void"); }
    | CHAR_TOKEN { setCurrentType("char"); }
    | SHORT { setCurrentType("short"); }
    | INT { setCurrentType("int"); }
    | LONG { setCurrentType("long"); }
    | FLOAT { setCurrentType("float"); }
    | DOUBLE { setCurrentType("double"); }
    | SIGNED { setCurrentType("signed"); }
    | UNSIGNED { setCurrentType("unsigned"); }
    | struct_specifier
    | enum_specifier
    | TYPE_NAME { setCurrentType($1); free($1); }
    ;

type_qualifier:
    CONST
    | VOLATILE
    ;

init_declarator_list:
    init_declarator
    | init_declarator_list COMMA init_declarator
    ;

init_declarator:
    declarator {
        if ($1 != NULL) {
            if (in_typedef) {
                addSymbol($1, "identifier", "TYPEDEF");
            } else {
                insertSymbol($1, currentType, 0);
            }
            free($1);
        }
    }
    | declarator ASSIGN initializer {
        if ($1 != NULL) {
            if (in_typedef) {
                addSymbol($1, "identifier", "TYPEDEF");
            } else {
                insertSymbol($1, currentType, 0);
            }
            free($1);
        }
    }
    ;

initializer:
    assignment_expression
    | LBRACE initializer_list RBRACE
    | LBRACE initializer_list COMMA RBRACE
    ;

initializer_list:
    initializer
    | initializer_list COMMA initializer
    ;

struct_specifier:
    STRUCT IDENTIFIER LBRACE struct_declaration_list RBRACE {
        if (in_typedef) {
            setCurrentType("struct");
        } else {
            insertSymbol($2, "struct", 0);
        }
        free($2);
    }
    | STRUCT LBRACE struct_declaration_list RBRACE {
        setCurrentType("struct");
    }
    | STRUCT IDENTIFIER {
        if (in_typedef) {
            setCurrentType("struct");
        } else {
            insertSymbol($2, "struct", 0);
        }
        free($2);
    }
    | UNION IDENTIFIER LBRACE struct_declaration_list RBRACE {
        if (in_typedef) {
            setCurrentType("union");
        } else {
            insertSymbol($2, "union", 0);
        }
        free($2);
    }
    | UNION LBRACE struct_declaration_list RBRACE {
        setCurrentType("union");
    }
    | UNION IDENTIFIER {
        if (in_typedef) {
            setCurrentType("union");
        } else {
            insertSymbol($2, "union", 0);
        }
        free($2);
    }
    ;

struct_declaration_list:
    struct_declaration
    | struct_declaration_list struct_declaration
    ;

struct_declaration:
    declaration_specifiers struct_declarator_list SEMICOLON
    ;

struct_declarator_list:
    struct_declarator
    | struct_declarator_list COMMA struct_declarator
    ;

struct_declarator:
    declarator {
        if ($1 != NULL) {
            insertSymbol($1, currentType, 0);
            free($1);
        }
    }
    | declarator COLON constant_expression {
        if ($1 != NULL) {
            insertSymbol($1, currentType, 0);
            free($1);
        }
    }
    | COLON constant_expression
    ;

enum_specifier:
    ENUM LBRACE enumerator_list RBRACE {
        setCurrentType("enum");
    }
    | ENUM IDENTIFIER LBRACE enumerator_list RBRACE {
        if (in_typedef) {
            setCurrentType("enum");
        } else {
            insertSymbol($2, "enum", 0);
        }
        free($2);
        setCurrentType("enum");
    }
    | ENUM IDENTIFIER {
        if (in_typedef) {
            setCurrentType("enum");
        } else {
            insertSymbol($2, "enum", 0);
        }
        free($2);
        setCurrentType("enum");
    }
    ;

enumerator_list:
    enumerator
    | enumerator_list COMMA enumerator
    ;

enumerator:
    IDENTIFIER {
        insertSymbol($1, "enum_constant", 0);
        free($1);
    }
    | IDENTIFIER ASSIGN constant_expression {
        insertSymbol($1, "enum_constant", 0);
        free($1);
    }
    ;

declarator:
    pointer direct_declarator {
        $$ = $2;
    }
    | direct_declarator {
        $$ = $1;
    }
    ;

direct_declarator:
    IDENTIFIER post_declarator {
        $$ = strdup($1);
        free($1);
    }
    | LPAREN declarator RPAREN post_declarator {
        $$ = $2;
    }
    ;

post_declarator:
    
    | LBRACKET constant_expression_opt RBRACKET post_declarator
    | LPAREN parameter_type_list RPAREN post_declarator
    | LPAREN RPAREN post_declarator
    ;

constant_expression_opt:
    
    | constant_expression
    ;

pointer:
    MULTIPLY
    | MULTIPLY type_qualifier_list
    | MULTIPLY pointer
    | MULTIPLY type_qualifier_list pointer
    ;

type_qualifier_list:
    type_qualifier
    | type_qualifier_list type_qualifier
    ;

parameter_type_list:
    parameter_list
    ;

parameter_list:
    parameter_declaration
    | parameter_list COMMA parameter_declaration
    ;

parameter_declaration:
    declaration_specifiers declarator {
        if ($2 != NULL) {
            insertSymbol($2, currentType, 0);
            free($2);
        }
    }
    | declaration_specifiers abstract_declarator
    | declaration_specifiers
    ;

abstract_declarator:
    pointer
    | direct_abstract_declarator
    | pointer direct_abstract_declarator
    ;

direct_abstract_declarator:
    LPAREN abstract_declarator RPAREN
    | LBRACKET constant_expression_opt RBRACKET
    | direct_abstract_declarator LBRACKET constant_expression_opt RBRACKET
    | LPAREN parameter_type_list RPAREN
    | direct_abstract_declarator LPAREN parameter_type_list RPAREN
    ;

statement:
    labeled_statement
    | compound_statement
    | expression_statement
    | selection_statement
    | iteration_statement
    | jump_statement
    ;

labeled_statement:
    IDENTIFIER COLON statement {
        insertSymbol($1, "label", 0);
        free($1);
    }
    | CASE constant_expression COLON statement
    | DEFAULT COLON statement
    ;

compound_statement:
    LBRACE RBRACE {
        recovering_from_error = 0;
    }
    | LBRACE block_item_list RBRACE {
        recovering_from_error = 0;
    }
    | LBRACE block_item_list error {
        yyerror("Missing closing brace in compound statement");
        yyerrok;
    }
    | LBRACE error RBRACE {
        yyerror("Error in compound statement body");
        yyerrok;
        recovering_from_error = 0;
    }
    ;

block_item_list:
    block_item
    | block_item_list block_item
    ;

block_item:
    declaration
    | statement
    | error RBRACE {
        printf("PARSE: Recovered from error in block, found closing brace\n");
        yyerrok;
        recovering_from_error = 0;
    }
    ;

expression_statement:
    SEMICOLON {
        recovering_from_error = 0;
    }
    | expression SEMICOLON {
        recovering_from_error = 0;
    }
    | expression error {
        yyerror("Missing semicolon after expression");
        yyerrok;
    }
    | error SEMICOLON {
        printf("PARSE: Recovered from expression error\n");
        yyerrok;
        recovering_from_error = 0;
    }
    ;

selection_statement:
    IF LPAREN expression RPAREN statement %prec LOWER_THAN_ELSE
    | IF LPAREN expression RPAREN statement ELSE statement
    | SWITCH LPAREN expression RPAREN statement
    | IF LPAREN expression error {
        yyerror("Missing closing parenthesis in if condition");
        yyerrok;
    } statement
    | IF error {
        yyerror("Error in if condition");
        yyerrok;
    } statement
    ;

iteration_statement:
    WHILE LPAREN expression RPAREN statement
    | DO statement WHILE LPAREN expression RPAREN SEMICOLON
    | DO statement UNTIL LPAREN expression RPAREN SEMICOLON
    | FOR LPAREN expression_statement expression_statement RPAREN statement
    | FOR LPAREN expression_statement expression_statement expression RPAREN statement
    | FOR LPAREN declaration expression_statement RPAREN statement
    | FOR LPAREN declaration expression_statement expression RPAREN statement
    | UNTIL LPAREN expression RPAREN statement
    | FOR LPAREN error RPAREN {
        yyerror("Error in for loop");
        yyerrok;
    } statement
    | FOR LPAREN expression_statement expression_statement error {
        yyerror("Missing closing parenthesis in for loop");
        yyerrok;
    } statement
    | WHILE LPAREN expression error {
        yyerror("Missing closing parenthesis in while loop");  
        yyerrok;
    } statement
    | WHILE LPAREN error RPAREN {
        yyerror("Invalid expression in while condition");
        yyerrok;
    } statement
    ;

jump_statement:
    GOTO IDENTIFIER SEMICOLON {
        free($2);
    }
    | CONTINUE SEMICOLON
    | BREAK SEMICOLON
    | RETURN SEMICOLON
    | RETURN expression SEMICOLON
    | GOTO IDENTIFIER error {
        yyerror("Missing semicolon after goto statement");
        free($2);
        yyerrok;
    }
    | RETURN expression error {
        yyerror("Missing semicolon after return statement");
        yyerrok;
    }
    ;

expression:
    assignment_expression
    | expression COMMA assignment_expression
    ;

assignment_expression:
    conditional_expression
    | unary_expression assignment_operator assignment_expression
    ;

assignment_operator:
    ASSIGN | MUL_ASSIGN | DIV_ASSIGN | MOD_ASSIGN | PLUS_ASSIGN | MINUS_ASSIGN
    | LSHIFT_ASSIGN | RSHIFT_ASSIGN | AND_ASSIGN | XOR_ASSIGN | OR_ASSIGN
    ;

conditional_expression:
    logical_or_expression
    | logical_or_expression QUESTION expression COLON conditional_expression
    ;

constant_expression:
    conditional_expression
    ;

logical_or_expression:
    logical_and_expression
    | logical_or_expression LOGICAL_OR logical_and_expression
    ;

logical_and_expression:
    inclusive_or_expression
    | logical_and_expression LOGICAL_AND inclusive_or_expression
    ;

inclusive_or_expression:
    exclusive_or_expression
    | inclusive_or_expression BITWISE_OR exclusive_or_expression
    ;

exclusive_or_expression:
    and_expression
    | exclusive_or_expression BITWISE_XOR and_expression
    ;

and_expression:
    equality_expression
    | and_expression BITWISE_AND equality_expression
    ;

equality_expression:
    relational_expression
    | equality_expression EQ relational_expression
    | equality_expression NE relational_expression
    ;

relational_expression:
    shift_expression
    | relational_expression LT shift_expression
    | relational_expression GT shift_expression
    | relational_expression LE shift_expression
    | relational_expression GE shift_expression
    ;

shift_expression:
    additive_expression
    | shift_expression LSHIFT additive_expression
    | shift_expression RSHIFT additive_expression
    ;

additive_expression:
    multiplicative_expression
    | additive_expression PLUS multiplicative_expression
    | additive_expression MINUS multiplicative_expression
    ;

multiplicative_expression:
    cast_expression
    | multiplicative_expression MULTIPLY cast_expression
    | multiplicative_expression DIVIDE cast_expression
    | multiplicative_expression MODULO cast_expression
    ;

cast_expression:
    unary_expression
    | LPAREN type_name RPAREN cast_expression
    ;

type_name:
    declaration_specifiers
    | declaration_specifiers abstract_declarator
    ;

unary_expression:
    postfix_expression
    | INCREMENT unary_expression
    | DECREMENT unary_expression
    | unary_operator cast_expression
    | SIZEOF unary_expression
    | SIZEOF LPAREN type_name RPAREN
    ;

unary_operator:
    BITWISE_AND | MULTIPLY | PLUS %prec UNARY_PLUS | MINUS %prec UNARY_MINUS
    | BITWISE_NOT | LOGICAL_NOT
    ;

postfix_expression:
    primary_expression
    | postfix_expression LBRACKET expression RBRACKET
    | postfix_expression LPAREN RPAREN
    | postfix_expression LPAREN argument_expression_list RPAREN
    | postfix_expression DOT IDENTIFIER {
        free($3);
    }
    | postfix_expression ARROW IDENTIFIER {
        free($3);
    }
    | postfix_expression INCREMENT
    | postfix_expression DECREMENT
    ;

primary_expression:
    IDENTIFIER {
        free($1);
    }
    | INTEGER_CONSTANT | HEX_CONSTANT | OCTAL_CONSTANT | BINARY_CONSTANT
    | FLOAT_CONSTANT
    | CHAR_CONSTANT
    | STRING_LITERAL
    | LPAREN expression RPAREN
    ;

argument_expression_list:
    assignment_expression
    | argument_expression_list COMMA assignment_expression
    ;

%%

void yyerror(const char* msg) {
    if (!recovering_from_error) {
        fprintf(stderr, "Syntax Error on line %d: %s at '%s'\n", yylineno, msg, yytext);
        error_count++;
        recovering_from_error = 1;
    }
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

    printf("Syntax Analyzer for C Language\n");
    printf("==============================\n\n");
    printf("Parsing file: %s\n\n", argv[1]);
    
    printf("%-20s %-20s\n", "Token", "Token_Type");
    printf("----------------------------------------\n");

    int result = yyparse();
    
    for (int i = 0; i < symbolCount; i++) {
        if (strcmp(symbol_table[i].token, "identifier") == 0) {
            printf("%-20s %-20s\n", symbol_table[i].token_name, symbol_table[i].token_type);
        }
    }
    
    printf("----------------------------------------\n");
    
    if (error_count == 0) {
        printf("\nParsing completed successfully.\n");
        printf("No syntax errors found.\n");
    } else {
        printf("\nParsing completed with errors.\n");
        printf("Total syntax errors: %d\n", error_count);
    }

    fclose(yyin);
    return (error_count > 0) ? 1 : 0;
}