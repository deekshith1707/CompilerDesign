/* ========================================================== */
/* PART 1: C Declarations and Includes (for parser.tab.c)     */
/* ========================================================== */
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"

// Global variables used by the parser
extern int yylineno;
extern char* yytext;
extern FILE* yyin;
int error_count = 0;
int recovering_from_error = 0;
int in_typedef = 0;

// Global AST root, to be passed to the IR generator
TreeNode* ast_root = NULL;

// Forward declarations
extern int yylex();
void yyerror(const char* msg);

%}

/* =================================================================== */
/* PART 2: Code required by the lexer (placed in parser.tab.h)         */
/* =================================================================== */
%code requires {
    #include "ast.h"
    #include "symbol_table.h"
}

/* ========================================================== */
/* PART 3: Bison declarations                                 */
/* ========================================================== */
%union {
    TreeNode* node;
}

%token <node> IDENTIFIER
%token <node> TYPE_NAME
%token <node> INTEGER_CONSTANT HEX_CONSTANT OCTAL_CONSTANT BINARY_CONSTANT
%token <node> FLOAT_CONSTANT
%token <node> STRING_LITERAL CHAR_CONSTANT
%token <node> PREPROCESSOR
%token <node> IF ELSE WHILE FOR DO SWITCH CASE DEFAULT BREAK CONTINUE RETURN
%token <node> INT CHAR_TOKEN FLOAT_TOKEN DOUBLE LONG SHORT UNSIGNED SIGNED VOID
%token <node> STRUCT ENUM UNION TYPEDEF STATIC EXTERN AUTO REGISTER
%token <node> CONST VOLATILE GOTO UNTIL SIZEOF
%token <node> ASSIGN PLUS_ASSIGN MINUS_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token <node> AND_ASSIGN OR_ASSIGN XOR_ASSIGN LSHIFT_ASSIGN RSHIFT_ASSIGN
%token <node> EQ NE LT GT LE GE
%token <node> LOGICAL_AND LOGICAL_OR
%token <node> LSHIFT RSHIFT
%token <node> INCREMENT DECREMENT
%token <node> ARROW
%token <node> PLUS MINUS MULTIPLY DIVIDE MODULO
%token <node> BITWISE_AND BITWISE_OR BITWISE_XOR BITWISE_NOT
%token <node> LOGICAL_NOT
%token <node> QUESTION
%token <node> LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET
%token <node> SEMICOLON COMMA DOT COLON

%token UNARY_MINUS UNARY_PLUS

%type <node> program translation_unit external_declaration preprocessor_directive
%type <node> function_definition declaration init_declarator_list init_declarator
%type <node> declaration_specifiers specifier
%type <node> storage_class_specifier type_specifier type_qualifier
%type <node> struct_or_union_specifier struct_declaration_list struct_declaration
%type <node> struct_declarator_list struct_declarator
%type <node> enum_specifier enumerator_list enumerator
%type <node> declarator direct_declarator
%type <node> type_qualifier_list
%type <node> parameter_type_list parameter_list parameter_declaration
%type <node> abstract_declarator direct_abstract_declarator
%type <node> type_name
%type <node> statement labeled_statement compound_statement block_item_list block_item
%type <node> expression_statement
%type <node> selection_statement iteration_statement jump_statement
%type <node> if_header marker_while_start for_label_marker
%type <node> expression assignment_expression conditional_expression
%type <node> logical_or_expression logical_and_expression
%type <node> inclusive_or_expression exclusive_or_expression and_expression
%type <node> equality_expression relational_expression shift_expression
%type <node> additive_expression multiplicative_expression
%type <node> cast_expression unary_expression postfix_expression primary_expression
%type <node> constant_expression
%type <node> initializer initializer_list
%type <node> argument_expression_list
%type <node> do_header do_trailer

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

/* ========================================================== */
/* PART 4: Grammar Rules                                      */
/* ========================================================== */

program:
    translation_unit {
        $$ = createNode(NODE_PROGRAM, "program");
        addChild($$, $1);
        ast_root = $$; // Set the global AST root
        printf("\n=== PARSING COMPLETED SUCCESSFULLY ===\n");
    }
    ;

translation_unit:
    external_declaration {
        $$ = createNode(NODE_PROGRAM, "translation_unit");
        if ($1) addChild($$, $1);
    }
    | translation_unit external_declaration {
        $$ = $1;
        if ($2) addChild($$, $2);
    }
    ;

external_declaration:
    function_definition { $$ = $1; }
    | declaration { $$ = $1; }
    | preprocessor_directive { $$ = NULL; }
    ;

preprocessor_directive:
    PREPROCESSOR { $$ = NULL; }
    ;

