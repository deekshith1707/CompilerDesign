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
int semantic_error_count = 0;
static int anonymous_union_counter = 0;
static int anonymous_struct_counter = 0;
int recovering_from_error = 0;
int in_typedef = 0;
int is_static = 0;  // Flag to track if current declaration is static
int in_function_body = 0;  // Flag to track if we're in a function body compound statement
int param_count_temp = 0;  // Temporary counter for function parameters
int parsing_function_decl = 0;  // Flag to indicate we're parsing a function declaration
int in_function_definition = 0;  // Flag to indicate we're parsing a function definition (not just a declaration)
char function_return_type[128] = "int";  // Save function return type before parsing parameters
int loop_depth = 0;  // Track nesting depth of loops (for break/continue validation)
int switch_depth = 0;  // Track nesting depth of switch statements (for break validation)

// Structure to track goto statements for validation
typedef struct {
    char label_name[256];
    int line_number;
} GotoRef;

GotoRef pending_gotos[100];  // Track up to 100 goto statements per function
int pending_goto_count = 0;

// Structure to store parameter information during parsing
typedef struct {
    char name[256];
    char type[256];
    int ptrLevel;
} ParamInfo;

ParamInfo pending_params[32];  // Store up to 32 parameters
int pending_param_count = 0;

// Global AST root, to be passed to the IR generator
TreeNode* ast_root = NULL;

// Forward declarations
extern int yylex();
void yyerror(const char* msg);
void semantic_error(const char* msg);
int isFunctionDeclarator(TreeNode* node);
int hasParameterListDescendant(TreeNode* node);

// Helper function to check if a node is an lvalue
int isLValue(TreeNode* node) {
    if (!node) return 0;
    // Variables, array accesses, and dereferenced pointers are lvalues
    if (node->type == NODE_IDENTIFIER) return 1;
    if (node->isLValue) return 1;
    if (node->type == NODE_POSTFIX_EXPRESSION && strcmp(node->value, "[]") == 0) return 1;
    if (node->type == NODE_UNARY_EXPRESSION && strcmp(node->value, "*") == 0) return 1;
    if (node->type == NODE_POSTFIX_EXPRESSION && strcmp(node->value, "->") == 0) return 1;
    if (node->type == NODE_POSTFIX_EXPRESSION && strcmp(node->value, ".") == 0) return 1;
    return 0;
}

// Helper function to check if a type is a pointer
int isPointerType(const char* type) {
    if (!type) return 0;
    return (strchr(type, '*') != NULL);
}

// Helper function to count pointer levels in a declarator tree
int countPointerLevels(TreeNode* node) {
    if (!node) return 0;
    if (node->type == NODE_POINTER) {
        // Count this pointer and any nested pointers
        int count = 1;
        for (int i = 0; i < node->childCount; i++) {
            count += countPointerLevels(node->children[i]);
        }
        return count;
    }
    return 0;
}

// Helper function to build full type string with pointers
void buildFullType(char* dest, const char* baseType, int ptrLevel) {
    strcpy(dest, baseType);
    for (int i = 0; i < ptrLevel; i++) {
        strcat(dest, " *");
    }
}

// Helper function to extract identifier name from declarator
const char* extractIdentifierName(TreeNode* node) {
    if (!node) return NULL;
    if (node->type == NODE_IDENTIFIER) return node->value;
    
    // Traverse through pointers and other declarator constructs
    for (int i = 0; i < node->childCount; i++) {
        const char* name = extractIdentifierName(node->children[i]);
        if (name) return name;
    }
    return NULL;
}

// Helper function to check if a declarator represents a function pointer
int isFunctionPointer(TreeNode* node) {
    if (!node) return 0;
    
    // Function pointer pattern: MULTIPLY -> (eventual) parameter_type_list
    // Check if this node is a pointer
    if (node->type == NODE_POINTER) {
        // Check if any descendant has a parameter list (indicating function pointer)
        return hasParameterListDescendant(node);
    }
    
    // Also check children recursively
    for (int i = 0; i < node->childCount; i++) {
        if (isFunctionPointer(node->children[i])) {
            return 1;
        }
    }
    
    return 0;
}

// Helper function to check if a node has a parameter list descendant
int hasParameterListDescendant(TreeNode* node) {
    if (!node) return 0;
    
    // Check if this node is a parameter list
    if (node->type == NODE_PARAMETER_LIST) {
        return 1;
    }
    
    // Recursively check all children
    for (int i = 0; i < node->childCount; i++) {
        if (hasParameterListDescendant(node->children[i])) {
            return 1;
        }
    }
    
    return 0;
}

// Helper function to count array dimensions in a declarator
// In function parameters, arrays decay to pointers, so each [] adds a pointer level
int countArrayDimensions(TreeNode* node) {
    if (!node) return 0;
    
    int count = 0;
    
    // Check if this node represents an array
    if (node->type == NODE_DIRECT_DECLARATOR && 
        (strcmp(node->value, "array") == 0 || strcmp(node->value, "array[]") == 0)) {
        count = 1;
    }
    
    // Recursively count in all children
    for (int i = 0; i < node->childCount; i++) {
        count += countArrayDimensions(node->children[i]);
    }
    
    return count;
}

// Helper function to build function pointer type string
void buildFunctionPointerType(char* dest, const char* returnType, TreeNode* declarator) {
    // For now, build a simplified function pointer type
    // Format: "returnType (*)(param_types...)"
    strcpy(dest, returnType);
    strcat(dest, " (*)()");  // Simplified - would need to extract parameter types
}

// Helper function to extract type from declaration_specifiers
const char* extractTypeFromSpecifiers(TreeNode* node) {
    if (!node) return NULL;
    if (node->value && (strstr(node->value, "int") || strstr(node->value, "char") || 
                        strstr(node->value, "float") || strstr(node->value, "double") ||
                        strstr(node->value, "struct"))) {
        return node->value;
    }
    for (int i = 0; i < node->childCount; i++) {
        const char* type = extractTypeFromSpecifiers(node->children[i]);
        if (type) return type;
    }
    return NULL;
}

