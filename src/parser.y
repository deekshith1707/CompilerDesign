/* ========================================================== */
/* PART 1: C Declarations and Includes (for parser.tab.c)     */
/* ========================================================== */
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations of types defined in the %code requires block
typedef enum NodeType NodeType;
typedef struct TreeNode TreeNode;
typedef struct Symbol Symbol;

/* == COMPLETE Symbol STRUCTURE DEFINITION == */
struct Symbol {
    char name[128];
    char type[128];
    char kind[32];
    int scope_level;
    int offset;
    int size;
    int is_array;
    int array_dims[10];
    int num_dims;
    int ptr_level;
    int is_function;
    char return_type[128];
    int param_count;
    char param_types[16][128];
    char param_names[16][128];
};

/* == FIX: ADDED COMPLETE LIST OF FORWARD DECLARATIONS FOR ALL HELPER FUNCTIONS == */
// This makes them visible to the grammar rule actions that follow.
extern int yylex();
void yyerror(const char* msg);

// Node and Tree Functions
TreeNode* createNode(NodeType type, const char* value);
void addChild(TreeNode* parent, TreeNode* child);

// IR Infrastructure Functions
void emit(const char* op, const char* arg1, const char* arg2, const char* result);
char* newTemp();
char* newLabel();

// Control Flow Stack Functions
void pushLoopLabels(char* continue_label, char* break_label);
void popLoopLabels();
char* getCurrentLoopContinue();
char* getCurrentLoopBreak();
void pushSwitchLabel(char* end_label);
void popSwitchLabel();
char* getCurrentSwitchEnd();

// Symbol Table Functions
void insertVariable(const char* name, const char* type, int is_array, int* dims, int num_dims, int ptr_level);
void insertFunction(const char* name, const char* ret_type, int param_count, char params[][128], char param_names[][128]);
Symbol* lookupSymbol(const char* name);
void enterScope();
void exitScope();
void insertSymbol(const char* name, const char* type, int is_function);

// Type System Utilities
int getTypeSize(const char* type);
int isArithmeticType(const char* type);
char* usualArithConv(const char* t1, const char* t2);
char* convertType(char* place, const char* from_type, const char* to_type);
int isAssignable(const char* lhs_type, const char* rhs_type);
void setCurrentType(const char* type);

// Global variables used by the parser actions
extern int yylineno;
extern char* yytext;
extern FILE* yyin;
int error_count = 0;
int recovering_from_error = 0;
char currentType[128] = "int";
int in_typedef = 0;

/* == Symbol table global variables == */
#define MAX_SYMBOLS 2000
Symbol symtab[MAX_SYMBOLS];
int symCount = 0;
int current_scope = 0;
int current_offset = 0;

%}

/* =================================================================== */
/* PART 2: Code required by the lexer (placed in parser.tab.h)         */
/* =================================================================== */
%code requires {
    typedef enum NodeType {
        NODE_PROGRAM, NODE_FUNCTION_DEFINITION, NODE_DECLARATION,
        NODE_DECLARATION_SPECIFIERS, NODE_DECLARATOR, NODE_DIRECT_DECLARATOR,
        NODE_POINTER, NODE_PARAMETER_LIST, NODE_PARAMETER_DECLARATION,
        NODE_COMPOUND_STATEMENT, NODE_STATEMENT, NODE_EXPRESSION_STATEMENT,
        NODE_SELECTION_STATEMENT, NODE_ITERATION_STATEMENT, NODE_JUMP_STATEMENT,
        NODE_LABELED_STATEMENT, NODE_EXPRESSION, NODE_ASSIGNMENT_EXPRESSION,
        
        NODE_CONDITIONAL_EXPRESSION, NODE_LOGICAL_OR_EXPRESSION,
        NODE_LOGICAL_AND_EXPRESSION, NODE_INCLUSIVE_OR_EXPRESSION,
        NODE_EXCLUSIVE_OR_EXPRESSION, NODE_AND_EXPRESSION, NODE_EQUALITY_EXPRESSION,
        NODE_RELATIONAL_EXPRESSION, NODE_SHIFT_EXPRESSION, NODE_ADDITIVE_EXPRESSION,
        NODE_MULTIPLICATIVE_EXPRESSION, NODE_CAST_EXPRESSION, NODE_UNARY_EXPRESSION,
        NODE_POSTFIX_EXPRESSION, NODE_PRIMARY_EXPRESSION, NODE_IDENTIFIER,
        NODE_CONSTANT, NODE_STRING_LITERAL, NODE_TYPE_SPECIFIER,
        NODE_STORAGE_CLASS_SPECIFIER, NODE_TYPE_QUALIFIER, NODE_STRUCT_SPECIFIER,
        NODE_ENUM_SPECIFIER, NODE_INITIALIZER, NODE_MARKER, NODE_TYPE_NAME
    } NodeType;

    typedef struct TreeNode {
        NodeType type;
        char* value;
        char* dataType;
        int isLValue;
        char* tacResult;
        char* trueLabel;
        char* falseLabel;
        char* nextLabel;
        char* startLabel;
        char* endLabel;
        char* incrLabel;
        struct TreeNode** children;
        int childCount;
        int childCapacity;
        int lineNumber;
    } TreeNode;

    // Forward declaration only for Symbol
    typedef struct Symbol Symbol;

    /* Prototypes for functions CALLED BY THE LEXER go here */
    TreeNode* createNode(NodeType type, const char* value);
    int is_type_name(const char* name);
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

/* == FIX: Added new types for the unambiguous do-while/until grammar == */
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
        printf("\n=== PARSING COMPLETED SUCCESSFULLY ===\n");
    }
    ;

translation_unit:
    external_declaration {
        $$ = $1;
    }
    | translation_unit external_declaration {
        $$ = $1;
        if ($2) addChild($$, $2);
    }
    ;