function_definition:
    declaration_specifiers declarator
    {
        if ($2 && $2->value) {
            // Insert the function into the symbol table
            insertSymbol($2->value, currentType, 1);
        }
    }
    compound_statement
    {
        $$ = createNode(NODE_FUNCTION_DEFINITION, "function");
        addChild($$, $1); // declaration_specifiers
        addChild($$, $2); // declarator (name)
        addChild($$, $4); // compound_statement (body)
        
        exitScope(); // Exit the function's scope
        recovering_from_error = 0;
    }
    ;

declaration:
    declaration_specifiers SEMICOLON {
        $$ = createNode(NODE_DECLARATION, "declaration");
        addChild($$, $1);
        in_typedef = 0;
        recovering_from_error = 0;
    }
    | declaration_specifiers init_declarator_list SEMICOLON {
        $$ = createNode(NODE_DECLARATION, "declaration");
        addChild($$, $1);
        addChild($$, $2);
        in_typedef = 0;
        recovering_from_error = 0;
    }
    ;

declaration_specifiers:
    specifier {
        $$ = createNode(NODE_DECLARATION_SPECIFIERS, "decl_specs");
        addChild($$, $1);
    }
    | declaration_specifiers specifier {
        $$ = $1;
        addChild($$, $2);
    }
    ;

specifier:
    storage_class_specifier { $$ = $1; }
    | type_specifier { $$ = $1; }
    | type_qualifier { $$ = $1; }
    ;

storage_class_specifier:
    TYPEDEF {
        in_typedef = 1;
        $$ = createNode(NODE_STORAGE_CLASS_SPECIFIER, "typedef");
    }
    | EXTERN { $$ = createNode(NODE_STORAGE_CLASS_SPECIFIER, "extern"); }
    | STATIC { $$ = createNode(NODE_STORAGE_CLASS_SPECIFIER, "static"); }
    | AUTO { $$ = createNode(NODE_STORAGE_CLASS_SPECIFIER, "auto"); }
    | REGISTER { $$ = createNode(NODE_STORAGE_CLASS_SPECIFIER, "register"); }
    ;

type_specifier:
    VOID {
        setCurrentType("void");
        $$ = createNode(NODE_TYPE_SPECIFIER, "void");
    }
    | CHAR_TOKEN {
        setCurrentType("char");
        $$ = createNode(NODE_TYPE_SPECIFIER, "char");
    }
    | SHORT {
        setCurrentType("short");
        $$ = createNode(NODE_TYPE_SPECIFIER, "short");
    }
    | INT {
        setCurrentType("int");
        $$ = createNode(NODE_TYPE_SPECIFIER, "int");
    }
    | LONG {
        setCurrentType("long");
        $$ = createNode(NODE_TYPE_SPECIFIER, "long");
    }
    | FLOAT_TOKEN {
        setCurrentType("float");
        $$ = createNode(NODE_TYPE_SPECIFIER, "float");
    }
    | DOUBLE {
        setCurrentType("double");
        $$ = createNode(NODE_TYPE_SPECIFIER, "double");
    }
    | SIGNED {
        setCurrentType("signed");
        $$ = createNode(NODE_TYPE_SPECIFIER, "signed");
    }
    | UNSIGNED {
        setCurrentType("unsigned");
        $$ = createNode(NODE_TYPE_SPECIFIER, "unsigned");
    }
    | struct_or_union_specifier { $$ = $1; }
    | enum_specifier { $$ = $1; }
    | TYPE_NAME {
        if ($1 && $1->value) setCurrentType($1->value);
        $$ = $1;
    }
    ;

type_qualifier:
    CONST { $$ = createNode(NODE_TYPE_QUALIFIER, "const"); }
    | VOLATILE { $$ = createNode(NODE_TYPE_QUALIFIER, "volatile"); }
    ;

struct_or_union_specifier:
    STRUCT IDENTIFIER {
        setCurrentType("struct");
        $$ = createNode(NODE_STRUCT_SPECIFIER, "struct");
        addChild($$, $2);
    }
    | STRUCT LBRACE struct_declaration_list RBRACE {
        setCurrentType("struct");
        $$ = createNode(NODE_STRUCT_SPECIFIER, "struct");
        addChild($$,$3);
    }
    | STRUCT IDENTIFIER LBRACE struct_declaration_list RBRACE {
        setCurrentType("struct");
        $$ = createNode(NODE_STRUCT_SPECIFIER, "struct_definition");
        addChild($$, $2);
        addChild($$, $4);
    }
    | UNION IDENTIFIER {
        setCurrentType("union");
        $$ = createNode(NODE_STRUCT_SPECIFIER, "union");
        addChild($$, $2);
    }
    | UNION LBRACE struct_declaration_list RBRACE {
        setCurrentType("union");
        $$ = createNode(NODE_STRUCT_SPECIFIER, "union");
        addChild($$,$3);
    }
    | UNION IDENTIFIER LBRACE struct_declaration_list RBRACE {
        setCurrentType("union");
        $$ = createNode(NODE_STRUCT_SPECIFIER, "union_definition");
        addChild($$, $2);
        addChild($$, $4);
    }
    ;