// Forward declaration for struct member processing
void processStructDeclaration(TreeNode* declNode, StructMember* members, int* memberCount, int maxMembers);

// Helper to recursively process struct declarations
void processStructDeclaration(TreeNode* declNode, StructMember* members, int* memberCount, int maxMembers) {
    if (!declNode || *memberCount >= maxMembers) return;
    
    // Check if this is a NODE_DECLARATION with "struct_decl"
    if (declNode->type == NODE_DECLARATION && declNode->value && 
        strcmp(declNode->value, "struct_decl") == 0 && declNode->childCount >= 2) {
        
        // First child: declaration_specifiers (the type)
        TreeNode* specifiers = declNode->children[0];
        const char* memberType = extractTypeFromSpecifiers(specifiers);
        
        if (memberType) {
            // Second child: struct_declarator_list (the names)
            TreeNode* declaratorList = declNode->children[1];
            
            // Extract all declarators in this list
            for (int j = 0; j < declaratorList->childCount && *memberCount < maxMembers; j++) {
                TreeNode* declarator = declaratorList->children[j];
                const char* memberName = extractIdentifierName(declarator);
                if (memberName) {
                    strncpy(members[*memberCount].name, memberName, 127);
                    members[*memberCount].name[127] = '\0';
                    strncpy(members[*memberCount].type, memberType, 127);
                    members[*memberCount].type[127] = '\0';
                    (*memberCount)++;
                }
            }
            
            // If declaratorList has no children but has a name itself, it's a single declarator
            if (declaratorList->childCount == 0) {
                const char* memberName = extractIdentifierName(declaratorList);
                if (memberName && *memberCount < maxMembers) {
                    strncpy(members[*memberCount].name, memberName, 127);
                    members[*memberCount].name[127] = '\0';
                    strncpy(members[*memberCount].type, memberType, 127);
                    members[*memberCount].type[127] = '\0';
                    (*memberCount)++;
                }
            }
        }
    }
    
    // Recursively process all children (to handle linked list of declarations)
    for (int i = 0; i < declNode->childCount; i++) {
        processStructDeclaration(declNode->children[i], members, memberCount, maxMembers);
    }
}

// Helper function to parse and insert struct definition
void parseStructDefinition(const char* structName, TreeNode* memberListNode) {
    if (!structName || !memberListNode) return;
    
    StructMember members[MAX_STRUCT_MEMBERS];
    int memberCount = 0;
    
    // Start processing from the root
    processStructDeclaration(memberListNode, members, &memberCount, MAX_STRUCT_MEMBERS);
    
    // Insert the struct definition
    if (memberCount > 0) {
        insertStruct(structName, members, memberCount);
    }
}

// Helper function to parse and insert union definition
void parseUnionDefinition(const char* unionName, TreeNode* memberListNode) {
    if (!unionName || !memberListNode) return;
    
    StructMember members[MAX_STRUCT_MEMBERS];
    int memberCount = 0;
    
    // Start processing from the root (same as struct)
    processStructDeclaration(memberListNode, members, &memberCount, MAX_STRUCT_MEMBERS);
    
    // Insert the union definition (size will be max of members)
    if (memberCount > 0) {
        insertUnion(unionName, members, memberCount);
    }
}

// Helper function to extract array dimensions from declarator
int extractArrayDimensions(TreeNode* node, int* dims, int maxDims) {
    if (!node) return 0;
    
    int count = 0;
    
    // Check if this is an array declarator node
    if (node->type == NODE_DIRECT_DECLARATOR && node->value && 
        strcmp(node->value, "array") == 0 && node->childCount >= 2) {
        // Second child contains the dimension
        TreeNode* dimNode = node->children[1];
        if (dimNode && dimNode->value) {
            if (count < maxDims) {
                dims[count++] = atoi(dimNode->value);
            }
        }
        // First child might be another array declarator
        if (node->children[0]) {
            count += extractArrayDimensions(node->children[0], &dims[count], maxDims - count);
        }
    } else {
        // Recursively check children
        for (int i = 0; i < node->childCount && count < maxDims; i++) {
            count += extractArrayDimensions(node->children[i], &dims[count], maxDims - count);
        }
    }
    
    return count;
}

// Helper function to check if declarator has array brackets
int hasArrayBrackets(TreeNode* node) {
    if (!node) return 0;
    
    // Check if this is an array declarator node
    if (node->type == NODE_DIRECT_DECLARATOR && node->value && 
        (strcmp(node->value, "array") == 0 || strcmp(node->value, "array[]") == 0)) {
        return 1;
    }
    
    // Recursively check children
    for (int i = 0; i < node->childCount; i++) {
        if (hasArrayBrackets(node->children[i])) {
            return 1;
        }
    }
    return 0;
}

// Helper function to check if a declarator is a function declarator (has parameters)
int isFunctionDeclarator(TreeNode* node) {
    if (!node) return 0;
    
    // Check if this node has NODE_PARAMETER_LIST as a child
    for (int i = 0; i < node->childCount; i++) {
        if (node->children[i] && node->children[i]->type == NODE_PARAMETER_LIST) {
            return 1;
        }
        // Recursively check children
        if (isFunctionDeclarator(node->children[i])) {
            return 1;
        }
    }
    return 0;
}

// Helper function to check if array declarator has empty brackets []
int hasEmptyArrayBrackets(TreeNode* node) {
    if (!node) return 0;
    
    // Check if this is an array declarator with empty brackets
    if (node->type == NODE_DIRECT_DECLARATOR && node->value && 
        strcmp(node->value, "array[]") == 0) {
        return 1;
    }
    
    // Recursively check children
    for (int i = 0; i < node->childCount; i++) {
        if (hasEmptyArrayBrackets(node->children[i])) {
            return 1;
        }
    }
    return 0;
}