external_declaration:
    function_definition { $$ = $1; }
    | declaration { $$ = $1; }
    | preprocessor_directive { $$ = NULL; } /* Can be NULL since it's not used */
    ;

preprocessor_directive:
    PREPROCESSOR { $$ = NULL; }
    ;

function_definition:
    declaration_specifiers declarator
    {
        if ($2 && $2->value) {
            char func_label[140];
            sprintf(func_label, "FUNC_%s", $2->value);
            emit("LABEL", func_label, "", "");
            emit("FUNC_BEGIN", $2->value, "", "");
            insertSymbol($2->value, currentType, 1);
        }
    }
    compound_statement
    {
        $$ = createNode(NODE_FUNCTION_DEFINITION, "function");
        addChild($$, $1);
        addChild($$, $2);
        addChild($$, $4);
        
        if ($2 && $2->value) {
            emit("FUNC_END", $2->value, "", "");
        }
        exitScope();
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
        addChild($$, $2); /* The IDENTIFIER (e.g., Point) */
        addChild($$, $4); /* The struct_declaration_list */
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
        addChild($$, $2); /* The IDENTIFIER */
        addChild($$, $4); /* The struct_declaration_list */
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
                if ($3 && $3->tacResult) {
                    emit("ASSIGN", $3->tacResult, "", $1->value);
                }
            }
        }
        $$ = $1;
        if ($3) addChild($$, $3);
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
    direct_declarator {
        $$ = $1;
    }
    | MULTIPLY declarator {
        $$ = $2;
    }
    | MULTIPLY type_qualifier_list declarator {
        $$ = $3;
    }
    ;

direct_declarator:
    IDENTIFIER {
        $$ = $1;
    }
    | LPAREN declarator RPAREN {
        $$ = $2;
    }
    | direct_declarator LBRACKET RBRACKET {
        $$ = $1;
    }
    | direct_declarator LBRACKET constant_expression RBRACKET {
        $$ = $1;
    }
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
    | declaration_specifiers {
        $$ = $1;
    }
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
    | LBRACKET constant_expression RBRACKET { $$ = createNode(NODE_DIRECT_DECLARATOR, "[]");
        addChild($$, $2); }
    | direct_abstract_declarator LBRACKET RBRACKET { $$ = $1; }
    | direct_abstract_declarator LBRACKET constant_expression RBRACKET { $$ = $1; addChild($$, $3); }
    | LPAREN RPAREN { $$ = createNode(NODE_DIRECT_DECLARATOR, "()"); }
    | LPAREN parameter_type_list RPAREN { $$ = createNode(NODE_DIRECT_DECLARATOR, "()");
        addChild($$, $2); }
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
            emit("LABEL", $1->value, "", "");
            insertSymbol($1->value, "label", 0);
        }
        $$ = createNode(NODE_LABELED_STATEMENT, "label");
        addChild($$, $1);
        addChild($$, $3);
    }
    | CASE constant_expression COLON statement {
        if ($2 && $2->tacResult) {
            char case_label[128];
            sprintf(case_label, "CASE_%s", $2->tacResult);
            emit("LABEL", case_label, "", "");
        }
        $$ = createNode(NODE_LABELED_STATEMENT, "case");
        addChild($$, $2);
        addChild($$, $4);
    }
    | DEFAULT COLON statement {
        emit("LABEL", "DEFAULT", "", "");
        $$ = createNode(NODE_LABELED_STATEMENT, "default");
        addChild($$, $3);
    }
    ;

compound_statement:
    LBRACE {
        enterScope();
    } RBRACE {
        $$ = createNode(NODE_COMPOUND_STATEMENT, "compound");
        exitScope();
        recovering_from_error = 0;
    }
    | LBRACE {
        enterScope();
    } block_item_list RBRACE {
        $$ = $3;
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
        $$ = createNode(NODE_MARKER, "if_header");
        $$->startLabel = newLabel(); // This is the else/end label
        
        if ($3 && $3->tacResult) {
            emit("IF_FALSE_GOTO", $3->tacResult, $$->startLabel, "");
        }
    }
    ;

marker_while_start: %empty {
    $$ = createNode(NODE_MARKER, "while_start");
    $$->startLabel = newLabel(); // Loop condition check
    $$->endLabel = newLabel();   // After loop
    pushLoopLabels($$->startLabel, $$->endLabel);
    emit("LABEL", $$->startLabel, "", "");
};

for_label_marker: %empty
    {
        $$ = createNode(NODE_MARKER, "for_labels");
        $$->startLabel = newLabel(); // This is the condition label (L_cond)
        $$->incrLabel  = newLabel(); // This is the increment label (L_incr)
        $$->endLabel   = newLabel(); // This is the end label (L_end)
        emit("LABEL", $$->startLabel, "", "");
        pushLoopLabels($$->incrLabel, $$->endLabel); // continue -> L_incr, break -> L_end
    }
;

selection_statement:
    if_header statement %prec LOWER_THAN_ELSE {
        emit("LABEL", $1->startLabel, "", "");
        $$ = createNode(NODE_SELECTION_STATEMENT, "if");
        addChild($$, $1);
        addChild($$, $2);
        recovering_from_error = 0;
    }
    | if_header statement ELSE {
        char* end_label = newLabel();
        emit("GOTO", end_label, "", "");
        emit("LABEL", $1->startLabel, "", "");
        $1->endLabel = end_label;
      }
      statement {
        emit("LABEL", $1->endLabel, "", "");
        $$ = createNode(NODE_SELECTION_STATEMENT, "if_else");
        addChild($$, $1);
        addChild($$, $2);
        addChild($$, $5);
        recovering_from_error = 0;
      }
    | SWITCH LPAREN expression RPAREN statement {
        char* switch_end = newLabel();
        pushSwitchLabel(switch_end);
        if ($3 && $3->tacResult) {
            emit("SWITCH", $3->tacResult, "", "");
        }
        // Statements inside will handle cases
        // The final break will jump here
        emit("LABEL", switch_end, "", "");
        popSwitchLabel();
        
        $$ = createNode(NODE_SELECTION_STATEMENT, "switch");
        addChild($$, $3);
        addChild($$, $5);
        recovering_from_error = 0;
    }
    ;
    