struct_declaration_list:
    struct_declaration { $$ = $1; }
    | struct_declaration_list struct_declaration { $$ = $1; addChild($$, $2); }
    ;

struct_declaration:
    declaration_specifiers struct_declarator_list SEMICOLON {
        $$ = createNode(NODE_DECLARATION, "struct_decl");
        addChild($$, $1);
        addChild($$, $2);
    }
    ;

struct_declarator_list:
    struct_declarator { $$ = $1; }
    | struct_declarator_list COMMA struct_declarator { $$ = $1; addChild($$, $3); }
    ;

struct_declarator:
    declarator { $$ = $1; }
    | COLON constant_expression { $$ = $2; }
    | declarator COLON constant_expression { $$ = $1; addChild($$, $3); }
    ;

enum_specifier:
    ENUM LBRACE enumerator_list RBRACE {
        setCurrentType("enum");
        $$ = createNode(NODE_ENUM_SPECIFIER, "enum");
        addChild($$, $3);
    }
    | ENUM IDENTIFIER {
        setCurrentType("enum");
        $$ = createNode(NODE_ENUM_SPECIFIER, "enum");
        addChild($$, $2);
    }
    | ENUM IDENTIFIER LBRACE enumerator_list RBRACE {
        setCurrentType("enum");
        $$ = createNode(NODE_ENUM_SPECIFIER, "enum");
        addChild($$, $2);
        addChild($$, $4);
    }
    ;

enumerator_list:
    enumerator { $$ = $1; }
    | enumerator_list COMMA enumerator { $$ = $1; addChild($$, $3); }
    ;

enumerator:
    IDENTIFIER {
        if ($1 && $1->value) {
            insertVariable($1->value, "enum_constant", 0, NULL, 0, 0);
        }
        $$ = $1;
    }
    | IDENTIFIER ASSIGN constant_expression {
        if ($1 && $1->value) {
            insertVariable($1->value, "enum_constant", 0, NULL, 0, 0);
        }
        $$ = $1;
        addChild($$, $3);
    }
    ;

init_declarator_list:
    init_declarator {
        $$ = $1;
    }
    | init_declarator_list COMMA init_declarator {
        $$ = $1;
        if ($3) addChild($$, $3);
    }
    ;

init_declarator:
    declarator {
        if ($1 && $1->value) {
            if (in_typedef) {
                // Insert the symbol and then manually set its kind to "typedef".
                insertVariable($1->value, currentType, 0, NULL, 0, 0);
                if (symCount > 0 && strcmp(symtab[symCount - 1].name, $1->value) == 0) {
                    strcpy(symtab[symCount - 1].kind, "typedef");
                }
            } else {
                insertVariable($1->value, currentType, 0, NULL, 0, 0);
            }
        }
        $$ = $1;
    }
    | declarator ASSIGN initializer {
        if ($1 && $1->value) {
            if (!in_typedef) {
                insertVariable($1->value, currentType, 0, NULL, 0, 0);
            }
            /* emit("ASSIGN", ...) REMOVED */
        }
        
        // Create an AST node for the initialization
        $$ = createNode(NODE_INITIALIZER, "=");
        addChild($$, $1); // The declarator (ID)
        addChild($$, $3); // The initializer (expression)
        $$->dataType = $1->dataType;
    }
    ;

initializer:
    assignment_expression { $$ = $1; }
    | LBRACE initializer_list RBRACE { $$ = $2; }
    | LBRACE initializer_list COMMA RBRACE { $$ = $2; }
    ;

initializer_list:
    initializer {
        $$ = createNode(NODE_INITIALIZER, "init_list");
        addChild($$, $1);
    }
    | initializer_list COMMA initializer {
        $$ = $1;
        addChild($$, $3);
    }
    ;

declarator:
    direct_declarator { $$ = $1; }
    | MULTIPLY declarator {
        $$ = createNode(NODE_POINTER, "*");
        addChild($$, $2);
        /* TODO: This should update the pointer level for symbol table */
    }
    | MULTIPLY type_qualifier_list declarator {
        $$ = createNode(NODE_POINTER, "*");
        addChild($$, $2); // type_qualifier_list
        addChild($$, $3); // declarator
        /* TODO: This should update the pointer level for symbol table */
    }
    ;

direct_declarator:
    IDENTIFIER { $$ = $1; }
    | LPAREN declarator RPAREN { $$ = $2; }
    | direct_declarator LBRACKET RBRACKET { $$ = $1; }
    | direct_declarator LBRACKET constant_expression RBRACKET { $$ = $1; }
    | direct_declarator LPAREN RPAREN {
        $$ = $1;
        enterScope();
    }
    | direct_declarator LPAREN parameter_type_list RPAREN {
        $$ = $1;
        enterScope();
    }
    ;