// Helper function to count initializer elements
int countInitializerElements(TreeNode* initNode) {
    if (!initNode) return 0;
    
    // Check if it's a string literal (for char arrays)
    if (initNode->type == NODE_STRING_LITERAL && initNode->value) {
        // Count the string length including NUL terminator
        // String value includes quotes, so subtract 2 and add 1 for NUL
        int len = strlen(initNode->value);
        if (len >= 2 && initNode->value[0] == '"' && initNode->value[len-1] == '"') {
            return len - 2 + 1;  // -2 for quotes, +1 for NUL
        }
        return len + 1;  // Fallback: add 1 for NUL
    }
    
    // Check if it's an initializer list
    if (initNode->type == NODE_INITIALIZER && initNode->value && 
        strcmp(initNode->value, "init_list") == 0) {
        // Count the direct children (each is an initializer element)
        return initNode->childCount;
    }
    
    // Single expression initializer
    return 1;
}

// Helper function to check if base type is char
int isCharType(const char* type) {
    return (strcmp(type, "char") == 0);
}

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
%token <node> INT CHAR_TOKEN FLOAT_TOKEN DOUBLE LONG SHORT UNSIGNED SIGNED VOID BOOL
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
%type <node> if_header marker_while_start for_label_marker for_increment_expression
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
        if (error_count == 0) {
            printf("\n=== PARSING COMPLETED SUCCESSFULLY ===\n");
        }
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
    | statement { $$ = $1; }
    | preprocessor_directive { $$ = NULL; }
    ;

preprocessor_directive:
    PREPROCESSOR { 
        // Check if this is #include <stdio.h> and insert standard library functions
        if ($1 && $1->value && strstr($1->value, "stdio.h")) {
            // Insert common stdio.h functions
            insertExternalFunction("printf", "int");
            insertExternalFunction("scanf", "int");
            insertExternalFunction("fprintf", "int");
            insertExternalFunction("fscanf", "int");
            insertExternalFunction("sprintf", "int");
            insertExternalFunction("sscanf", "int");
            insertExternalFunction("fopen", "FILE*");
            insertExternalFunction("fclose", "int");
            insertExternalFunction("fread", "size_t");
            insertExternalFunction("fwrite", "size_t");
            insertExternalFunction("fgetc", "int");
            insertExternalFunction("fputc", "int");
            insertExternalFunction("fputs", "int");
            insertExternalFunction("fgets", "char*");
            insertExternalFunction("puts", "int");
            insertExternalFunction("getchar", "int");
            insertExternalFunction("putchar", "int");
        }
        // Check if this is #include <stdlib.h> and insert standard library functions
        if ($1 && $1->value && strstr($1->value, "stdlib.h")) {
            // Insert common stdlib.h functions
            insertExternalFunction("atoi", "int");
            insertExternalFunction("atof", "double");
            insertExternalFunction("atol", "long");
            insertExternalFunction("malloc", "void*");
            insertExternalFunction("calloc", "void*");
            insertExternalFunction("realloc", "void*");
            insertExternalFunction("free", "void");
            insertExternalFunction("exit", "void");
            insertExternalFunction("abort", "void");
            insertExternalFunction("system", "int");
            insertExternalFunction("getenv", "char*");
            insertExternalFunction("rand", "int");
            insertExternalFunction("srand", "void");
            insertExternalFunction("abs", "int");
            insertExternalFunction("labs", "long");
            insertExternalFunction("div", "div_t");
            insertExternalFunction("ldiv", "ldiv_t");
            insertExternalFunction("qsort", "void");
            insertExternalFunction("bsearch", "void*");
        }
        $$ = NULL; 
    }
    ;

function_definition:
    declaration_specifiers declarator
    {
        // Now we know it's a definition (has compound_statement coming)
        // Extract function name
        const char* funcName = extractIdentifierName($2);
        if (funcName) {
            // Extract return type from declaration_specifiers
            char funcRetType[128];
            strcpy(funcRetType, currentType);
            
            // Try to find the base type from declaration_specifiers
            if ($1 && $1->childCount > 0) {
                for (int i = 0; i < $1->childCount; i++) {
                    TreeNode* spec = $1->children[i];
                    if (spec && spec->value) {
                        if (strcmp(spec->value, "int") == 0 || 
                            strcmp(spec->value, "char") == 0 ||
                            strcmp(spec->value, "void") == 0 ||
                            strcmp(spec->value, "float") == 0 ||
                            strcmp(spec->value, "double") == 0) {
                            strcpy(funcRetType, spec->value);
                            break;
                        }
                    }
                }
            }
            
            insertSymbol(funcName, funcRetType, 1, is_static);
        }
        is_static = 0;
        // Enter function scope
        enterFunctionScope(funcName);
        
        // Now insert the pending parameters that were stored during parsing
        for (int i = 0; i < pending_param_count; i++) {
            insertVariable(pending_params[i].name, pending_params[i].type, 0, NULL, 0, pending_params[i].ptrLevel, 0);
        }
        // Mark them as parameters
        markRecentSymbolsAsParameters(pending_param_count);
        param_count_temp = pending_param_count;  // Update param_count_temp for IR generation
        pending_param_count = 0;  // Reset for next function
        
        in_function_body = 1;
    }
    compound_statement
    {
        $$ = createNode(NODE_FUNCTION_DEFINITION, "function");
        addChild($$, $1); // declaration_specifiers
        addChild($$, $2); // declarator (name)
        addChild($$, $4); // compound_statement (body)
        
        // Validate all goto statements now that all labels are defined
        for (int i = 0; i < pending_goto_count; i++) {
            Symbol* label = lookupLabel(pending_gotos[i].label_name);
            if (!label) {
                type_error(pending_gotos[i].line_number, "Undefined label '%s'", 
                          pending_gotos[i].label_name);
            }
        }
        pending_goto_count = 0;  // Reset for next function
        
        in_function_body = 0;
        param_count_temp = 0;
        exitFunctionScope();
        recovering_from_error = 0;
    }
    ;