/* == FIX: Refactored do-while and do-until to be unambiguous == */

// Helper rule for the common part of do-while/until loops
do_header:
    DO {
        $$ = createNode(NODE_MARKER, "do_header");
        $$->startLabel = newLabel(); // The label for the start of the loop body
        $$->endLabel   = newLabel(); // The label for breaking out of the loop
        pushLoopLabels($$->startLabel, $$->endLabel);
        emit("LABEL", $$->startLabel, "", "");
    }
    ;

// Helper rule for the distinct trailers of do-while/until
do_trailer:
    WHILE LPAREN expression RPAREN SEMICOLON {
        if ($3 && $3->tacResult) {
            // If condition is true, jump back to the start of the loop body
            emit("IF_TRUE_GOTO", $3->tacResult, getCurrentLoopContinue(), "");
        }
        $$ = createNode(NODE_ITERATION_STATEMENT, "do_while");
        addChild($$, $3); // expression
    }
    | UNTIL LPAREN expression RPAREN SEMICOLON {
        if ($3 && $3->tacResult) {
            // If the condition is FALSE, jump back to the start of the loop
            emit("IF_FALSE_GOTO", $3->tacResult, getCurrentLoopContinue(), "");
        }
        $$ = createNode(NODE_ITERATION_STATEMENT, "do_until");
        addChild($$, $3); // expression
    }
    ;

iteration_statement:
    WHILE LPAREN marker_while_start expression RPAREN
    {
        if ($4 && $4->tacResult) {
            emit("IF_FALSE_GOTO", $4->tacResult, $3->endLabel, "");
        }
    }
    statement
    {
        emit("GOTO", $3->startLabel, "", "");
        emit("LABEL", $3->endLabel, "", "");
        popLoopLabels();
        
        $$ = createNode(NODE_ITERATION_STATEMENT, "while");
        addChild($$, $4);
        addChild($$, $7);
        recovering_from_error = 0;
    }
    /* == FIX: Combined do-while and do-until into a single structure == */
    | do_header statement do_trailer {
        // This is the break label, emitted after the condition check
        emit("LABEL", $1->endLabel, "", "");
        popLoopLabels();
        
        $$ = $3; // The node was created in do_trailer
        addChild($$, $1); // do_header marker
        addChild($$, $2); // statement
        recovering_from_error = 0;
    }
    | FOR LPAREN
        expression_statement                  // $3: init
        for_label_marker                      // $4: marker for labels
        expression_statement                  // $5: cond
        {                                     // MRA for conditional jump
            // An empty condition in a for loop is treated as true.
            // Only emit a jump if a condition expression exists.
            if ($5 && $5->tacResult) {
                emit("IF_FALSE_GOTO", $5->tacResult, $4->endLabel, "");
            }
        }
        expression                            // $7: incr
        RPAREN                                // $8
        statement                             // $9: body
        {                                     // Final Action
            emit("LABEL", $4->incrLabel, "", "");
            // NOTE: The TAC for the increment expression ($7) was already emitted when it was parsed.
            // This label just marks the correct location for 'continue' statements to jump to.
            emit("GOTO", $4->startLabel, "", ""); // After increment, loop back to the condition check.
            emit("LABEL", $4->endLabel, "", "");  // 'break' statements will jump here.
            popLoopLabels();
            $$ = createNode(NODE_ITERATION_STATEMENT, "for");
            addChild($$, $3);  // init
            addChild($$, $5);  // cond
            addChild($$, $7);  // incr
            addChild($$, $9);  // statement body
            recovering_from_error = 0;
        }
    ;

jump_statement:
    GOTO IDENTIFIER SEMICOLON {
        if ($2 && $2->value) {
            emit("GOTO", $2->value, "", "");
        }
        $$ = createNode(NODE_JUMP_STATEMENT, "goto");
        addChild($$, $2);
    }
    | CONTINUE SEMICOLON {
        char* continue_label = getCurrentLoopContinue();
        if (continue_label) {
            emit("GOTO", continue_label, "", "");
        } else {
            yyerror("continue statement not in loop");
        }
        $$ = createNode(NODE_JUMP_STATEMENT, "continue");
    }
    | BREAK SEMICOLON {
        char* break_label = getCurrentLoopBreak();
        if (!break_label) {
            break_label = getCurrentSwitchEnd();
        }
        if (break_label) {
            emit("GOTO", break_label, "", "");
        } else {
            yyerror("break statement not in loop or switch");
        }
        $$ = createNode(NODE_JUMP_STATEMENT, "break");
    }
    | RETURN SEMICOLON {
        emit("RETURN", "", "", "");
        $$ = createNode(NODE_JUMP_STATEMENT, "return");
    }
    | RETURN expression SEMICOLON {
        if ($2 && $2->tacResult) {
            emit("RETURN", $2->tacResult, "", "");
        }
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
        if ($3 && $3->tacResult) {
            $$->tacResult = $3->tacResult;
        }
    }
    ;