type_qualifier_list:
    type_qualifier { $$ = $1; }
    | type_qualifier_list type_qualifier { $$ = $1; addChild($$, $2); }
    ;

parameter_type_list:
    parameter_list { $$ = $1; }
    ;

parameter_list:
    parameter_declaration {
        $$ = createNode(NODE_PARAMETER_LIST, "params");
        addChild($$, $1);
    }
    | parameter_list COMMA parameter_declaration {
        $$ = $1;
        addChild($$, $3);
    }
    ;

parameter_declaration:
    declaration_specifiers declarator {
        if ($2 && $2->value) {
            insertVariable($2->value, currentType, 0, NULL, 0, 0);
        }
        $$ = createNode(NODE_PARAMETER_DECLARATION, "param");
        addChild($$, $1);
        addChild($$, $2);
    }
    | declaration_specifiers abstract_declarator {
        $$ = createNode(NODE_PARAMETER_DECLARATION, "param");
        addChild($$, $1);
        addChild($$, $2);
    }
    | declaration_specifiers { $$ = $1; }
    ;

abstract_declarator:
    MULTIPLY {
        $$ = createNode(NODE_POINTER, "*");
        enterScope();
    }
    | direct_abstract_declarator { $$ = $1; }
    | MULTIPLY type_qualifier_list { $$ = createNode(NODE_POINTER, "*"); addChild($$, $2); }
    | MULTIPLY direct_abstract_declarator { $$ = createNode(NODE_POINTER, "*"); addChild($$, $2); }
    | MULTIPLY type_qualifier_list direct_abstract_declarator { $$ = createNode(NODE_POINTER, "*"); addChild($$, $2); addChild($$, $3); }
    ;

direct_abstract_declarator:
    LPAREN abstract_declarator RPAREN { $$ = $2; }
    | LBRACKET RBRACKET { $$ = createNode(NODE_DIRECT_DECLARATOR, "[]"); }
    | LBRACKET constant_expression RBRACKET { $$ = createNode(NODE_DIRECT_DECLARATOR, "[]"); addChild($$, $2); }
    | direct_abstract_declarator LBRACKET RBRACKET { $$ = $1; }
    | direct_abstract_declarator LBRACKET constant_expression RBRACKET { $$ = $1; addChild($$, $3); }
    | LPAREN RPAREN { $$ = createNode(NODE_DIRECT_DECLARATOR, "()"); }
    | LPAREN parameter_type_list RPAREN { $$ = createNode(NODE_DIRECT_DECLARATOR, "()"); addChild($$, $2); }
    | direct_abstract_declarator LPAREN RPAREN { $$ = $1; }
    | direct_abstract_declarator LPAREN parameter_type_list RPAREN { $$ = $1; addChild($$, $3); }
    ;

statement:
    labeled_statement { $$ = $1; }
    | compound_statement { $$ = $1; }
    | expression_statement { $$ = $1; }
    | selection_statement { $$ = $1; }
    | iteration_statement { $$ = $1; }
    | jump_statement { $$ = $1; }
    ;

labeled_statement:
    IDENTIFIER COLON statement {
        if ($1 && $1->value) {
            /* emit("LABEL", ...) REMOVED */
            insertSymbol($1->value, "label", 0);
        }
        $$ = createNode(NODE_LABELED_STATEMENT, "label");
        addChild($$, $1);
        addChild($$, $3);
    }
    | CASE constant_expression COLON statement {
        /* emit("LABEL", ...) REMOVED */
        $$ = createNode(NODE_LABELED_STATEMENT, "case");
        addChild($$, $2);
        addChild($$, $4);
    }
    | DEFAULT COLON statement {
        /* emit("LABEL", ...) REMOVED */
        $$ = createNode(NODE_LABELED_STATEMENT, "default");
        addChild($$, $3);
    }
    ;

compound_statement:
    LBRACE { enterScope(); } RBRACE {
        $$ = createNode(NODE_COMPOUND_STATEMENT, "compound");
        exitScope();
        recovering_from_error = 0;
    }
    | LBRACE { enterScope(); } block_item_list RBRACE {
        $$ = $3; // Pass up the block_item_list node
        exitScope();
        recovering_from_error = 0;
    }
    ;

block_item_list:
    block_item {
        $$ = createNode(NODE_COMPOUND_STATEMENT, "block_list");
        addChild($$, $1);
    }
    | block_item_list block_item {
        $$ = $1;
        if ($2) addChild($$, $2);
    }
    ;

block_item:
    declaration { $$ = $1; }
    | statement { $$ = $1; }
    ;

expression_statement:
    SEMICOLON {
        $$ = createNode(NODE_EXPRESSION_STATEMENT, ";");
        recovering_from_error = 0;
    }
    | expression SEMICOLON {
        $$ = $1;
        recovering_from_error = 0;
    }
    ;