declaration:
    declaration_specifiers SEMICOLON {
        $$ = createNode(NODE_DECLARATION, "declaration");
        addChild($$, $1);
        in_typedef = 0;
        is_static = 0;
        recovering_from_error = 0;
        pending_param_count = 0;  // Reset pending parameters (in case this was a function declaration)
    }
    | declaration_specifiers init_declarator_list SEMICOLON {
        $$ = createNode(NODE_DECLARATION, "declaration");
        addChild($$, $1);
        addChild($$, $2);
        in_typedef = 0;
        is_static = 0;
        recovering_from_error = 0;
        pending_param_count = 0;  // Reset pending parameters (in case this was a function declaration)
    }
    ;

declaration_specifiers:
    specifier {
        $$ = createNode(NODE_DECLARATION_SPECIFIERS, "decl_specs");
        addChild($$, $1);
        // Don't overwrite function return type if we're inside parameter parsing
        // parsing_function_decl will be set by function_definition rule
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
    | STATIC {
        is_static = 1;
        $$ = createNode(NODE_STORAGE_CLASS_SPECIFIER, "static");
    }
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
    | BOOL {
        setCurrentType("bool");
        $$ = createNode(NODE_TYPE_SPECIFIER, "bool");
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
        // Reference to existing struct
        char structType[256];
        sprintf(structType, "struct %s", $2->value);
        setCurrentType(structType);
        $$ = createNode(NODE_STRUCT_SPECIFIER, "struct");
        addChild($$, $2);
    }
    | STRUCT LBRACE struct_declaration_list RBRACE {
        // Anonymous struct - generate a name for size calculation
        char anonName[128];
        sprintf(anonName, "__anon_struct_%d", anonymous_struct_counter++);
        
        // Parse struct members and insert into struct table
        parseStructDefinition(anonName, $3);
        
        char structType[256];
        sprintf(structType, "struct %s", anonName);
        setCurrentType(structType);
        $$ = createNode(NODE_STRUCT_SPECIFIER, "struct");
        addChild($$,$3);
    }
    | STRUCT IDENTIFIER LBRACE struct_declaration_list RBRACE {
        // Struct definition - record it
        char structType[256];
        sprintf(structType, "struct %s", $2->value);
        setCurrentType(structType);
        
        // Parse struct members and insert into struct table
        parseStructDefinition($2->value, $4);
        
        $$ = createNode(NODE_STRUCT_SPECIFIER, "struct_definition");
        addChild($$, $2);
        addChild($$, $4);
    }
    | UNION IDENTIFIER {
        char unionType[256];
        sprintf(unionType, "union %s", $2->value);
        setCurrentType(unionType);
        $$ = createNode(NODE_STRUCT_SPECIFIER, "union");
        addChild($$, $2);
    }
    | UNION LBRACE struct_declaration_list RBRACE {
        // Anonymous union - generate a name for size calculation
        char anonName[128];
        sprintf(anonName, "__anon_union_%d", anonymous_union_counter++);
        
        // Parse union members and insert into union table
        parseUnionDefinition(anonName, $3);
        
        char unionType[256];
        sprintf(unionType, "union %s", anonName);
        setCurrentType(unionType);
        $$ = createNode(NODE_STRUCT_SPECIFIER, "union");
        addChild($$,$3);
    }
    | UNION IDENTIFIER LBRACE struct_declaration_list RBRACE {
        char unionType[256];
        sprintf(unionType, "union %s", $2->value);
        setCurrentType(unionType);
        
        // Parse union members and insert into union table
        parseUnionDefinition($2->value, $4);
        
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
            // Enum constants are integers, so size is 4 bytes
            insertVariable($1->value, "int", 0, NULL, 0, 0, 0);  // Enum constants are not static
            // Update the last inserted symbol's kind to enum_constant
            if (symCount > 0 && strcmp(symtab[symCount - 1].name, $1->value) == 0) {
                strcpy(symtab[symCount - 1].kind, "enum_constant");
            }
        }
        $$ = $1;
    }
    | IDENTIFIER ASSIGN constant_expression {
        if ($1 && $1->value) {
            // Enum constants are integers, so size is 4 bytes
            insertVariable($1->value, "int", 0, NULL, 0, 0, 0);  // Enum constants are not static
            // Update the last inserted symbol's kind to enum_constant
            if (symCount > 0 && strcmp(symtab[symCount - 1].name, $1->value) == 0) {
                strcpy(symtab[symCount - 1].kind, "enum_constant");
            }
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
        const char* varName = extractIdentifierName($1);
        // Skip insertion for function declarators, but not for function pointers
        // Function pointers have parameters AND a pointer level > 0
        int ptrLevel = countPointerLevels($1);
        int isFuncPtr = isFunctionPointer($1) || (ptrLevel > 0 && pending_param_count > 0);
        
        if (varName && (pending_param_count == 0 || isFuncPtr)) {
            int isArray = hasArrayBrackets($1);
            int arrayDims[10] = {0};
            int numDims = 0;
            int hasEmptyBrackets = hasEmptyArrayBrackets($1);
            
            if (isArray) {
                numDims = extractArrayDimensions($1, arrayDims, 10);
                
                // Check for error: array has empty brackets but no initializer
                if (hasEmptyBrackets && numDims == 0) {
                    type_error(yylineno, "array size missing and no initializer");
                }
            }
            
            char fullType[256];
            if (isFuncPtr) {
                // Build function pointer type
                buildFunctionPointerType(fullType, currentType, $1);
            } else {
                buildFullType(fullType, currentType, ptrLevel);
            }
            
            if (in_typedef) {
                // Insert the symbol and then manually set its kind to "typedef".
                insertVariable(varName, fullType, isArray, arrayDims, numDims, ptrLevel, 0);  // Typedefs are not static
                if (symCount > 0 && strcmp(symtab[symCount - 1].name, varName) == 0) {
                    strcpy(symtab[symCount - 1].kind, "typedef");
                }
            } else {
                // Always insert the variable into the symbol table
                // The symbol table will handle duplicates within the same scope/block
                insertVariable(varName, fullType, isArray, arrayDims, numDims, ptrLevel, is_static);
                // Mark as function pointer if needed
                if (isFuncPtr && symCount > 0 && strcmp(symtab[symCount - 1].name, varName) == 0) {
                    strcpy(symtab[symCount - 1].kind, "function_pointer");
                }
                
                // Reset pending parameters for function pointers (they don't define functions)
                if (isFuncPtr) {
                    pending_param_count = 0;
                }
            }
        }
        $$ = $1;
    }
    | declarator ASSIGN initializer {
        const char* varName = extractIdentifierName($1);
        if (varName && !in_typedef) {
            int ptrLevel = countPointerLevels($1);
            int isArray = hasArrayBrackets($1);
            int arrayDims[10] = {0};
            int numDims = 0;
            int hasEmptyBrackets = hasEmptyArrayBrackets($1);
            int isFuncPtr = isFunctionPointer($1) || (ptrLevel > 0 && pending_param_count > 0);
            
            if (isArray) {
                numDims = extractArrayDimensions($1, arrayDims, 10);
                
                // Check for negative array sizes in extracted dimensions
                for (int i = 0; i < numDims; i++) {
                    if (arrayDims[i] < 0) {
                        type_error(yylineno, "negative array size");
                        break;
                    }
                }
                
                // Handle array size inference and validation
                if (hasEmptyBrackets && numDims == 0) {
                    // Size inference: array has [] and no explicit size
                    int initCount = countInitializerElements($3);
                    if (initCount > 0) {
                        // Infer the size from initializer
                        arrayDims[0] = initCount;
                        numDims = 1;
                    } else {
                        type_error(yylineno, "array size missing and no initializer");
                    }
                } else if (numDims > 0 && arrayDims[0] > 0) {
                    // Validate: explicit size exists, check initializer count
                    int initCount = countInitializerElements($3);
                    if (initCount > arrayDims[0]) {
                        char fullType[256];
                        buildFullType(fullType, currentType, ptrLevel);
                        type_error(yylineno, "too many initializers for '%s[%d]'", 
                                  fullType, arrayDims[0]);
                    }
                    // Partial initialization is OK (initCount < arrayDims[0])
                }
            }
            
            char fullType[256];
            if (isFuncPtr) {
                // Build function pointer type
                buildFunctionPointerType(fullType, currentType, $1);
            } else {
                buildFullType(fullType, currentType, ptrLevel);
            }
            
            // Type check initialization for function pointers
            if (isFuncPtr && $3->dataType) {
                // Function pointer initialization: check if initializer is a function
                // For now, allow initialization of function pointers with identifiers
                // This will need more sophisticated checking in a complete implementation
                if ($3->type == NODE_IDENTIFIER) {
                    // Check if the identifier is a function
                    Symbol* sym = lookupSymbol($3->value);
                    if (!sym || !sym->is_function) {
                        // Don't error here - might be a valid function name
                        // The error will be caught later if it's truly invalid
                    }
                }
            }
            // Type check initialization: check if trying to initialize scalar with array
            else if (!isArray && ptrLevel == 0 && $3->dataType && isArrayType($3->dataType)) {
                // Trying to initialize non-pointer scalar with array
                char* base_type = getArrayBaseType($3->dataType);
                if (base_type) {
                    type_error(yylineno, "cannot convert array type '%s' to '%s'", $3->dataType, base_type);
                } else {
                    type_error(yylineno, "cannot convert array type '%s' to '%s'", $3->dataType, fullType);
                }
            }
            
            // Type check initialization: pointer = non-zero integer (error) - but not for function pointers
            else if (!isFuncPtr && ptrLevel > 0 && $3->dataType && !isNullPointer($3)) {
                char* init_decayed = decayArrayToPointer($3->dataType);
                // If initializer is not a pointer and not NULL, it's an error
                if (!strstr(init_decayed, "*") && !isArithmeticType(init_decayed)) {
                    type_error(yylineno, "initialization makes pointer from integer without a cast");
                } else if (isArithmeticType(init_decayed)) {
                    // Integer to pointer (not NULL)
                    type_error(yylineno, "initialization makes pointer from integer without a cast");
                }
                free(init_decayed);
            }
            
            // Type check initialization: integer = pointer (error)
            if (!isArray && ptrLevel == 0 && $3->dataType && isArithmeticType(fullType)) {
                char* init_decayed = decayArrayToPointer($3->dataType);
                if (strstr(init_decayed, "*")) {
                    // Trying to initialize integer with pointer
                    type_error(yylineno, "initialization makes integer from pointer without a cast");
                }
                free(init_decayed);
            }
            
            // Always insert the variable into the symbol table
            // The symbol table will handle duplicates within the same scope/block
            insertVariable(varName, fullType, isArray, arrayDims, numDims, ptrLevel, is_static);
            // Mark as function pointer if needed
            if (isFuncPtr && symCount > 0 && strcmp(symtab[symCount - 1].name, varName) == 0) {
                strcpy(symtab[symCount - 1].kind, "function_pointer");
            }
            /* emit("ASSIGN", ...) REMOVED */
        }        // Create an AST node for the initialization
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
    | direct_declarator LBRACKET RBRACKET { 
        $$ = createNode(NODE_DIRECT_DECLARATOR, "array[]");
        addChild($$, $1);
    }
    | direct_declarator LBRACKET constant_expression RBRACKET { 
        $$ = createNode(NODE_DIRECT_DECLARATOR, "array");
        addChild($$, $1);
        
        // Validate array size
        if ($3) {
            // Check if the size expression is of integer type
            if ($3->dataType && !isIntegerType($3->dataType)) {
                if ($3->value) {
                    type_error(yylineno, "invalid array size '%s'", $3->value);
                } else {
                    type_error(yylineno, "invalid array size (non-integer type)");
                }
            } 
            // Check if size is a negative constant
            else if ($3->value) {
                int size = atoi($3->value);
                if (size < 0) {
                    type_error(yylineno, "negative array size");
                }
            }
            addChild($$, $3);  // Store the dimension
        }
    }
    | direct_declarator LPAREN RPAREN {
        $$ = $1;
        param_count_temp = 0;  // No parameters
    }
    | direct_declarator LPAREN { param_count_temp = 0; } parameter_type_list RPAREN {
        $$ = $1;
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
        // param_count_temp is now incremented in parameter_declaration when in_function_definition is set
    }
    | parameter_list COMMA parameter_declaration {
        $$ = $1;
        addChild($$, $3);
        // param_count_temp is now incremented in parameter_declaration when in_function_definition is set
    }
    ;

parameter_declaration:
    declaration_specifiers declarator {
        // Store parameter information instead of inserting it immediately
        // This allows us to differentiate between function declarations and definitions
        $$ = createNode(NODE_PARAMETER_DECLARATION, "param");
        addChild($$, $1);
        addChild($$, $2);
        
        // Extract identifier name and count pointer levels
        const char* paramName = extractIdentifierName($2);
        if (paramName && pending_param_count < 32) {
            int ptrLevel = countPointerLevels($2);
            // In function parameters, arrays decay to pointers
            // char* argv[] becomes char**, so count array dimensions as additional pointer levels
            int arrayDims = countArrayDimensions($2);
            int totalPtrLevel = ptrLevel + arrayDims;
            
            char fullType[256];
            buildFullType(fullType, currentType, totalPtrLevel);
            
            // Store parameter info for later insertion (if this is a definition)
            strncpy(pending_params[pending_param_count].name, paramName, 255);
            strncpy(pending_params[pending_param_count].type, fullType, 255);
            pending_params[pending_param_count].ptrLevel = totalPtrLevel;
            pending_param_count++;
        }
    }
    | declaration_specifiers abstract_declarator {
        $$ = createNode(NODE_PARAMETER_DECLARATION, "param");
        addChild($$, $1);
        addChild($$, $2);
        // Also count parameters without names (for function pointers)
        pending_param_count++;
    }
    | declaration_specifiers { 
        $$ = $1;
        // Also count parameters with just type (for function pointers)
        pending_param_count++;
    }
    ;

abstract_declarator:
    MULTIPLY {
        $$ = createNode(NODE_POINTER, "*");
        // Don't enter scope for abstract declarators
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
            insertLabel($1->value);  // Insert as a label in the symbol table
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
    LBRACE {
        if (!in_function_body) {
            enterScope();  // Only enter scope for nested blocks, not function body
        }
        in_function_body = 0;  // After first brace, we're past function body level
    } RBRACE {
        $$ = createNode(NODE_COMPOUND_STATEMENT, "compound");
        if (current_scope > 0) {
            exitScope();
        }
        recovering_from_error = 0;
    }
    | LBRACE {
        if (!in_function_body) {
            enterScope();  // Only enter scope for nested blocks, not function body
        }
        in_function_body = 0;  // After first brace, we're past function body level
    } block_item_list RBRACE {
        $$ = $3; // Pass up the block_item_list node
        if (current_scope > 0) {
            exitScope();
        }
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
    | function_definition { $$ = $1; }
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
        // Check that the condition expression is not void
        if ($3 && $3->dataType && strcmp($3->dataType, "void") == 0) {
            type_error(yylineno, "void value not ignored as it ought to be");
        }
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

for_increment_expression:
    %empty { $$ = NULL; }  // Empty increment expression (e.g., for(;;))
    | expression { $$ = $1; }  // Normal increment expression
    ;

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
    | SWITCH LPAREN expression RPAREN {
        switch_depth++;  // Enter switch context
    } statement {
        /* All label, stack, and emit logic REMOVED */
        switch_depth--;  // Exit switch context
        $$ = createNode(NODE_SELECTION_STATEMENT, "switch");
        addChild($$, $3); // The expression
        addChild($$, $6); // The statement
        recovering_from_error = 0;
    }
    ;

/* == Helper rules for do-while/until == */
do_header:
    DO {
        loop_depth++;  // Enter loop context
        $$ = createNode(NODE_MARKER, "do_header");
        /* All label, stack, and emit logic REMOVED */
    }
    ;

do_trailer:
    WHILE LPAREN expression RPAREN SEMICOLON {
        /* emit("IF_TRUE_GOTO", ...) REMOVED */
        // Check that the condition expression is not void
        if ($3 && $3->dataType && strcmp($3->dataType, "void") == 0) {
            type_error(yylineno, "void value not ignored as it ought to be");
        }
        $$ = createNode(NODE_ITERATION_STATEMENT, "do_while");
        addChild($$, $3); // expression
    }
    | UNTIL LPAREN expression RPAREN SEMICOLON {
        /* emit("IF_FALSE_GOTO", ...) REMOVED */
        // Check that the condition expression is not void
        if ($3 && $3->dataType && strcmp($3->dataType, "void") == 0) {
            type_error(yylineno, "void value not ignored as it ought to be");
        }
        $$ = createNode(NODE_ITERATION_STATEMENT, "do_until");
        addChild($$, $3); // expression
    }
    ;

iteration_statement:
    WHILE LPAREN marker_while_start expression RPAREN
    {
        /* emit("IF_FALSE_GOTO", ...) REMOVED */
        // Check that the condition expression is not void
        if ($4 && $4->dataType && strcmp($4->dataType, "void") == 0) {
            type_error(yylineno, "void value not ignored as it ought to be");
        }
        loop_depth++;  // Enter loop context
    }
    statement
    {
        /* All emit and popLoopLabels logic REMOVED */
        loop_depth--;  // Exit loop context
        $$ = createNode(NODE_ITERATION_STATEMENT, "while");
        addChild($$, $4); // expression
        addChild($$, $7); // statement
        addChild($$, $3); // Add the marker node
        recovering_from_error = 0;
    }
    | do_header statement do_trailer {
        /* All emit and popLoopLabels logic REMOVED */
        loop_depth--;  // Exit loop context (entered in do_header)
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
            // Check that the condition expression is not void (if present)
            if ($5 && $5->childCount > 0 && $5->children[0]) {
                TreeNode* cond = $5->children[0];
                if (cond->dataType && strcmp(cond->dataType, "void") == 0) {
                    type_error(yylineno, "void value not ignored as it ought to be");
                }
            }
            loop_depth++;  // Enter loop context
        }
        for_increment_expression              // $7: incr (can be empty)
        RPAREN                                // $8
        statement                             // $9: body
        {
            /* All emit and popLoopLabels logic REMOVED */
            loop_depth--;  // Exit loop context
            $$ = createNode(NODE_ITERATION_STATEMENT, "for");
            addChild($$, $3);  // init
            addChild($$, $5);  // cond
            if ($7) addChild($$, $7);  // incr (only if present)
            addChild($$, $9);  // statement body
            addChild($$, $4);  // Add the marker node
            recovering_from_error = 0;
        }
    | FOR LPAREN
        {
            // C99: Variables declared in for loop init are scoped to the loop
            enterScope();
        }
        declaration                           // $4: init (C99 style variable declaration)
        for_label_marker                      // $5: marker
        expression_statement                  // $6: cond
        { 
            /* emit("IF_FALSE_GOTO", ...) REMOVED */
            // Check that the condition expression is not void (if present)
            if ($6 && $6->childCount > 0 && $6->children[0]) {
                TreeNode* cond = $6->children[0];
                if (cond->dataType && strcmp(cond->dataType, "void") == 0) {
                    type_error(yylineno, "void value not ignored as it ought to be");
                }
            }
            loop_depth++;  // Enter loop context
        }
        for_increment_expression              // $8: incr (can be empty)
        RPAREN                                // $9
        statement                             // $10: body
        {
            /* All emit and popLoopLabels logic REMOVED */
            loop_depth--;  // Exit loop context
            $$ = createNode(NODE_ITERATION_STATEMENT, "for");
            addChild($$, $4);  // init (declaration)
            addChild($$, $6);  // cond
            if ($8) addChild($$, $8);  // incr (only if present)
            addChild($$, $10);  // statement body
            addChild($$, $5);  // Add the marker node
            recovering_from_error = 0;
            
            // Exit the for loop scope
            exitScope();
        }
    ;

jump_statement:
    GOTO IDENTIFIER SEMICOLON {
        /* emit("GOTO", ...) REMOVED */
        // Record goto for later validation (after all labels are defined)
        if ($2 && $2->value && pending_goto_count < 100) {
            strcpy(pending_gotos[pending_goto_count].label_name, $2->value);
            pending_gotos[pending_goto_count].line_number = yylineno;
            pending_goto_count++;
        }
        $$ = createNode(NODE_JUMP_STATEMENT, "goto");
        addChild($$, $2);
    }
    | CONTINUE SEMICOLON {
        /* All label logic and emit REMOVED */
        // Semantic check: continue must be inside a loop
        if (loop_depth == 0) {
            type_error(yylineno, "'continue' statement not in loop");
        }
        $$ = createNode(NODE_JUMP_STATEMENT, "continue");
    }
    | BREAK SEMICOLON {
        /* All label logic and emit REMOVED */
        // Semantic check: break must be inside a loop or switch
        if (loop_depth == 0 && switch_depth == 0) {
            type_error(yylineno, "'break' statement not in loop or switch");
        }
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
        // Enhanced assignment type checking
        TypeCheckResult result = checkAssignment($1, $3);
        
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "=");
        addChild($$, $1);
        addChild($$, $3);

        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression PLUS_ASSIGN assignment_expression {
        if (!isLValue($1)) {
            semantic_error("lvalue required as left operand of assignment");
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "+=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression MINUS_ASSIGN assignment_expression {
        if (!isLValue($1)) {
            semantic_error("lvalue required as left operand of assignment");
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "-=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression MUL_ASSIGN assignment_expression {
        if (!isLValue($1)) {
            semantic_error("lvalue required as left operand of assignment");
        }
        $$ = createNode(NODE_ASSIGNMENT_EXPRESSION, "*=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
    }
    | unary_expression DIV_ASSIGN assignment_expression {
        if (!isLValue($1)) {
            semantic_error("lvalue required as left operand of assignment");
        }
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
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("|", $1, $3, &result_type);
        
        $$ = createNode(NODE_INCLUSIVE_OR_EXPRESSION, "|");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    ;

exclusive_or_expression:
    and_expression {
        $$ = $1;
    }
    | exclusive_or_expression BITWISE_XOR and_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("^", $1, $3, &result_type);
        
        $$ = createNode(NODE_EXCLUSIVE_OR_EXPRESSION, "^");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    ;

and_expression:
    equality_expression {
        $$ = $1;
    }
    | and_expression BITWISE_AND equality_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("&", $1, $3, &result_type);
        
        $$ = createNode(NODE_AND_EXPRESSION, "&");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    ;

equality_expression:
    relational_expression {
        $$ = $1;
    }
    | equality_expression EQ relational_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("==", $1, $3, &result_type);
        
        $$ = createNode(NODE_EQUALITY_EXPRESSION, "==");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    | equality_expression NE relational_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("!=", $1, $3, &result_type);
        
        $$ = createNode(NODE_EQUALITY_EXPRESSION, "!=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    ;

relational_expression:
    shift_expression {
        $$ = $1;
    }
    | relational_expression LT shift_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("<", $1, $3, &result_type);
        
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, "<");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    | relational_expression GT shift_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp(">", $1, $3, &result_type);
        
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, ">");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    | relational_expression LE shift_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("<=", $1, $3, &result_type);
        
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, "<=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    | relational_expression GE shift_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp(">=", $1, $3, &result_type);
        
        $$ = createNode(NODE_RELATIONAL_EXPRESSION, ">=");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    ;

shift_expression:
    additive_expression {
        $$ = $1;
    }
    | shift_expression LSHIFT additive_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("<<", $1, $3, &result_type);
        
        $$ = createNode(NODE_SHIFT_EXPRESSION, "<<");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    | shift_expression RSHIFT additive_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp(">>", $1, $3, &result_type);
        
        $$ = createNode(NODE_SHIFT_EXPRESSION, ">>");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    ;

additive_expression:
    multiplicative_expression {
        $$ = $1;
    }
    | additive_expression PLUS multiplicative_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("+", $1, $3, &result_type);
        
        if (result == TYPE_ERROR) {
            // Error already reported by checkBinaryOp
        }
        
        $$ = createNode(NODE_ADDITIVE_EXPRESSION, "+");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    | additive_expression MINUS multiplicative_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("-", $1, $3, &result_type);
        
        if (result == TYPE_ERROR) {
            // Error already reported by checkBinaryOp
        }

        $$ = createNode(NODE_ADDITIVE_EXPRESSION, "-");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    ;

multiplicative_expression:
    cast_expression {
        $$ = $1;
    }
    | multiplicative_expression MULTIPLY cast_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("*", $1, $3, &result_type);
        
        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "*");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    | multiplicative_expression DIVIDE cast_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("/", $1, $3, &result_type);
        
        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "/");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
    }
    | multiplicative_expression MODULO cast_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("%", $1, $3, &result_type);
        
        $$ = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "%");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
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
        
        // Extract the cast type from the type_name node
        char cast_type[256];
        strcpy(cast_type, currentType);
        
        // Check if abstract_declarator adds pointer levels
        if ($2->childCount > 1) {
            TreeNode* abs_decl = $2->children[1];
            int ptr_levels = countPointerLevels(abs_decl);
            for (int i = 0; i < ptr_levels; i++) {
                strcat(cast_type, " *");
            }
        }
        
        $$->dataType = strdup(cast_type);
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
        if (!isLValue($2)) {
            semantic_error("lvalue required as increment operand");
        }
        /* newTemp(), emit("ADD"), emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "++_pre");
        addChild($$, $2);
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    | DECREMENT unary_expression {
        if (!isLValue($2)) {
            semantic_error("lvalue required as decrement operand");
        }
        /* newTemp(), emit("SUB"), emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "--_pre");
        addChild($$, $2);
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    | BITWISE_AND cast_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkUnaryOp("&", $2, &result_type);
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "&");
        addChild($$, $2);
        $$->dataType = result_type;
    }
    | MULTIPLY cast_expression {
        char* result_type = NULL;
        TypeCheckResult result = checkUnaryOp("*", $2, &result_type);
        
        $$ = createNode(NODE_UNARY_EXPRESSION, "*");
        addChild($$, $2);
        $$->dataType = result_type;
        $$->isLValue = 1;
    }
    | PLUS cast_expression {
        $$ = $2;
    }
    | MINUS cast_expression {
        /* newTemp(), emit("NEG") REMOVED */
        $$ = createNode(NODE_UNARY_EXPRESSION, "-_unary");
        addChild($$, $2);
        $$->dataType = $2->dataType ? strdup($2->dataType) : NULL;
        
        // Compute the negated value if the operand is a constant
        if ($2->value) {
            int val = atoi($2->value);
            char negVal[32];
            snprintf(negVal, sizeof(negVal), "%d", -val);
            $$->value = strdup(negVal);
        }
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
        // Enhanced array access type checking
        char* result_type = NULL;
        TypeCheckResult result = checkArrayAccess($1, $3, &result_type);
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "[]");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
        $$->isLValue = 1;
    }
    | postfix_expression LPAREN RPAREN {
        // Enhanced function call type checking (no arguments)
        char* result_type = NULL;
        TypeCheckResult result = checkFunctionCall($1->value, NULL, &result_type);
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "()");
        addChild($$, $1);
        $$->dataType = result_type;
    }
    | postfix_expression LPAREN argument_expression_list RPAREN {
        // Enhanced function call type checking
        char* result_type = NULL;
        TypeCheckResult result = checkFunctionCall($1->value, $3, &result_type);
        
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "()");
        addChild($$, $1);
        addChild($$, $3);
        $$->dataType = result_type;
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
        if (!isLValue($1)) {
            semantic_error("lvalue required as increment operand");
        }
        /* newTemp(), emit("ASSIGN"), emit("ADD"), emit("ASSIGN") REMOVED */
        $$ = createNode(NODE_POSTFIX_EXPRESSION, "++_post");
        addChild($$, $1);
        $$->dataType = $1->dataType ? strdup($1->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
    | postfix_expression DECREMENT {
        if (!isLValue($1)) {
            semantic_error("lvalue required as decrement operand");
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
                // Undeclared identifier - report semantic error
                type_error(yylineno, "'%s' undeclared (first use in this function)", $1->value);
                $$->dataType = strdup("int"); // Assume int to continue parsing
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
        fprintf(stderr, "Syntax Error on line %d: %s", yylineno, msg);
        if (yytext && strlen(yytext) > 0) {
            fprintf(stderr, " at '%s'", yytext);
        }
        fprintf(stderr, "\n");
        error_count++;
        recovering_from_error = 1;
    }
}

void semantic_error(const char* msg) {
    fprintf(stderr, "Semantic Error on line %d: %s\n", yylineno, msg);
    semantic_error_count++;
    error_count++;
}