assignment_expression:
    conditional_expression {
        $$ = $1;
    }
    | unary_expression ASSIGN assignment_expression {
        /* == FIX: Check for undeclared identifier on LHS of assignment == */
        Symbol* sym = lookupSymbol($1->tacResult);
        if (!sym) {
            char err_msg[256];
            sprintf(err_msg, "Undeclared identifier: %s", $1->tacResult);
            yyerror(err_msg);
        }
    
        if (!$1->isLValue) {
            yyerror("Left side of assignment must be an lvalue");
        }
        
        if ($1->dataType && $3->dataType && !isAssignable($1->dataType, $3->dataType)) {
            char err_msg[256];
            sprintf(err_msg, "Type mismatch in assignment: cannot assign %s to %s",
                    $3->dataType, $1->dataType);
            yyerror(err_msg);
        }
        
        char* rhs = $3->tacResult;
        if ($1->dataType && $3->dataType) {
            rhs = convertType($3->tacResult, $3->dataType, $1->dataType);
        }
        
        if ($1->tacResult && rhs) {
            emit("ASSIGN", rhs, "", $1->tacResult);
        }
        
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression PLUS_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("ADD", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "+=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression MINUS_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("SUB", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "-=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression MUL_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("MUL", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "*=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression DIV_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("DIV", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "/=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression MOD_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("MOD", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "%=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression AND_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("BITAND", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "&=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression OR_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("BITOR", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "|=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression XOR_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("BITXOR", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "^=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression LSHIFT_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("LSHIFT", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "<<=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    | unary_expression RSHIFT_ASSIGN assignment_expression {
        if ($1->tacResult && $3->tacResult) {
            emit("RSHIFT", $1->tacResult, $3->tacResult, $1->tacResult);
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, ">>=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = $1->tacResult;
    }
    ;

conditional_expression:
    logical_or_expression {
        $$ = $1;
    }
    | logical_or_expression QUESTION expression COLON conditional_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("TERNARY", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_CONDITIONAL_EXPRESSION, "?:");
        addChild($$, $1);
        addChild($$, $3);
        addChild($$, $5);
        $$->tacResult = temp;
        $$->dataType = $3->dataType ? strdup($3->dataType) : NULL;
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
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("OR", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_LOGICAL_OR_EXPRESSION, "||");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    ;

logical_and_expression:
    inclusive_or_expression {
        $$ = $1;
    }
    | logical_and_expression LOGICAL_AND inclusive_or_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("AND", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_LOGICAL_AND_EXPRESSION, "&&");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
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
        
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("BITOR", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_INCLUSIVE_OR_EXPRESSION, "|");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
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
        
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("BITXOR", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_EXCLUSIVE_OR_EXPRESSION, "^");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
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
        
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("BITAND", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_AND_EXPRESSION, "&");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    ;

equality_expression:
    relational_expression {
        $$ = $1;
    }
    | equality_expression EQ relational_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("EQ", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_EQUALITY_EXPRESSION, "==");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | equality_expression NE relational_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("NE", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_EQUALITY_EXPRESSION, "!=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    ;

relational_expression:
    shift_expression {
        $$ = $1;
    }
    | relational_expression LT shift_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("LT", $1->tacResult, $3->tacResult, temp);
        }
        /* == FIX: Corrected typo from RELational to RELATIONAL == */
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, "<");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | relational_expression GT shift_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("GT", $1->tacResult, $3->tacResult, temp);
        }
        /* == FIX: Corrected typo from RELational to RELATIONAL == */
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, ">");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | relational_expression LE shift_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("LE", $1->tacResult, $3->tacResult, temp);
        }
        /* == FIX: Corrected typo from RELational to RELATIONAL == */
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, "<=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | relational_expression GE shift_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("GE", $1->tacResult, $3->tacResult, temp);
        }
        /* == FIX: Corrected typo from RELational to RELATIONAL == */
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, ">=");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    ;