if_header:
    IF LPAREN expression RPAREN {
        $$ = $3; // Pass the expression node up
        /* All label and emit logic is REMOVED */
    }
    ;

marker_while_start: %empty {
    $$ = createNode(NODE_MARKER, "while_start");
    /* All label, stack, and emit logic REMOVED */
};

for_label_marker: %empty {
    $$ = createNode(NODE_MARKER, "for_labels");
    /* All label, stack, and emit logic REMOVED */
};

selection_statement:
    if_header statement %prec LOWER_THAN_ELSE {
        /* emit("LABEL", ...) REMOVED */
        $$ = createNode(NODE_SELECTION_STATEMENT, "if");
        addChild($$, $1); // The expression from if_header
        addChild($$, $2); // The 'then' statement
        recovering_from_error = 0;
    }
    | if_header statement ELSE {
        /* All emit logic from this MRA is REMOVED */
      }
      statement {
        /* emit("LABEL", ...) REMOVED */
        $$ = createNode(NODE_SELECTION_STATEMENT, "if_else");
        addChild($$, $1); // The expression from if_header
        addChild($$, $2); // The 'then' statement
        addChild($$, $5); // The 'else' statement
        recovering_from_error = 0;
      }
    | SWITCH LPAREN expression RPAREN statement {
        /* All label, stack, and emit logic REMOVED */
        $$ = createNode(NODE_SELECTION_STATEMENT, "switch");
        addChild($$, $3); // The expression
        addChild($$, $5); // The statement
        recovering_from_error = 0;
    }
    ;

/* == Helper rules for do-while/until == */
do_header:
    DO {
        $$ = createNode(NODE_MARKER, "do_header");
        /* All label, stack, and emit logic REMOVED */
    }
    ;

do_trailer:
    WHILE LPAREN expression RPAREN SEMICOLON {
        /* emit("IF_TRUE_GOTO", ...) REMOVED */
        $$ = createNode(NODE_ITERATION_STATEMENT, "do_while");
        addChild($$, $3); // expression
    }
    | UNTIL LPAREN expression RPAREN SEMICOLON {
        /* emit("IF_FALSE_GOTO", ...) REMOVED */
        $$ = createNode(NODE_ITERATION_STATEMENT, "do_until");
        addChild($$, $3); // expression
    }
    ;

iteration_statement:
    WHILE LPAREN marker_while_start expression RPAREN
    {
        /* emit("IF_FALSE_GOTO", ...) REMOVED */
    }
    statement
    {
        /* All emit and popLoopLabels logic REMOVED */
        $$ = createNode(NODE_ITERATION_STATEMENT, "while");
        addChild($$, $4); // expression
        addChild($$, $7); // statement
        addChild($$, $3); // Add the marker node
        recovering_from_error = 0;
    }
    | do_header statement do_trailer {
        /* All emit and popLoopLabels logic REMOVED */
        $$ = $3; // The node (do_while/do_until) created in do_trailer
        addChild($$, $1); // do_header marker
        addChild($$, $2); // statement
        recovering_from_error = 0;
    }
    | FOR LPAREN
        expression_statement                  // $3: init
        for_label_marker                      // $4: marker
        expression_statement                  // $5: cond
        { 
            /* emit("IF_FALSE_GOTO", ...) REMOVED */
        }
        expression                            // $7: incr
        RPAREN                                // $8
        statement                             // $9: body
        {
            /* All emit and popLoopLabels logic REMOVED */
            $$ = createNode(NODE_ITERATION_STATEMENT, "for");
            addChild($$, $3);  // init
            addChild($$, $5);  // cond
            addChild($$, $7);  // incr
            addChild($$, $9);  // statement body
            addChild($$, $4);  // Add the marker node
            recovering_from_error = 0;
        }
    ;

jump_statement:
    GOTO IDENTIFIER SEMICOLON {
        /* emit("GOTO", ...) REMOVED */
        $$ = createNode(NODE_JUMP_STATEMENT, "goto");
        addChild($$, $2);
    }
    | CONTINUE SEMICOLON {
        /* All label logic and emit REMOVED */
        $$ = createNode(NODE_JUMP_STATEMENT, "continue");
    }
    | BREAK SEMICOLON {
        /* All label logic and emit REMOVED */
        $$ = createNode(NODE_JUMP_STATEMENT, "break");
    }
    | RETURN SEMICOLON {
        /* emit("RETURN", ...) REMOVED */
        $$ = createNode(NODE_JUMP_STATEMENT, "return");
    }
    | RETURN expression SEMICOLON {
        /* emit("RETURN", ...) REMOVED */
        $$ = createNode(NODE_JUMP_STATEMENT, "return");
        addChild($$, $2);
    }
    ;

expression:
    assignment_expression {
        $$ = $1;
    }
    | expression COMMA assignment_expression {
        $$ = createNode(NODE_EXPRESSION, ",");
        addChild($$, $1);
        addChild($$, $3);
        /* $$->tacResult = ... REMOVED */
    }
    ;

assignment_expression:
    conditional_expression {
        $$ = $1;
    }
    | unary_expression ASSIGN assignment_expression {
        /* 1. Semantic Checks */
        if (!$1->isLValue) {
            yyerror("Left side of assignment must be an lvalue");
        }
        
        if ($1->dataType && $3->dataType && !isAssignable($1->dataType, $3->dataType)) {
            char err_msg[256];
            sprintf(err_msg, "Type mismatch in assignment: cannot assign %s to %s",
                    $3->dataType, $1->dataType);
            yyerror(err_msg);
        }
        
        /* 2. Build Node */
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "=");
        addChild($$, $1);
        addChild($$, $3);

        /* 3. Store Type */
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
        
        /* All emit, convertType, and tacResult logic REMOVED */
    }
    | unary_expression PLUS_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "+=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression MINUS_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "-=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression MUL_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "*=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression DIV_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "/=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression MOD_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "%=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression AND_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "&=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression OR_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "|=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression XOR_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "^=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression LSHIFT_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "<<=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression RSHIFT_ASSIGN assignment_expression {
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, ">>=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    ;

conditional_expression:
    logical_or_expression {
        $$ = $1;
    }
    | logical_or_expression QUESTION expression COLON conditional_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_CONDITIONAL_EXPRESSION, "?:");
        addChild($$, $1);
        addChild($$, $3);
        addChild($$, $5);
        $$->dataType = $3->dataType ? strdup($3->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    ;

constant_expression:
    conditional_expression {
        $$ = $1;
    }
    ;

logical_or_expression:
    logical_and_expression {
        $$ = $1;
    }
    | logical_or_expression LOGICAL_OR logical_and_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_LOGICAL_OR_EXPRESSION, "||");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

logical_and_expression:
    inclusive_or_expression {
        $$ = $1;
    }
    | logical_and_expression LOGICAL_AND inclusive_or_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_LOGICAL_AND_EXPRESSION, "&&");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

inclusive_or_expression:
    exclusive_or_expression {
        $$ = $1;
    }
    | inclusive_or_expression BITWISE_OR exclusive_or_expression {
        if ($1->dataType && !isArithmeticType($1->dataType)) {
            yyerror("Bitwise OR requires arithmetic types");
        }
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_INCLUSIVE_OR_EXPRESSION, "|");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

exclusive_or_expression:
    and_expression {
        $$ = $1;
    }
    | exclusive_or_expression BITWISE_XOR and_expression {
        if ($1->dataType && !isArithmeticType($1->dataType)) {
            yyerror("Bitwise XOR requires arithmetic types");
        }
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_EXCLUSIVE_OR_EXPRESSION, "^");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

and_expression:
    equality_expression {
        $$ = $1;
    }
    | and_expression BITWISE_AND equality_expression {
        if ($1->dataType && !isArithmeticType($1->dataType)) {
            yyerror("Bitwise AND requires arithmetic types");
        }
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_AND_EXPRESSION, "&");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

equality_expression:
    relational_expression {
        $$ = $1;
    }
    | equality_expression EQ relational_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_EQUALITY_EXPRESSION, "==");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | equality_expression NE relational_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_EQUALITY_EXPRESSION, "!=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

relational_expression:
    shift_expression {
        $$ = $1;
    }
    | relational_expression LT shift_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, "<");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | relational_expression GT shift_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, ">");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | relational_expression LE shift_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, "<=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | relational_expression GE shift_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, ">=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