shift_expression:
    additive_expression {
        $$ = $1;
    }
    | shift_expression LSHIFT additive_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("LSHIFT", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_SHIFT_EXPRESSION, "<<");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | shift_expression RSHIFT additive_expression {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("RSHIFT", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_SHIFT_EXPRESSION, ">>");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
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
        char* op1 = $1->tacResult;
        char* op2 = $3->tacResult;
        
        if ($1->dataType && $3->dataType) {
            result_type = usualArithConv($1->dataType, $3->dataType);
            op1 = convertType($1->tacResult, $1->dataType, result_type);
            op2 = convertType($3->tacResult, $3->dataType, result_type);
        }
        
        char* temp = newTemp();
        if (op1 && op2) {
            emit("ADD", op1, op2, temp);
        }
        
        $$ = createNode(NODE_ADDITIVE_EXPRESSION, "+");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = result_type ? strdup(result_type) : strdup("int");
    }
    | additive_expression MINUS multiplicative_expression {
        if ($1->dataType && $3->dataType &&
            !isArithmeticType($1->dataType) && !isArithmeticType($3->dataType)) {
            yyerror("Subtraction requires arithmetic types");
        }
        
        char* result_type = NULL;
        char* op1 = $1->tacResult;
        char* op2 = $3->tacResult;
        
        if ($1->dataType && $3->dataType) {
            result_type = usualArithConv($1->dataType, $3->dataType);
            op1 = convertType($1->tacResult, $1->dataType, result_type);
            op2 = convertType($3->tacResult, $3->dataType, result_type);
        }
        
        char* temp = newTemp();
        if (op1 && op2) {
            emit("SUB", op1, op2, temp);
        }
        
        $$ = createNode(NODE_ADDITIVE_EXPRESSION, "-");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
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
        char* op1 = $1->tacResult;
        char* op2 = $3->tacResult;
        
        if ($1->dataType && $3->dataType) {
            result_type = usualArithConv($1->dataType, $3->dataType);
            op1 = convertType($1->tacResult, $1->dataType, result_type);
            op2 = convertType($3->tacResult, $3->dataType, result_type);
        }
        
        char* temp = newTemp();
        if (op1 && op2) {
            emit("MUL", op1, op2, temp);
        }
        
        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "*");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = result_type ? strdup(result_type) : strdup("int");
    }
    | multiplicative_expression DIVIDE cast_expression {
        if ($1->dataType && $3->dataType &&
            !isArithmeticType($1->dataType) && !isArithmeticType($3->dataType)) {
            yyerror("Division requires arithmetic types");
        }
        
        char* result_type = NULL;
        char* op1 = $1->tacResult;
        char* op2 = $3->tacResult;
        
        if ($1->dataType && $3->dataType) {
            result_type = usualArithConv($1->dataType, $3->dataType);
            op1 = convertType($1->tacResult, $1->dataType, result_type);
            op2 = convertType($3->tacResult, $3->dataType, result_type);
        }
        
        char* temp = newTemp();
        if (op1 && op2) {
            emit("DIV", op1, op2, temp);
        }
        
        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "/");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = result_type ? strdup(result_type) : strdup("int");
    }
    | multiplicative_expression MODULO cast_expression {
        if ($1->dataType && $3->dataType &&
            !isArithmeticType($1->dataType) && !isArithmeticType($3->dataType)) {
            yyerror("Modulo requires arithmetic types");
        }
        
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("MOD", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "%");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    ;

cast_expression:
    unary_expression {
        $$ = $1;
    }
    | LPAREN type_name RPAREN cast_expression {
        $$ = $4;
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
        
        char* temp = newTemp();
        if ($2->tacResult) {
            emit("ADD", $2->tacResult, "1", temp);
            emit("ASSIGN", temp, "", $2->tacResult);
        }
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "++");
        addChild($$, $2);
        $$->tacResult = $2->tacResult;
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
    }
    | DECREMENT unary_expression {
        if (!$2->isLValue) {
            yyerror("Decrement requires lvalue");
        }
        
        char* temp = newTemp();
        if ($2->tacResult) {
            emit("SUB", $2->tacResult, "1", temp);
            emit("ASSIGN", temp, "", $2->tacResult);
        }
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "--");
        addChild($$, $2);
        $$->tacResult = $2->tacResult;
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
    }
    | BITWISE_AND cast_expression {
        char* temp = newTemp();
        if ($2->tacResult) {
            emit("ADDR", $2->tacResult, "", temp);
        }
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "&");
        addChild($$, $2);
        $$->tacResult = temp;
        $$->dataType = strdup("int*");
    }
    | MULTIPLY cast_expression {
        char* temp = newTemp();
        if ($2->tacResult) {
            emit("DEREF", $2->tacResult, "", temp);
        }
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "*");
        addChild($$, $2);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
        $$->isLValue = 1;
    }
    | PLUS cast_expression {
        $$ = $2;
    }
    | MINUS cast_expression {
        char* temp = newTemp();
        if ($2->tacResult) {
            emit("NEG", $2->tacResult, "", temp);
        }
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "-");
        addChild($$, $2);
        $$->tacResult = temp;
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
    }
    | BITWISE_NOT cast_expression {
        char* temp = newTemp();
        if ($2->tacResult) {
            emit("BITNOT", $2->tacResult, "", temp);
        }
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "~");
        addChild($$, $2);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | LOGICAL_NOT cast_expression {
        char* temp = newTemp();
        if ($2->tacResult) {
            emit("NOT", $2->tacResult, "", temp);
        }
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "!");
        addChild($$, $2);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | SIZEOF unary_expression {
        char* temp = newTemp();
        char size_str[32];
        if ($2->dataType) {
            sprintf(size_str, "%d", getTypeSize($2->dataType));
        } else {
            sprintf(size_str, "4");
        }
        emit("ASSIGN", size_str, "", temp);
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "sizeof");
        addChild($$, $2);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | SIZEOF LPAREN type_name RPAREN {
        char* temp = newTemp();
        emit("ASSIGN", "4", "", temp);
        $$ = createNode(NODE_UNARY_EXPRESSION, "sizeof");
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    ;

postfix_expression:
    primary_expression {
        $$ = $1;
    }
    | postfix_expression LBRACKET expression RBRACKET {
        char* temp = newTemp();
        if ($1->tacResult && $3->tacResult) {
            emit("ARRAY_ACCESS", $1->tacResult, $3->tacResult, temp);
        }
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "[]");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
        $$->isLValue = 1;
    }
    | postfix_expression LPAREN RPAREN {
        /* == FIX: Check for undeclared function and implicitly declare it == */
        Symbol* sym = lookupSymbol($1->tacResult);
        if (!sym) {
            insertFunction($1->tacResult, "int", 0, NULL, NULL);
        } else if (!sym->is_function) {
            char err_msg[256];
            sprintf(err_msg, "Called object '%s' is not a function", $1->tacResult);
            yyerror(err_msg);
        }

        char* temp = newTemp();
        if ($1->tacResult) {
            emit("CALL", $1->tacResult, "0", temp);
        }
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "()");
        addChild($$, $1);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | postfix_expression LPAREN argument_expression_list RPAREN {
        /* == FIX: Check for undeclared function and implicitly declare it == */
        Symbol* sym = lookupSymbol($1->tacResult);
        if (!sym) {
            insertFunction($1->tacResult, "int", 0, NULL, NULL);
        } else if (!sym->is_function) {
            char err_msg[256];
            sprintf(err_msg, "Called object '%s' is not a function", $1->tacResult);
            yyerror(err_msg);
        }
        
        char* temp = newTemp();
        if ($1->tacResult) {
            emit("CALL", $1->tacResult, "", temp);
        }
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "()");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
    }
    | postfix_expression DOT IDENTIFIER {
        char* temp = newTemp();
        if ($1->tacResult && $3->value) {
            emit("MEMBER_ACCESS", $1->tacResult, $3->value, temp);
        }
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, ".");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
        $$->isLValue = 1;
    }
    | postfix_expression ARROW IDENTIFIER {
        char* temp = newTemp();
        if ($1->tacResult && $3->value) {
            emit("ARROW_ACCESS", $1->tacResult, $3->value, temp);
        }
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "->");
        addChild($$, $1);
        addChild($$, $3);
        $$->tacResult = temp;
        $$->dataType = strdup("int");
        $$->isLValue = 1;
    }
    | postfix_expression INCREMENT {
        if (!$1->isLValue) {
            yyerror("Post-increment requires lvalue");
        }
        
        char* temp = newTemp();
        if ($1->tacResult) {
            emit("ASSIGN", $1->tacResult, "", temp);
            char* temp2 = newTemp();
            emit("ADD", $1->tacResult, "1", temp2);
            emit("ASSIGN", temp2, "", $1->tacResult);
        }
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "++");
        addChild($$, $1);
        $$->tacResult = temp;
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | postfix_expression DECREMENT {
        if (!$1->isLValue) {
            yyerror("Post-decrement requires lvalue");
        }
        
        char* temp = newTemp();
        if ($1->tacResult) {
            emit("ASSIGN", $1->tacResult, "", temp);
            char* temp2 = newTemp();
            emit("SUB", $1->tacResult, "1", temp2);
            emit("ASSIGN", temp2, "", $1->tacResult);
        }
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "--");
        addChild($$, $1);
        $$->tacResult = temp;
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    ;