shift_expression:
    additive_expression {
        $$ = $1;
    }
    | shift_expression LSHIFT additive_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_SHIFT_EXPRESSION, "<<");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | shift_expression RSHIFT additive_expression {
        /* emit(...) and newTemp() REMOVED */
        $$ = createNode(NODE_SHIFT_EXPRESSION, ">>");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

additive_expression:
    multiplicative_expression {
        $$ = $1;
    }
    | additive_expression PLUS multiplicative_expression {
        if ($1->dataType && $3->dataType &&
            !isArithmeticType($1->dataType) && !isArithmeticType($3->dataType)) {
            yyerror("Addition requires arithmetic types");
        }
        
        char* result_type = NULL;
        if ($1->dataType && $3->dataType) {
            result_type = usualArithConv($1->dataType, $3->dataType);
        }
        
        /* All convertType, newTemp, emit, tacResult logic REMOVED */
        
        $$ = createNode(NODE_ADDITIVE_EXPRESSION, "+");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type ? strdup(result_type) : strdup("int");
    }
    | additive_expression MINUS multiplicative_expression {
        if ($1->dataType && $3->dataType &&
            !isArithmeticType($1->dataType) && !isArithmeticType($3->dataType)) {
            yyerror("Subtraction requires arithmetic types");
        }
        
        char* result_type = NULL;
        if ($1->dataType && $3->dataType) {
            result_type = usualArithConv($1->dataType, $3->dataType);
        }
        
        /* All convertType, newTemp, emit, tacResult logic REMOVED */

        $$ = createNode(NODE_ADDITIVE_EXPRESSION, "-");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type ? strdup(result_type) : strdup("int");
    }
    ;

multiplicative_expression:
    cast_expression {
        $$ = $1;
    }
    | multiplicative_expression MULTIPLY cast_expression {
        if ($1->dataType && $3->dataType &&
            !isArithmeticType($1->dataType) && !isArithmeticType($3->dataType)) {
            yyerror("Multiplication requires arithmetic types");
        }
        
        char* result_type = NULL;
        if ($1->dataType && $3->dataType) {
            result_type = usualArithConv($1->dataType, $3->dataType);
        }
        
        /* All convertType, newTemp, emit, tacResult logic REMOVED */

        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "*");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type ? strdup(result_type) : strdup("int");
    }
    | multiplicative_expression DIVIDE cast_expression {
        if ($1->dataType && $3->dataType &&
            !isArithmeticType($1->dataType) && !isArithmeticType($3->dataType)) {
            yyerror("Division requires arithmetic types");
        }
        
        char* result_type = NULL;
        if ($1->dataType && $3->dataType) {
            result_type = usualArithConv($1->dataType, $3->dataType);
        }
        
        /* All convertType, newTemp, emit, tacResult logic REMOVED */
        
        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "/");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type ? strdup(result_type) : strdup("int");
    }
    | multiplicative_expression MODULO cast_expression {
        if ($1->dataType && $3->dataType &&
            !isArithmeticType($1->dataType) && !isArithmeticType($3->dataType)) {
            yyerror("Modulo requires arithmetic types");
        }
        
        /* All newTemp, emit, tacResult logic REMOVED */
        
        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "%");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int");
    }
    ;

cast_expression:
    unary_expression {
        $$ = $1;
    }
    | LPAREN type_name RPAREN cast_expression {
        $$ = createNode(NODE_CAST_EXPRESSION, "cast");
        addChild($$, $2); // The type_name
        addChild($$, $4); // The expression being cast
        $$->dataType = strdup(currentType); // Type was set when $2 was parsed
    }
    ;

type_name:
    declaration_specifiers {
        $$ = createNode(NODE_TYPE_NAME, "type_name");
        addChild($$, $1);
    }
    | declaration_specifiers abstract_declarator {
        $$ = createNode(NODE_TYPE_NAME, "type_name");
        addChild($$, $1);
        addChild($$, $2);
    }
    ;