primary_expression:
    IDENTIFIER {
        $$ = $1;
        if ($1 && $1->value) {
            Symbol* sym = lookupSymbol($1->value);
            /* == FIX: Relaxed immediate error checking for undeclared identifiers. == */
            /* This allows function calls to be handled properly later.         */
            if (sym) {
                $$->tacResult = strdup(sym->name);
                $$->dataType = strdup(sym->is_function ? sym->return_type : sym->type);
                $$->isLValue = !sym->is_function;
            } else {
                // Potentially undeclared. Don't error yet. Assume it's an identifier.
                $$->tacResult = strdup($1->value);
                $$->dataType = strdup("int"); // Assume int, will be checked at point of use
                $$->isLValue = 1;             // Assume lvalue
            }
        }
    }
    | INTEGER_CONSTANT {
        $$ = $1;
        if ($1 && $1->value) {
            char* temp = newTemp();
            emit("ASSIGN", $1->value, "", temp);
            
            $$->tacResult = temp;
            $$->dataType = strdup("int");
        }
    }
    | HEX_CONSTANT {
        $$ = $1;
        if ($1 && $1->value) {
            char* temp = newTemp();
            emit("ASSIGN", $1->value, "", temp);
            
            $$->tacResult = temp;
            $$->dataType = strdup("int");
        }
    }
    | OCTAL_CONSTANT {
        $$ = $1;
        if ($1 && $1->value) {
            char* temp = newTemp();
            emit("ASSIGN", $1->value, "", temp);
            
            $$->tacResult = temp;
            $$->dataType = strdup("int");
        }
    }
    | BINARY_CONSTANT {
        $$ = $1;
        if ($1 && $1->value) {
            char* temp = newTemp();
            emit("ASSIGN", $1->value, "", temp);
            
            $$->tacResult = temp;
            $$->dataType = strdup("int");
        }
    }
    | FLOAT_CONSTANT {
        $$ = $1;
        if ($1 && $1->value) {
            char* temp = newTemp();
            emit("ASSIGN", $1->value, "", temp);
            
            $$->tacResult = temp;
            $$->dataType = strdup("float");
        }
    }
    | CHAR_CONSTANT {
        $$ = $1;
        if ($1 && $1->value) {
            char* temp = newTemp();
            emit("ASSIGN", $1->value, "", temp);
            
            $$->tacResult = temp;
            $$->dataType = strdup("char");
        }
    }
    | STRING_LITERAL {
        $$ = $1;
        if ($1 && $1->value) {
            char* temp = newTemp();
            emit("ASSIGN", $1->value, "", temp);
            
            $$->tacResult = temp;
            $$->dataType = strdup("char*");
        }
    }
    | LPAREN expression RPAREN {
        $$ = $2;
    }
    ;

argument_expression_list:
    assignment_expression {
        if ($1 && $1->tacResult) {
            emit("ARG", $1->tacResult, "", "");
        }
        $$ = createNode(NODE_EXPRESSION, "args");
        addChild($$, $1);
    }
    | argument_expression_list COMMA assignment_expression {
        if ($3 && $3->tacResult) {
            emit("ARG", $3->tacResult, "", "");
        }
        $$ = $1;
        addChild($$, $3);
    }
    ;

%%

/* ========================================================== */
/* PART 5: C Function Implementations                         */
/* ========================================================== */
Symbol symtab[MAX_SYMBOLS];

// ============================================================================
// NODE AND TREE FUNCTIONS
// ============================================================================
TreeNode* createNode(NodeType type, const char* value) {
    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->dataType = NULL;
    node->isLValue = 0;
    node->tacResult = NULL;
    node->trueLabel = NULL;
    node->falseLabel = NULL;
    node->nextLabel = NULL;
    node->startLabel = NULL;
    node->endLabel = NULL;
    node->incrLabel = NULL;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    node->lineNumber = yylineno;
    return node;
}

void addChild(TreeNode* parent, TreeNode* child) {
    if (!parent || !child) return;
    if (parent->childCount >= parent->childCapacity) {
        parent->childCapacity = parent->childCapacity == 0 ? 4 : parent->childCapacity * 2;
        parent->children = (TreeNode**)realloc(parent->children,
                                               parent->childCapacity * sizeof(TreeNode*));
    }
    parent->children[parent->childCount++] = child;
}