unary_expression:
    postfix_expression {
        $$ = $1;
    }
    | INCREMENT unary_expression {
        if (!$2->isLValue) {
            yyerror("Increment requires lvalue");
        }
        /* newTemp(), emit("ADD"), emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "++_pre");
        addChild($$, $2);
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    | DECREMENT unary_expression {
        if (!$2->isLValue) {
            yyerror("Decrement requires lvalue");
        }
        /* newTemp(), emit("SUB"), emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "--_pre");
        addChild($$, $2);
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    | BITWISE_AND cast_expression {
        /* newTemp(), emit("ADDR") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "&");
        addChild($$, $2);
        // TODO: A better type system would create "pointer to $2->dataType"
        $$->dataType = strdup("int*");
        /* $$->tacResult = ... REMOVED */
    }
    | MULTIPLY cast_expression {
        /* newTemp(), emit("DEREF") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "*");
        addChild($$, $2);
        // TODO: A better type system would find "base type of $2->dataType"
        $$->dataType = strdup("int");
        $$->isLValue = 1;
        /* $$->tacResult = ... REMOVED */
    }
    | PLUS cast_expression {
        $$ = $2;
    }
    | MINUS cast_expression {
        /* newTemp(), emit("NEG") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "-_unary");
        addChild($$, $2);
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    | BITWISE_NOT cast_expression {
        /* newTemp(), emit("BITNOT") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "~");
        addChild($$, $2);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | LOGICAL_NOT cast_expression {
        /* newTemp(), emit("NOT") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "!");
        addChild($$, $2);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | SIZEOF unary_expression {
        /* newTemp(), sprintf, emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "sizeof");
        addChild($$, $2);
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | SIZEOF LPAREN type_name RPAREN {
        /* newTemp(), emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "sizeof");
        addChild($$, $3); // Add the type_name node
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    ;

postfix_expression:
    primary_expression {
        $$ = $1;
    }
    | postfix_expression LBRACKET expression RBRACKET {
        /* newTemp(), emit("ARRAY_ACCESS") REMOVED */
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "[]");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL; // Simplistic
        $$->isLValue = 1;
        /* $$->tacResult = ... REMOVED */
    }
    | postfix_expression LPAREN RPAREN {
        /* == Check for undeclared function and implicitly declare it == */
        Symbol* sym = lookupSymbol($1->value);
        if (!sym) {
            insertFunction($1->value, "int", 0, NULL, NULL);
        } else if (!sym->is_function) {
            char err_msg[256];
            sprintf(err_msg, "Called object '%s' is not a function", $1->value);
            yyerror(err_msg);
        }

        /* newTemp(), emit("CALL") REMOVED */
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "()");
        addChild($$, $1);
        $$->dataType = strdup("int"); // Should be sym->return_type
        /* $$->tacResult = ... REMOVED */
    }
    | postfix_expression LPAREN argument_expression_list RPAREN {
        /* == Check for undeclared function and implicitly declare it == */
        Symbol* sym = lookupSymbol($1->value);
        if (!sym) {
            insertFunction($1->value, "int", 0, NULL, NULL);
        } else if (!sym->is_function) {
            char err_msg[256];
            sprintf(err_msg, "Called object '%s' is not a function", $1->value);
            yyerror(err_msg);
        }
        
        /* newTemp(), emit("CALL") REMOVED */
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "()");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int"); // Should be sym->return_type
        /* $$->tacResult = ... REMOVED */
    }
    | postfix_expression DOT IDENTIFIER {
        /* newTemp(), emit("MEMBER_ACCESS") REMOVED */
        $$ = createNode(NODE_POSTFIX_EXPRESSION, ".");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int"); // Simplistic
        $$->isLValue = 1;
        /* $$->tacResult = ... REMOVED */
    }
    | postfix_expression ARROW IDENTIFIER {
        /* newTemp(), emit("ARROW_ACCESS") REMOVED */
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "->");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = strdup("int"); // Simplistic
        $$->isLValue = 1;
        /* $$->tacResult = ... REMOVED */
    }
    | postfix_expression INCREMENT {
        if (!$1->isLValue) {
            yyerror("Post-increment requires lvalue");
        }
        /* newTemp(), emit("ASSIGN"), emit("ADD"), emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "++_post");
        addChild($$, $1);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    | postfix_expression DECREMENT {
        if (!$1->isLValue) {
            yyerror("Post-decrement requires lvalue");
        }
        /* newTemp(), emit("ASSIGN"), emit("SUB"), emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "--_post");
        addChild($$, $1);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    ;

primary_expression:
    IDENTIFIER {
        $$ = $1;
        if ($1 && $1->value) {
            Symbol* sym = lookupSymbol($1->value);
            if (sym) {
                $$->dataType = strdup(sym->is_function ? sym->return_type : sym->type);
                $$->isLValue = !sym->is_function;
            } else {
                // Potentially undeclared. Don't error yet.
                $$->dataType = strdup("int"); // Assume int
                $$->isLValue = 1; // Assume lvalue
            }
            /* $$->tacResult = ... REMOVED */
        }
    }
    | INTEGER_CONSTANT {
        $$ = $1;
        /* newTemp(), emit() REMOVED */
        $$->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
    | HEX_CONSTANT {
        $$ = $1;
        $$->dataType = strdup("int");
    }
    | OCTAL_CONSTANT {
        $$ = $1;
        $$->dataType = strdup("int");
    }
    | BINARY_CONSTANT {
        $$ = $1;
        $$->dataType = strdup("int");
    }
    | FLOAT_CONSTANT {
        $$ = $1;
        $$->dataType = strdup("float");
    }
    | CHAR_CONSTANT {
        $$ = $1;
        $$->dataType = strdup("char");
    }
    | STRING_LITERAL {
        $$ = $1;
        $$->dataType = strdup("char*");
    }
    | LPAREN expression RPAREN {
        $$ = $2;
    }
    ;

argument_expression_list:
    assignment_expression {
        /* emit("ARG", ...) REMOVED */
        $$ = createNode(NODE_EXPRESSION, "args");
        addChild($$, $1);
    }
    | argument_expression_list COMMA assignment_expression {
        /* emit("ARG", ...) REMOVED */
        $$ = $1;
        addChild($$, $3);
    }
    ;

%%

/* ========================================================== */
/* PART 5: C Function Implementations                         */
/* ========================================================== */

/*
 * All other functions (createNode, addChild, symbol table functions,
 * IR functions, main, etc.) have been moved to:
 * - ast.c
 * - symbol_table.c
 * - ir_context.c
 * - ir_generator.c
 * - main.c
 *
 * Only yyerror() remains, as it is tightly coupled with the parser.
 */

void yyerror(const char* msg) {
    if (!recovering_from_error) {
        fprintf(stderr, "Syntax Error on line %d: %s at '%s'\n", yylineno, msg, yytext);
        error_count++;
        recovering_from_error = 1;
    }
}