void freeNode(TreeNode* node) {
    if (!node) return;
    if (node->value) free(node->value);
    if (node->dataType) free(node->dataType);
    if (node->tacResult) free(node->tacResult);
    if (node->trueLabel) free(node->trueLabel);
    if (node->falseLabel) free(node->falseLabel);
    if (node->nextLabel) free(node->nextLabel);
    if (node->startLabel) free(node->startLabel);
    if (node->endLabel) free(node->endLabel);
    if (node->incrLabel) free(node->incrLabel);
    
    for (int i = 0; i < node->childCount; i++) {
        freeNode(node->children[i]);
    }
    if (node->children) free(node->children);
    free(node);
}


// ============================================================================
// IR INFRASTRUCTURE
// ============================================================================
typedef struct {
    char op[32];
    char arg1[128];
    char arg2[128];
    char result[128];
} Quadruple;

#define MAX_IR_SIZE 10000
Quadruple IR[MAX_IR_SIZE];
int irCount = 0;
int tempCount = 0;
int labelCount = 0;

void emit(const char* op, const char* arg1, const char* arg2, const char* result) {
    if (irCount >= MAX_IR_SIZE) {
        fprintf(stderr, "Error: IR size limit exceeded\n");
        return;
    }
    strcpy(IR[irCount].op, op);
    strcpy(IR[irCount].arg1, arg1 ? arg1 : "");
    strcpy(IR[irCount].arg2, arg2 ? arg2 : "");
    strcpy(IR[irCount].result, result ? result : "");
    irCount++;
}

char* newTemp() {
    static char temp[16];
    sprintf(temp, "t%d", tempCount++);
    return strdup(temp);
}

char* newLabel() {
    static char label[32];
    sprintf(label, "L%d", labelCount++);
    return strdup(label);
}

void printIR(const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s for writing\n", filename);
        return;
    }
    
    fprintf(fp, "# Three-Address Code (Intermediate Representation)\n");
    fprintf(fp, "# Format: OPERATION ARG1 ARG2 RESULT\n");
    fprintf(fp, "# ================================================\n\n");
    
    for (int i = 0; i < irCount; i++) {
        if (strlen(IR[i].op) > 0) {
            if (strcmp(IR[i].op, "LABEL") == 0) {
                 fprintf(fp, "\n%s:\n", IR[i].arg1);
            } else if (strcmp(IR[i].op, "FUNC_BEGIN") == 0) {
                 fprintf(fp, "\n# Function Start: %s\n", IR[i].arg1);
            } else if (strcmp(IR[i].op, "FUNC_END") == 0) {
                 fprintf(fp, "# Function End: %s\n", IR[i].arg1);
            }
            else {
                fprintf(fp, "    %-15s", IR[i].op);
                if (strlen(IR[i].arg1) > 0) fprintf(fp, " %-25s", IR[i].arg1);
                else fprintf(fp, " %-25s", "");
                if (strlen(IR[i].arg2) > 0) fprintf(fp, " %-25s", IR[i].arg2);
                else fprintf(fp, " %-25s", "");
                if (strlen(IR[i].result) > 0) fprintf(fp, " %-25s", IR[i].result);
                fprintf(fp, "\n");
            }
        }
    }
    
    fclose(fp);
    printf("\nIntermediate code written to: %s\n", filename);
    printf("Total IR instructions: %d\n", irCount);
}


// ============================================================================
// CONTROL FLOW LABEL STACKS
// ============================================================================
#define MAX_LOOP_DEPTH 100
typedef struct { char label[32]; } Label;
Label loopContinueStack[MAX_LOOP_DEPTH];
Label loopBreakStack[MAX_LOOP_DEPTH];
int loopDepth = 0;
Label switchEndStack[MAX_LOOP_DEPTH];
int switchDepth = 0;

void pushLoopLabels(char* continue_label, char* break_label) {
    if (loopDepth >= MAX_LOOP_DEPTH) { fprintf(stderr, "Error: Loop nesting too deep\n"); return; }
    strcpy(loopContinueStack[loopDepth].label, continue_label);
    strcpy(loopBreakStack[loopDepth].label, break_label);
    loopDepth++;
}

void popLoopLabels() { if (loopDepth > 0) { loopDepth--; } }
char* getCurrentLoopContinue() { return (loopDepth > 0) ? loopContinueStack[loopDepth - 1].label : NULL; }
char* getCurrentLoopBreak() { return (loopDepth > 0) ? loopBreakStack[loopDepth - 1].label : NULL; }

void pushSwitchLabel(char* end_label) {
    if (switchDepth >= MAX_LOOP_DEPTH) { fprintf(stderr, "Error: Switch nesting too deep\n"); return; }
    strcpy(switchEndStack[switchDepth].label, end_label);
    switchDepth++;
}
void popSwitchLabel() { if (switchDepth > 0) { switchDepth--; } }
char* getCurrentSwitchEnd() { return (switchDepth > 0) ? switchEndStack[switchDepth - 1].label : NULL; }

// ============================================================================
// SYMBOL TABLE
// ============================================================================
int getTypeSize(const char* type) {
    if (strcmp(type, "char") == 0) return 1;
    if (strcmp(type, "short") == 0) return 2;
    if (strcmp(type, "int") == 0) return 4;
    if (strcmp(type, "long") == 0) return 8;
    if (strcmp(type, "float") == 0) return 4;
    if (strcmp(type, "double") == 0) return 8;
    if (strstr(type, "*")) return 8;
    return 4;
}

void insertVariable(const char* name, const char* type, int is_array, int* dims, int num_dims, int ptr_level) {
    if (symCount >= MAX_SYMBOLS) { fprintf(stderr, "Error: Symbol table overflow\n"); return; }
    for (int i = symCount - 1; i >= 0; i--) {
        if (strcmp(symtab[i].name, name) == 0 && symtab[i].scope_level == current_scope) { return; }
    }
    
    strcpy(symtab[symCount].name, name);
    strcpy(symtab[symCount].type, type);
    strcpy(symtab[symCount].kind, is_array ? "array" : (ptr_level > 0 ? "pointer" : "variable"));
    symtab[symCount].scope_level = current_scope;
    symtab[symCount].offset = current_offset;
    symtab[symCount].is_array = is_array;
    symtab[symCount].ptr_level = ptr_level;
    symtab[symCount].is_function = 0;
    
    int base_size = getTypeSize(type);
    if (is_array) {
        symtab[symCount].num_dims = num_dims;
        int total_elements = 1;
        for (int i = 0; i < num_dims; i++) {
            symtab[symCount].array_dims[i] = dims[i];
            total_elements *= dims[i];
        }
        symtab[symCount].size = base_size * total_elements;
    } else {
        symtab[symCount].size = base_size;
    }
    
    current_offset += symtab[symCount].size;
    symCount++;
}

void insertFunction(const char* name, const char* ret_type, int param_count, char params[][128], char param_names[][128]) {
    if (symCount >= MAX_SYMBOLS) { fprintf(stderr, "Error: Symbol table overflow\n"); return; }
    strcpy(symtab[symCount].name, name);
    strcpy(symtab[symCount].return_type, ret_type);
    strcpy(symtab[symCount].kind, "function");
    symtab[symCount].scope_level = 0;
    symtab[symCount].is_function = 1;
    symtab[symCount].param_count = param_count;
    for (int i = 0; i < param_count; i++) {
        strcpy(symtab[symCount].param_types[i], params[i]);
        strcpy(symtab[symCount].param_names[i], param_names[i]);
    }
    symCount++;
}

Symbol* lookupSymbol(const char* name) {
    for (int i = symCount - 1; i >= 0; i--) {
        if (strcmp(symtab[i].name, name) == 0 && symtab[i].scope_level <= current_scope) {
            return &symtab[i];
        }
    }
    return NULL;
}

void enterScope() { current_scope++; }
void exitScope() {
    while (symCount > 0 && symtab[symCount - 1].scope_level == current_scope) {
        symCount--;
    }
    current_scope--;
}

// ============================================================================
// TYPE SYSTEM UTILITIES
// ============================================================================
int isArithmeticType(const char* type) {
    return (strcmp(type, "int") == 0 || strcmp(type, "char") == 0 ||
            strcmp(type, "short") == 0 || strcmp(type, "long") == 0 ||
            strcmp(type, "float") == 0 || strcmp(type, "double") == 0);
}

char* usualArithConv(const char* t1, const char* t2) {
    static char result[32];
    if (strcmp(t1, "double") == 0 || strcmp(t2, "double") == 0) strcpy(result, "double");
    else if (strcmp(t1, "float") == 0 || strcmp(t2, "float") == 0) strcpy(result, "float");
    else if (strcmp(t1, "long") == 0 || strcmp(t2, "long") == 0) strcpy(result, "long");
    else strcpy(result, "int");
    return result;
}

char* convertType(char* place, const char* from_type, const char* to_type) {
    if (strcmp(from_type, to_type) == 0) return place;
    char* temp = newTemp();
    char cast_op[64];
    sprintf(cast_op, "CAST_%s_to_%s", from_type, to_type);
    emit(cast_op, place, "", temp);
    return temp;
}

int isAssignable(const char* lhs_type, const char* rhs_type) {
    if (strcmp(lhs_type, rhs_type) == 0) return 1;
    if (isArithmeticType(lhs_type) && isArithmeticType(rhs_type)) return 1;
    if (strstr(lhs_type, "*") && strstr(rhs_type, "*")) return 1;
    return 0;
}

int is_type_name(const char* name) {
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, name) == 0 &&
            strcmp(symtab[i].kind, "typedef") == 0) {
            return 1;
        }
    }
    return 0;
}

void insertSymbol(const char* name, const char* type, int is_function) {
    if (is_function) {
        insertFunction(name, type, 0, NULL, NULL);
    } else {
        insertVariable(name, type, 0, NULL, 0, 0);
    }
}

void setCurrentType(const char* type) { strcpy(currentType, type); }


// ============================================================================
// MAIN, ERROR, AND SYMBOL TABLE PRINT FUNCTIONS
// ============================================================================
void yyerror(const char* msg) {
    if (!recovering_from_error) {
        fprintf(stderr, "Syntax Error on line %d: %s at '%s'\n", yylineno, msg, yytext);
        error_count++;
        recovering_from_error = 1;
    }
}

void printSymbolTable() {
    printf("\n=== SYMBOL TABLE ===\n");
    printf("%-20s %-15s %-10s %-10s %-10s\n", "Name", "Type", "Kind", "Scope", "Size");
    printf("------------------------------------------------------------------------\n");
    for (int i = 0; i < symCount; i++) {
        printf("%-20s %-15s %-10s %-10d %-10d\n",
               symtab[i].name, symtab[i].type, symtab[i].kind,
               symtab[i].scope_level, symtab[i].size);
    }
    printf("------------------------------------------------------------------------\n");
    printf("Total symbols: %d\n\n", symCount);
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
    printf("=================================================\n");
    printf("  Intermediate Code Generator for C Language\n");
    printf("=================================================\n");
    printf("Input file: %s\n", argv[1]);
    printf("=================================================\n\n");
    
    yyparse();
    
    if (error_count == 0) {
        printf("\n=== PARSING SUCCESSFUL ===\n");
        printSymbolTable();
        
        char output_file[256];
        strcpy(output_file, argv[1]);
        char* dot = strrchr(output_file, '.');
        if (dot) *dot = '\0';
        strcat(output_file, ".ir");
        
        printIR(output_file);
        
        printf("\n=== CODE GENERATION COMPLETED ===\n");
    } else {
        printf("\n=== PARSING FAILED ===\n");
        printf("Total errors: %d\n", error_count);
    }
    fclose(yyin);
    return (error_count > 0) ? 1 : 0;
}