/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 4 "../src/parser.y"

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
static int current_enum_value = 0;  // Track current enum constant value
int recovering_from_error = 0;
int in_typedef = 0;
int is_static = 0;  // Flag to track if current declaration is static
int has_const_before_ptr = 0;  // Flag for "const int*" (pointer to const)
int has_const_after_ptr = 0;   // Flag for "int* const" (const pointer)
int in_function_body = 0;  // Flag to track if we're in a function body compound statement
int param_count_temp = 0;  // Temporary counter for function parameters
int parsing_function_decl = 0;  // Flag to indicate we're parsing a function declaration
int in_function_definition = 0;  // Flag to indicate we're parsing a function definition (not just a declaration)
char function_return_type[128] = "int";  // Save function return type before parsing parameters
char saved_declaration_type[128] = "int";  // Save type from declaration_specifiers before declarator parsing
int loop_depth = 0;  // Track nesting depth of loops (for break/continue validation)
int switch_depth = 0;  // Track nesting depth of switch statements (for break validation)
int parsing_parameters = 0;  // Flag to indicate we're parsing function/function pointer parameters

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
    int isReference;
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
int extractArrayDimensions(TreeNode* node, int* dims, int maxDims);
int hasArrayBrackets(TreeNode* node);
const char* extractTypeFromSpecifiers(TreeNode* node);  // Forward declaration for function pointer type building

// Helper function to check if a node is an lvalue
int isLValue(TreeNode* node) {
    if (!node) return 0;
    
    // ERROR B: Check if this is a typedef name being used as lvalue
    if (node->type == NODE_TYPE_NAME) {
        // TYPE_NAME nodes represent typedef names - they are NOT lvalues
        return 0;
    }
    
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

// Helper function to get the referenced type from a reference type
// e.g., "int &" -> "int", "char &" -> "char"
char* stripReferenceType(const char* type) {
    if (!type) return NULL;
    
    // Look for " &" in the type string
    const char* ref_pos = strstr(type, " &");
    if (ref_pos) {
        // Create a copy without the " &" suffix
        size_t len = ref_pos - type;
        char* result = (char*)malloc(len + 1);
        strncpy(result, type, len);
        result[len] = '\0';
        return result;
    }
    
    // Not a reference type, return a copy
    return strdup(type);
}

// Helper function to check if a NODE_POINTER is a reference
int isReferenceNode(TreeNode* node) {
    if (!node) return 0;
    if (node->type == NODE_POINTER && node->value && strcmp(node->value, "&") == 0) {
        return 1;
    }
    return 0;
}

// Helper function to count pointer levels in a declarator tree
// References are treated as pointers for storage purposes
int countPointerLevels(TreeNode* node) {
    if (!node) return 0;
    if (node->type == NODE_POINTER) {
        // Count this pointer/reference and any nested pointers
        int count = 1;
        for (int i = 0; i < node->childCount; i++) {
            count += countPointerLevels(node->children[i]);
        }
        return count;
    }
    return 0;
}

// Helper function to count reference levels in a declarator tree
int countReferenceLevels(TreeNode* node) {
    if (!node) return 0;
    int count = 0;
    if (isReferenceNode(node)) {
        count = 1;
    }
    for (int i = 0; i < node->childCount; i++) {
        count += countReferenceLevels(node->children[i]);
    }
    return count;  // FIX: was returning 0 instead of count
}

// Helper function to build full type string with pointers and references
void buildFullType(char* dest, const char* baseType, int ptrLevel) {
    strcpy(dest, baseType);
    for (int i = 0; i < ptrLevel; i++) {
        strcat(dest, " *");
    }
}

// Helper function to build full type string including references
void buildFullTypeWithRefs(char* dest, const char* baseType, TreeNode* declarator) {
    strcpy(dest, baseType);
    
    // Walk the declarator tree and append * or & as appropriate
    TreeNode* current = declarator;
    while (current && current->type == NODE_POINTER) {
        if (isReferenceNode(current)) {
            strcat(dest, " &");
        } else {
            strcat(dest, " *");
        }
        // Move to child (the next declarator level)
        if (current->childCount > 0) {
            current = current->children[current->childCount - 1];  // Last child is usually the nested declarator
        } else {
            break;
        }
    }
}

// Helper function to extract identifier name from declarator
const char* extractIdentifierName(TreeNode* node) {
    if (!node) return NULL;
    if (node->type == NODE_IDENTIFIER) return node->value;
    // Also check for TYPE_NAME since we allow typedefs in declarator position
    if (node->type == NODE_TYPE_NAME) return node->value;
    
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
// Helper function to extract parameter types from a parameter list node
void extractParameterTypes(TreeNode* paramList, char* dest) {
    if (!paramList || paramList->type != NODE_PARAMETER_LIST) {
        return;
    }
    
    // Iterate through parameter children
    for (int i = 0; i < paramList->childCount; i++) {
        TreeNode* param = paramList->children[i];
        if (!param || param->type != NODE_PARAMETER_DECLARATION) continue;
        
        if (i > 0) {
            strcat(dest, ", ");
        }
        
        // Extract the type from declaration_specifiers (first child)
        if (param->childCount > 0) {
            TreeNode* declSpecs = param->children[0];
            const char* baseType = extractTypeFromSpecifiers(declSpecs);
            
            if (baseType) {
                strcat(dest, baseType);
                
                // Check if there's a declarator (second child) for pointers/references
                if (param->childCount > 1) {
                    TreeNode* declarator = param->children[1];
                    
                    // Check for pointers
                    int ptrCount = countPointerLevels(declarator);
                    for (int p = 0; p < ptrCount; p++) {
                        // Check if it's a reference or pointer
                        if (isReferenceNode(declarator)) {
                            strcat(dest, " &");
                            break;  // Only one reference allowed
                        } else {
                            strcat(dest, " *");
                        }
                    }
                    
                    // Check for references specifically
                    int refCount = countReferenceLevels(declarator);
                    if (refCount > 0 && ptrCount == 0) {
                        strcat(dest, " &");
                    }
                }
            }
        }
    }
}

// Helper function to find parameter list in a declarator tree
TreeNode* findParameterList(TreeNode* node) {
    if (!node) return NULL;
    if (node->type == NODE_PARAMETER_LIST) return node;
    
    // Recursively search children
    for (int i = 0; i < node->childCount; i++) {
        TreeNode* found = findParameterList(node->children[i]);
        if (found) return found;
    }
    return NULL;
}

void buildFunctionPointerType(char* dest, const char* returnType, TreeNode* declarator) {
    // Build complete function pointer type using pending_params
    // Format: "returnType (*)(param_types...)"
    strcpy(dest, returnType);
    strcat(dest, " (*)(");
    
    // Use the pending_params array that was populated during parsing
    for (int i = 0; i < pending_param_count; i++) {
        if (i > 0) {
            strcat(dest, ", ");
        }
        strcat(dest, pending_params[i].type);
    }
    
    strcat(dest, ")");
}

// Helper function to extract type from declaration_specifiers
const char* extractTypeFromSpecifiers(TreeNode* node) {
    if (!node) return NULL;
    
    // Check if this is a TYPE_NAME node (typedef name)
    if (node->type == NODE_TYPE_NAME && node->value) {
        return node->value;
    }
    
    // Check for built-in type names
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

// Helper function to evaluate simple constant expressions for enum values
int evaluateConstantExpression(TreeNode* expr) {
    if (!expr) return 0;
    
    // Handle integer constants
    if (expr->type == NODE_INTEGER_CONSTANT || expr->type == NODE_CONSTANT) {
        if (expr->value) {
            return atoi(expr->value);
        }
    }
    
    // Handle hex constants
    if (expr->type == NODE_HEX_CONSTANT && expr->value) {
        return (int)strtol(expr->value, NULL, 16);
    }
    
    // Handle octal constants
    if (expr->type == NODE_OCTAL_CONSTANT && expr->value) {
        return (int)strtol(expr->value, NULL, 8);
    }
    
    // If it's a wrapper node with one child, recurse
    if (expr->childCount == 1) {
        return evaluateConstantExpression(expr->children[0]);
    }
    
    return 0;
}

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
            // DON'T resolve typedef here - it may not be available yet
            // Typedef resolution will happen at member access time
            const char* actual_type = memberType;
            
            // Second child: struct_declarator_list (the names)
            TreeNode* declaratorList = declNode->children[1];
            
            // struct_declarator_list can be:
            // 1. A single declarator (IDENTIFIER or array/pointer declarator)
            // 2. Multiple declarators (first one + children are additional ones)
            
            // First, process the primary declarator (the node itself)
            const char* memberName = extractIdentifierName(declaratorList);
            if (memberName && *memberCount < maxMembers) {
                strncpy(members[*memberCount].name, memberName, 127);
                members[*memberCount].name[127] = '\0';
                
                // Check if it's an array and build the full type with dimensions
                char fullType[128];
                if (hasArrayBrackets(declaratorList)) {
                    int dims[10] = {0};
                    int numDims = extractArrayDimensions(declaratorList, dims, 10);
                    // Build type like "char[20]" or "int[10][20]"
                    strcpy(fullType, actual_type);
                    for (int d = 0; d < numDims; d++) {
                        char dimStr[32];
                        sprintf(dimStr, "[%d]", dims[d]);
                        strcat(fullType, dimStr);
                    }
                    strncpy(members[*memberCount].type, fullType, 127);
                } else {
                    // Check for pointer level in declarator
                    int ptrLevel = countPointerLevels(declaratorList);
                    if (ptrLevel > 0) {
                        strcpy(fullType, actual_type);
                        for (int p = 0; p < ptrLevel; p++) {
                            strcat(fullType, "*");
                        }
                        strncpy(members[*memberCount].type, fullType, 127);
                    } else {
                        strncpy(members[*memberCount].type, actual_type, 127);
                    }
                }
                members[*memberCount].type[127] = '\0';
                (*memberCount)++;
            }
            
            // Then, process any additional declarators in the list (siblings)
            // But NOT if this is an array/pointer declarator (those children are part of the declarator structure)
            int isComplexDeclarator = (declaratorList->type == NODE_DIRECT_DECLARATOR && 
                                      declaratorList->value && strcmp(declaratorList->value, "array") == 0) ||
                                     (declaratorList->type == NODE_POINTER);
            
            if (!isComplexDeclarator) {
                for (int j = 0; j < declaratorList->childCount && *memberCount < maxMembers; j++) {
                    TreeNode* declarator = declaratorList->children[j];
                    const char* additionalName = extractIdentifierName(declarator);
                    if (additionalName) {
                        strncpy(members[*memberCount].name, additionalName, 127);
                        members[*memberCount].name[127] = '\0';
                        
                        // Check if it's an array and build the full type with dimensions
                        char fullType[128];
                        if (hasArrayBrackets(declarator)) {
                            int dims[10] = {0};
                            int numDims = extractArrayDimensions(declarator, dims, 10);
                            // Build type like "char[20]" or "int[10][20]"
                            strcpy(fullType, actual_type);
                            for (int d = 0; d < numDims; d++) {
                                char dimStr[32];
                                sprintf(dimStr, "[%d]", dims[d]);
                                strcat(fullType, dimStr);
                            }
                            strncpy(members[*memberCount].type, fullType, 127);
                        } else {
                            // Check for pointer level in declarator
                            int ptrLevel = countPointerLevels(declarator);
                            if (ptrLevel > 0) {
                                strcpy(fullType, actual_type);
                                for (int p = 0; p < ptrLevel; p++) {
                                    strcat(fullType, "*");
                                }
                                strncpy(members[*memberCount].type, fullType, 127);
                            } else {
                                strncpy(members[*memberCount].type, actual_type, 127);
                            }
                        }
                        members[*memberCount].type[127] = '\0';
                        (*memberCount)++;
                    }
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


#line 723 "parser.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_IDENTIFIER = 3,                 /* IDENTIFIER  */
  YYSYMBOL_TYPE_NAME = 4,                  /* TYPE_NAME  */
  YYSYMBOL_INTEGER_CONSTANT = 5,           /* INTEGER_CONSTANT  */
  YYSYMBOL_HEX_CONSTANT = 6,               /* HEX_CONSTANT  */
  YYSYMBOL_OCTAL_CONSTANT = 7,             /* OCTAL_CONSTANT  */
  YYSYMBOL_BINARY_CONSTANT = 8,            /* BINARY_CONSTANT  */
  YYSYMBOL_FLOAT_CONSTANT = 9,             /* FLOAT_CONSTANT  */
  YYSYMBOL_STRING_LITERAL = 10,            /* STRING_LITERAL  */
  YYSYMBOL_CHAR_CONSTANT = 11,             /* CHAR_CONSTANT  */
  YYSYMBOL_PREPROCESSOR = 12,              /* PREPROCESSOR  */
  YYSYMBOL_IF = 13,                        /* IF  */
  YYSYMBOL_ELSE = 14,                      /* ELSE  */
  YYSYMBOL_WHILE = 15,                     /* WHILE  */
  YYSYMBOL_FOR = 16,                       /* FOR  */
  YYSYMBOL_DO = 17,                        /* DO  */
  YYSYMBOL_SWITCH = 18,                    /* SWITCH  */
  YYSYMBOL_CASE = 19,                      /* CASE  */
  YYSYMBOL_DEFAULT = 20,                   /* DEFAULT  */
  YYSYMBOL_BREAK = 21,                     /* BREAK  */
  YYSYMBOL_CONTINUE = 22,                  /* CONTINUE  */
  YYSYMBOL_RETURN = 23,                    /* RETURN  */
  YYSYMBOL_INT = 24,                       /* INT  */
  YYSYMBOL_CHAR_TOKEN = 25,                /* CHAR_TOKEN  */
  YYSYMBOL_FLOAT_TOKEN = 26,               /* FLOAT_TOKEN  */
  YYSYMBOL_DOUBLE = 27,                    /* DOUBLE  */
  YYSYMBOL_LONG = 28,                      /* LONG  */
  YYSYMBOL_SHORT = 29,                     /* SHORT  */
  YYSYMBOL_UNSIGNED = 30,                  /* UNSIGNED  */
  YYSYMBOL_SIGNED = 31,                    /* SIGNED  */
  YYSYMBOL_VOID = 32,                      /* VOID  */
  YYSYMBOL_BOOL = 33,                      /* BOOL  */
  YYSYMBOL_STRUCT = 34,                    /* STRUCT  */
  YYSYMBOL_ENUM = 35,                      /* ENUM  */
  YYSYMBOL_UNION = 36,                     /* UNION  */
  YYSYMBOL_TYPEDEF = 37,                   /* TYPEDEF  */
  YYSYMBOL_STATIC = 38,                    /* STATIC  */
  YYSYMBOL_EXTERN = 39,                    /* EXTERN  */
  YYSYMBOL_AUTO = 40,                      /* AUTO  */
  YYSYMBOL_REGISTER = 41,                  /* REGISTER  */
  YYSYMBOL_CONST = 42,                     /* CONST  */
  YYSYMBOL_VOLATILE = 43,                  /* VOLATILE  */
  YYSYMBOL_GOTO = 44,                      /* GOTO  */
  YYSYMBOL_UNTIL = 45,                     /* UNTIL  */
  YYSYMBOL_SIZEOF = 46,                    /* SIZEOF  */
  YYSYMBOL_ASSIGN = 47,                    /* ASSIGN  */
  YYSYMBOL_PLUS_ASSIGN = 48,               /* PLUS_ASSIGN  */
  YYSYMBOL_MINUS_ASSIGN = 49,              /* MINUS_ASSIGN  */
  YYSYMBOL_MUL_ASSIGN = 50,                /* MUL_ASSIGN  */
  YYSYMBOL_DIV_ASSIGN = 51,                /* DIV_ASSIGN  */
  YYSYMBOL_MOD_ASSIGN = 52,                /* MOD_ASSIGN  */
  YYSYMBOL_AND_ASSIGN = 53,                /* AND_ASSIGN  */
  YYSYMBOL_OR_ASSIGN = 54,                 /* OR_ASSIGN  */
  YYSYMBOL_XOR_ASSIGN = 55,                /* XOR_ASSIGN  */
  YYSYMBOL_LSHIFT_ASSIGN = 56,             /* LSHIFT_ASSIGN  */
  YYSYMBOL_RSHIFT_ASSIGN = 57,             /* RSHIFT_ASSIGN  */
  YYSYMBOL_EQ = 58,                        /* EQ  */
  YYSYMBOL_NE = 59,                        /* NE  */
  YYSYMBOL_LT = 60,                        /* LT  */
  YYSYMBOL_GT = 61,                        /* GT  */
  YYSYMBOL_LE = 62,                        /* LE  */
  YYSYMBOL_GE = 63,                        /* GE  */
  YYSYMBOL_LOGICAL_AND = 64,               /* LOGICAL_AND  */
  YYSYMBOL_LOGICAL_OR = 65,                /* LOGICAL_OR  */
  YYSYMBOL_LSHIFT = 66,                    /* LSHIFT  */
  YYSYMBOL_RSHIFT = 67,                    /* RSHIFT  */
  YYSYMBOL_INCREMENT = 68,                 /* INCREMENT  */
  YYSYMBOL_DECREMENT = 69,                 /* DECREMENT  */
  YYSYMBOL_ARROW = 70,                     /* ARROW  */
  YYSYMBOL_PLUS = 71,                      /* PLUS  */
  YYSYMBOL_MINUS = 72,                     /* MINUS  */
  YYSYMBOL_MULTIPLY = 73,                  /* MULTIPLY  */
  YYSYMBOL_DIVIDE = 74,                    /* DIVIDE  */
  YYSYMBOL_MODULO = 75,                    /* MODULO  */
  YYSYMBOL_BITWISE_AND = 76,               /* BITWISE_AND  */
  YYSYMBOL_BITWISE_OR = 77,                /* BITWISE_OR  */
  YYSYMBOL_BITWISE_XOR = 78,               /* BITWISE_XOR  */
  YYSYMBOL_BITWISE_NOT = 79,               /* BITWISE_NOT  */
  YYSYMBOL_LOGICAL_NOT = 80,               /* LOGICAL_NOT  */
  YYSYMBOL_QUESTION = 81,                  /* QUESTION  */
  YYSYMBOL_LPAREN = 82,                    /* LPAREN  */
  YYSYMBOL_RPAREN = 83,                    /* RPAREN  */
  YYSYMBOL_LBRACE = 84,                    /* LBRACE  */
  YYSYMBOL_RBRACE = 85,                    /* RBRACE  */
  YYSYMBOL_LBRACKET = 86,                  /* LBRACKET  */
  YYSYMBOL_RBRACKET = 87,                  /* RBRACKET  */
  YYSYMBOL_SEMICOLON = 88,                 /* SEMICOLON  */
  YYSYMBOL_COMMA = 89,                     /* COMMA  */
  YYSYMBOL_DOT = 90,                       /* DOT  */
  YYSYMBOL_COLON = 91,                     /* COLON  */
  YYSYMBOL_UNARY_MINUS = 92,               /* UNARY_MINUS  */
  YYSYMBOL_UNARY_PLUS = 93,                /* UNARY_PLUS  */
  YYSYMBOL_LOWER_THAN_ELSE = 94,           /* LOWER_THAN_ELSE  */
  YYSYMBOL_YYACCEPT = 95,                  /* $accept  */
  YYSYMBOL_program = 96,                   /* program  */
  YYSYMBOL_translation_unit = 97,          /* translation_unit  */
  YYSYMBOL_external_declaration = 98,      /* external_declaration  */
  YYSYMBOL_preprocessor_directive = 99,    /* preprocessor_directive  */
  YYSYMBOL_function_definition = 100,      /* function_definition  */
  YYSYMBOL_101_1 = 101,                    /* $@1  */
  YYSYMBOL_declaration = 102,              /* declaration  */
  YYSYMBOL_declaration_specifiers = 103,   /* declaration_specifiers  */
  YYSYMBOL_specifier = 104,                /* specifier  */
  YYSYMBOL_storage_class_specifier = 105,  /* storage_class_specifier  */
  YYSYMBOL_type_specifier = 106,           /* type_specifier  */
  YYSYMBOL_type_qualifier = 107,           /* type_qualifier  */
  YYSYMBOL_struct_or_union_specifier = 108, /* struct_or_union_specifier  */
  YYSYMBOL_struct_declaration_list = 109,  /* struct_declaration_list  */
  YYSYMBOL_struct_declaration = 110,       /* struct_declaration  */
  YYSYMBOL_struct_declarator_list = 111,   /* struct_declarator_list  */
  YYSYMBOL_struct_declarator = 112,        /* struct_declarator  */
  YYSYMBOL_enum_specifier = 113,           /* enum_specifier  */
  YYSYMBOL_enumerator_list = 114,          /* enumerator_list  */
  YYSYMBOL_enumerator = 115,               /* enumerator  */
  YYSYMBOL_init_declarator_list = 116,     /* init_declarator_list  */
  YYSYMBOL_init_declarator = 117,          /* init_declarator  */
  YYSYMBOL_initializer = 118,              /* initializer  */
  YYSYMBOL_initializer_list = 119,         /* initializer_list  */
  YYSYMBOL_declarator = 120,               /* declarator  */
  YYSYMBOL_direct_declarator = 121,        /* direct_declarator  */
  YYSYMBOL_122_2 = 122,                    /* $@2  */
  YYSYMBOL_type_qualifier_list = 123,      /* type_qualifier_list  */
  YYSYMBOL_parameter_type_list = 124,      /* parameter_type_list  */
  YYSYMBOL_parameter_list = 125,           /* parameter_list  */
  YYSYMBOL_parameter_declaration = 126,    /* parameter_declaration  */
  YYSYMBOL_abstract_declarator = 127,      /* abstract_declarator  */
  YYSYMBOL_direct_abstract_declarator = 128, /* direct_abstract_declarator  */
  YYSYMBOL_statement = 129,                /* statement  */
  YYSYMBOL_labeled_statement = 130,        /* labeled_statement  */
  YYSYMBOL_compound_statement = 131,       /* compound_statement  */
  YYSYMBOL_132_3 = 132,                    /* $@3  */
  YYSYMBOL_133_4 = 133,                    /* $@4  */
  YYSYMBOL_block_item_list = 134,          /* block_item_list  */
  YYSYMBOL_block_item = 135,               /* block_item  */
  YYSYMBOL_expression_statement = 136,     /* expression_statement  */
  YYSYMBOL_if_header = 137,                /* if_header  */
  YYSYMBOL_marker_while_start = 138,       /* marker_while_start  */
  YYSYMBOL_for_label_marker = 139,         /* for_label_marker  */
  YYSYMBOL_for_increment_expression = 140, /* for_increment_expression  */
  YYSYMBOL_selection_statement = 141,      /* selection_statement  */
  YYSYMBOL_142_5 = 142,                    /* $@5  */
  YYSYMBOL_143_6 = 143,                    /* $@6  */
  YYSYMBOL_do_header = 144,                /* do_header  */
  YYSYMBOL_do_trailer = 145,               /* do_trailer  */
  YYSYMBOL_iteration_statement = 146,      /* iteration_statement  */
  YYSYMBOL_147_7 = 147,                    /* $@7  */
  YYSYMBOL_148_8 = 148,                    /* $@8  */
  YYSYMBOL_149_9 = 149,                    /* $@9  */
  YYSYMBOL_150_10 = 150,                   /* $@10  */
  YYSYMBOL_jump_statement = 151,           /* jump_statement  */
  YYSYMBOL_expression = 152,               /* expression  */
  YYSYMBOL_assignment_expression = 153,    /* assignment_expression  */
  YYSYMBOL_conditional_expression = 154,   /* conditional_expression  */
  YYSYMBOL_constant_expression = 155,      /* constant_expression  */
  YYSYMBOL_logical_or_expression = 156,    /* logical_or_expression  */
  YYSYMBOL_logical_and_expression = 157,   /* logical_and_expression  */
  YYSYMBOL_inclusive_or_expression = 158,  /* inclusive_or_expression  */
  YYSYMBOL_exclusive_or_expression = 159,  /* exclusive_or_expression  */
  YYSYMBOL_and_expression = 160,           /* and_expression  */
  YYSYMBOL_equality_expression = 161,      /* equality_expression  */
  YYSYMBOL_relational_expression = 162,    /* relational_expression  */
  YYSYMBOL_shift_expression = 163,         /* shift_expression  */
  YYSYMBOL_additive_expression = 164,      /* additive_expression  */
  YYSYMBOL_multiplicative_expression = 165, /* multiplicative_expression  */
  YYSYMBOL_cast_expression = 166,          /* cast_expression  */
  YYSYMBOL_type_name = 167,                /* type_name  */
  YYSYMBOL_unary_expression = 168,         /* unary_expression  */
  YYSYMBOL_postfix_expression = 169,       /* postfix_expression  */
  YYSYMBOL_primary_expression = 170,       /* primary_expression  */
  YYSYMBOL_argument_expression_list = 171  /* argument_expression_list  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  133
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1996

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  95
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  77
/* YYNRULES -- Number of rules.  */
#define YYNRULES  234
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  403

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   349


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   750,   750,   761,   765,   772,   773,   774,   775,   779,
     830,   829,   898,   913,   927,   940,   955,   956,   957,   961,
     965,   966,   970,   971,   975,   979,   983,   987,   991,   995,
     999,  1003,  1007,  1011,  1015,  1016,  1017,  1024,  1028,  1032,
    1040,  1054,  1067,  1074,  1088,  1103,  1104,  1108,  1116,  1117,
    1121,  1122,  1123,  1127,  1133,  1138,  1148,  1149,  1153,  1166,
    1186,  1189,  1196,  1285,  1441,  1442,  1443,  1447,  1451,  1458,
    1459,  1464,  1471,  1479,  1480,  1485,  1486,  1490,  1514,  1519,
    1519,  1526,  1527,  1531,  1535,  1540,  1548,  1588,  1621,  1643,
    1647,  1651,  1652,  1653,  1654,  1655,  1659,  1660,  1661,  1662,
    1663,  1664,  1665,  1666,  1667,  1671,  1672,  1673,  1674,  1675,
    1676,  1680,  1689,  1695,  1703,  1703,  1715,  1715,  1730,  1734,
    1741,  1742,  1743,  1747,  1751,  1755,  1763,  1779,  1784,  1790,
    1791,  1795,  1802,  1802,  1813,  1813,  1827,  1835,  1850,  1865,
    1870,  1879,  1878,  1903,  1915,  1911,  1944,  1951,  1943,  1985,
    1996,  2004,  2012,  2016,  2024,  2027,  2036,  2039,  2049,  2058,
    2067,  2076,  2085,  2091,  2097,  2103,  2109,  2115,  2124,  2127,
    2139,  2145,  2148,  2159,  2162,  2173,  2176,  2188,  2191,  2203,
    2206,  2218,  2221,  2230,  2242,  2245,  2254,  2263,  2272,  2284,
    2287,  2296,  2308,  2311,  2324,  2340,  2343,  2352,  2361,  2373,
    2376,  2399,  2403,  2411,  2414,  2424,  2434,  2442,  2451,  2454,
    2475,  2482,  2494,  2501,  2511,  2514,  2525,  2534,  2544,  2555,
    2566,  2576,  2589,  2612,  2622,  2628,  2632,  2636,  2640,  2644,
    2648,  2652,  2679,  2685,  2690
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "IDENTIFIER",
  "TYPE_NAME", "INTEGER_CONSTANT", "HEX_CONSTANT", "OCTAL_CONSTANT",
  "BINARY_CONSTANT", "FLOAT_CONSTANT", "STRING_LITERAL", "CHAR_CONSTANT",
  "PREPROCESSOR", "IF", "ELSE", "WHILE", "FOR", "DO", "SWITCH", "CASE",
  "DEFAULT", "BREAK", "CONTINUE", "RETURN", "INT", "CHAR_TOKEN",
  "FLOAT_TOKEN", "DOUBLE", "LONG", "SHORT", "UNSIGNED", "SIGNED", "VOID",
  "BOOL", "STRUCT", "ENUM", "UNION", "TYPEDEF", "STATIC", "EXTERN", "AUTO",
  "REGISTER", "CONST", "VOLATILE", "GOTO", "UNTIL", "SIZEOF", "ASSIGN",
  "PLUS_ASSIGN", "MINUS_ASSIGN", "MUL_ASSIGN", "DIV_ASSIGN", "MOD_ASSIGN",
  "AND_ASSIGN", "OR_ASSIGN", "XOR_ASSIGN", "LSHIFT_ASSIGN",
  "RSHIFT_ASSIGN", "EQ", "NE", "LT", "GT", "LE", "GE", "LOGICAL_AND",
  "LOGICAL_OR", "LSHIFT", "RSHIFT", "INCREMENT", "DECREMENT", "ARROW",
  "PLUS", "MINUS", "MULTIPLY", "DIVIDE", "MODULO", "BITWISE_AND",
  "BITWISE_OR", "BITWISE_XOR", "BITWISE_NOT", "LOGICAL_NOT", "QUESTION",
  "LPAREN", "RPAREN", "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
  "SEMICOLON", "COMMA", "DOT", "COLON", "UNARY_MINUS", "UNARY_PLUS",
  "LOWER_THAN_ELSE", "$accept", "program", "translation_unit",
  "external_declaration", "preprocessor_directive", "function_definition",
  "$@1", "declaration", "declaration_specifiers", "specifier",
  "storage_class_specifier", "type_specifier", "type_qualifier",
  "struct_or_union_specifier", "struct_declaration_list",
  "struct_declaration", "struct_declarator_list", "struct_declarator",
  "enum_specifier", "enumerator_list", "enumerator",
  "init_declarator_list", "init_declarator", "initializer",
  "initializer_list", "declarator", "direct_declarator", "$@2",
  "type_qualifier_list", "parameter_type_list", "parameter_list",
  "parameter_declaration", "abstract_declarator",
  "direct_abstract_declarator", "statement", "labeled_statement",
  "compound_statement", "$@3", "$@4", "block_item_list", "block_item",
  "expression_statement", "if_header", "marker_while_start",
  "for_label_marker", "for_increment_expression", "selection_statement",
  "$@5", "$@6", "do_header", "do_trailer", "iteration_statement", "$@7",
  "$@8", "$@9", "$@10", "jump_statement", "expression",
  "assignment_expression", "conditional_expression", "constant_expression",
  "logical_or_expression", "logical_and_expression",
  "inclusive_or_expression", "exclusive_or_expression", "and_expression",
  "equality_expression", "relational_expression", "shift_expression",
  "additive_expression", "multiplicative_expression", "cast_expression",
  "type_name", "unary_expression", "postfix_expression",
  "primary_expression", "argument_expression_list", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-298)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-147)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     533,   -71,   -65,   900,  -298,  -298,  -298,  -298,  -298,  -298,
    -298,  -298,   -46,    -8,    25,  -298,    27,  1850,   -33,   -27,
      24,  1006,  -298,  -298,  -298,  -298,  -298,  -298,  -298,  -298,
    -298,  -298,     9,    11,    13,  -298,  -298,  -298,  -298,  -298,
    -298,  -298,   126,  1865,  1894,  1894,  1850,  1850,  1850,  1850,
    1850,  1850,  1750,    47,  -298,   144,   373,  -298,  -298,  -298,
    -298,   965,  -298,  -298,  -298,  -298,  -298,  -298,  -298,  -298,
    -298,  -298,   877,  -298,   877,  -298,  -298,    66,  -298,  -298,
     -26,    98,    87,    91,   119,   155,   199,   163,   162,    46,
    -298,   534,    45,   194,  -298,   877,  1850,  -298,   791,  1850,
    -298,  -298,  -298,   115,  -298,   877,  -298,  -298,  -298,   149,
     132,  1953,   137,   207,   139,  1953,   143,  1750,  -298,  1850,
    -298,  -298,  -298,  -298,  -298,  -298,  -298,  -298,  1349,   -59,
     152,   142,   705,  -298,  -298,  -298,   -28,   215,    84,    84,
    -298,  -298,   175,  -298,   -24,   -48,   225,     7,  -298,  1850,
    1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,
    1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,
    1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,  1850,
    -298,  -298,   239,  1669,  1850,   245,  -298,  -298,    21,  1850,
    -298,  1953,    28,   877,  -298,  -298,  1953,  -298,   444,  1413,
    -298,   207,   205,    61,  -298,  1953,  1453,  -298,   172,   -10,
      67,  1286,  1024,  -298,    70,  -298,  1850,  -298,  -298,  -298,
    -298,   619,  -298,  -298,  -298,  -298,   215,  -298,   182,  -298,
      84,  1537,   185,   193,  1052,  -298,   203,   204,  -298,  -298,
      98,     0,    87,    91,   119,   155,   199,   199,   163,   163,
     163,   163,   162,   162,    46,    46,  -298,  -298,  -298,  -298,
    -298,  -298,  -298,  -298,  -298,  -298,  -298,  -298,  -298,  -298,
    -298,  -298,  -298,    39,    72,  -298,  -298,    51,   420,  -298,
     965,  -298,  -298,  1493,  1850,   179,  -298,   196,  -298,  -298,
      82,  1850,  -298,   207,  1533,  -298,  -298,   -10,    70,    70,
    -298,  1223,   206,   201,  -298,   209,  -298,   211,  1809,  1137,
    -298,  -298,  -298,  -298,  -298,  -298,  -298,   246,  1537,  -298,
    -298,  -298,  -298,  1953,  -298,   213,   877,  1581,  1625,  1850,
    -298,  1850,  -298,  -298,  -298,   420,   877,  -298,  -298,  -298,
     135,  1850,  -298,  -298,  -298,  -298,    70,   237,   121,  1160,
    -298,  -298,  -298,  1953,  -298,  -298,  -298,   219,  -298,   216,
    -298,   120,   222,  -298,  -298,   229,    58,   231,    59,  -298,
    -298,   877,  1850,  -298,  -298,  -298,  -298,   237,  -298,  -298,
    -298,  -298,  1390,  -298,   208,   220,   228,   230,  -298,   234,
     232,  1850,  -298,  -298,  -298,  -298,  -298,  -298,   877,   242,
    -298,   877,  -298
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,   222,   223,   224,   225,   226,   227,   228,   230,
     229,     9,     0,     0,     0,   136,     0,     0,     0,     0,
       0,     0,    27,    25,    29,    30,    28,    26,    32,    31,
      24,    33,     0,     0,     0,    19,    21,    20,    22,    23,
      37,    38,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   116,   123,     0,     0,     3,     8,     5,
       6,     0,    14,    16,    17,    18,    34,    35,     7,   105,
     106,   107,     0,   108,     0,   109,   110,     0,   154,   156,
     168,   171,   173,   175,   177,   179,   181,   184,   189,   192,
     195,   199,   203,   214,   125,     0,     0,   127,     0,     0,
     222,   223,   170,     0,   199,     0,   151,   150,   152,     0,
      39,     0,    54,     0,    42,     0,     0,     0,   212,     0,
     204,   205,   208,   209,   207,   206,   210,   211,   201,     0,
       0,     0,     0,     1,     4,    73,    36,     0,     0,     0,
      12,    15,     0,    60,    62,    69,   131,     0,   124,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     220,   221,     0,     0,     0,     0,   231,   111,     0,     0,
     128,     0,     0,     0,   113,   153,     0,    36,     0,     0,
      45,     0,    58,     0,    56,     0,     0,   149,     0,    89,
      90,     0,     0,   202,    91,   232,     0,   115,   122,   120,
     121,     0,   118,    74,    81,    70,     0,    72,     0,    13,
       0,     0,     0,    79,     0,   132,     0,     0,   143,   155,
     172,     0,   174,   176,   178,   180,   182,   183,   185,   186,
     187,   188,   190,   191,   193,   194,   196,   197,   198,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     219,   216,   233,     0,     0,   218,   126,     0,     0,   128,
       0,   134,   112,     0,     0,     0,    48,    50,    40,    46,
       0,     0,    53,     0,     0,    43,   213,    92,    93,    95,
     101,    88,     0,    83,    84,     0,    97,     0,     0,     0,
     200,   117,   119,    82,    71,    75,    61,    62,     0,    63,
      64,    11,    78,     0,    76,     0,     0,     0,     0,     0,
     217,     0,   215,   141,   144,     0,     0,    41,    51,    47,
       0,     0,    55,    59,    57,    44,    94,    89,    90,     0,
      86,    87,   102,     0,    96,    98,   103,     0,    99,     0,
      67,     0,     0,    77,   133,     0,     0,     0,     0,   169,
     234,     0,   129,   147,   135,    49,    52,    92,    85,   104,
     100,    65,     0,    80,     0,     0,     0,     0,   142,     0,
     130,   129,    66,    68,   139,   137,   140,   138,     0,     0,
     145,     0,   148
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -298,  -298,  -298,   238,  -298,  -121,  -298,  -105,     1,   -55,
    -298,  -298,  -132,  -298,  -106,  -191,  -298,   -20,  -298,   125,
      29,  -298,   100,  -297,  -298,    -2,  -298,  -298,  -202,  -295,
    -298,   -19,  -110,  -104,   -70,  -298,   101,  -298,  -298,  -298,
     111,   -88,  -298,  -298,    56,   -54,  -298,  -298,  -298,  -298,
    -298,  -298,  -298,  -298,  -298,  -298,  -298,   -21,  -129,   -14,
    -183,  -298,   186,   187,   188,   189,   190,    94,   123,   109,
     110,    33,   223,    20,  -298,  -298,  -298
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    55,    56,    57,    58,    59,   232,    60,   198,    62,
      63,    64,    65,    66,   199,   200,   285,   286,    67,   203,
     204,   142,   143,   319,   361,   225,   145,   323,   226,   302,
     303,   304,   305,   214,    68,    69,    70,   131,   132,   221,
     222,    71,    72,   189,   278,   389,    73,   326,   336,    74,
     238,    75,   371,   372,   191,   391,    76,    77,    78,    79,
     103,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,   130,    91,    92,    93,   273
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     109,    61,   146,   102,   147,   224,   141,   297,   289,   206,
     190,   218,   110,   357,   112,   289,   114,    94,   213,   -74,
     239,   360,   236,   231,   215,   187,    95,   219,   362,   307,
     149,   129,    40,    41,   233,   194,    96,   104,   234,   150,
     259,   260,   261,   262,   263,   264,   265,   266,   267,   268,
     269,   325,   237,   128,   272,   151,   -74,    61,   105,   144,
     -10,   106,   220,   118,   120,   121,   104,   104,   104,   104,
     104,   104,   211,   141,    97,   188,   212,   224,   192,   122,
     123,   124,   125,   126,   127,   393,   279,   135,   223,   149,
     283,   329,   289,   111,   313,   113,   129,   115,   129,   294,
     218,   338,   320,   289,   276,   298,   299,    98,   343,    99,
     149,   281,   107,   180,   181,   182,   219,   149,   128,   166,
     167,   168,   330,   282,   135,   223,   359,   183,   331,   116,
     241,   184,  -114,    61,   333,   185,   227,   228,   135,   223,
     149,   385,   387,   141,   133,   377,   292,   149,   149,   211,
     293,   220,   308,   212,   148,   149,   309,   137,   376,   332,
     138,   149,   152,   274,   153,   313,   139,   342,   277,   154,
     104,   293,   104,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   320,
     334,   351,   280,   346,   137,   155,   287,   138,   102,   256,
     257,   258,   370,   349,   186,   381,   193,   212,   137,   382,
     202,   138,   301,   156,   157,   224,   196,   139,   135,   223,
     102,   201,    61,   205,   314,   141,   284,   217,   317,   162,
     163,   207,   104,   164,   165,   216,   104,   195,   149,   235,
     135,   223,   270,   298,   299,   313,   141,   373,   275,   310,
     246,   247,   291,   320,   104,   296,   364,    40,    41,   158,
     159,   160,   161,   229,   230,   315,   374,   339,   340,    53,
     102,   252,   253,   346,   254,   255,   322,   102,   317,    40,
      41,   248,   249,   250,   251,   327,   328,   341,   137,   352,
     353,   138,   354,   231,   134,   102,   394,   139,   355,   350,
     363,   388,   379,   380,   104,   383,   366,   368,   395,   301,
     137,   104,   384,   138,   386,   369,   396,   398,   397,   349,
     375,   149,   344,   212,   301,   401,   290,   102,   400,   104,
     316,   402,   312,   321,   378,   335,   240,   399,   287,   242,
     208,   243,     0,   244,     0,   245,   227,   228,     0,   104,
     301,   390,     0,     0,   301,     0,     0,     0,     0,     0,
       0,   104,     0,     0,     0,     0,     0,     0,     0,     0,
     390,     0,     0,    -2,     1,   314,     2,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,     0,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,     0,    43,
       0,     1,     0,   100,   101,     4,     5,     6,     7,     8,
       9,    10,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    44,    45,     0,    46,    47,    48,   135,   136,    49,
       0,     0,    50,    51,     0,    52,     0,    53,     0,     0,
       0,    54,     0,     0,     0,     0,    43,     0,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    44,    45,
       0,    46,    47,    48,     0,     0,    49,     0,     0,    50,
      51,     0,    52,     0,     0,     0,     0,     0,    54,     0,
       0,     0,     0,     0,     0,     0,     0,   137,     0,     0,
     138,     0,     0,     0,     0,     0,   139,     0,     0,     0,
       0,     0,     0,     0,     1,   284,     2,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,     0,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,     0,    43,
       0,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    44,    45,     0,    46,    47,    48,     0,     0,    49,
       0,     0,    50,    51,     0,    52,     0,    53,     0,     0,
       1,    54,     2,     3,     4,     5,     6,     7,     8,     9,
      10,     0,    12,     0,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,     0,    43,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    44,    45,     0,
      46,    47,    48,     0,     0,    49,     0,     0,    50,    51,
       0,    52,     0,    53,   311,     0,     1,    54,     2,     3,
       4,     5,     6,     7,     8,     9,    10,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
       0,    43,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    44,    45,     0,    46,    47,    48,     0,
       0,    49,     0,     0,    50,    51,     0,    52,     0,    53,
       0,     0,     1,    54,   100,   101,     4,     5,     6,     7,
       8,     9,    10,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -146,  -146,  -146,  -146,  -146,
    -146,  -146,  -146,  -146,  -146,  -146,  -146,  -146,  -146,  -146,
    -146,  -146,  -146,  -146,  -146,     0,     0,    43,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    44,
      45,     0,    46,    47,    48,     0,     0,    49,     0,     0,
      50,    51,     0,    52,     0,     0,     0,     0,     1,    54,
       2,   101,     4,     5,     6,     7,     8,     9,    10,     0,
      12,     0,    13,    14,    15,    16,    17,    18,    19,    20,
      21,     0,     0,   -36,   -36,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    42,     0,    43,   -36,   -36,   -36,   -36,   -36,   -36,
     -36,   -36,   -36,   -36,   -36,   -36,   -36,   -36,   -36,   -36,
     -36,   -36,   -36,   -36,     0,    44,    45,     0,    46,    47,
      48,     0,     0,    49,     0,     0,    50,    51,     0,    52,
       0,    53,     0,     0,     0,    54,     0,     0,   135,   136,
       0,     0,     0,   -36,     0,     0,   -36,     0,     0,     0,
       0,     0,   -36,   -36,     0,     0,   -36,     0,   -36,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,   100,
     101,     4,     5,     6,     7,     8,     9,    10,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   100,   101,     4,
       5,     6,     7,     8,     9,    10,     0,     0,   137,     0,
       0,   138,     0,     0,     0,     0,     0,   139,     0,     0,
       0,     0,    43,   140,     0,   100,   101,     4,     5,     6,
       7,     8,     9,    10,     0,     0,     0,     0,     0,     0,
      43,     0,     0,     0,    44,    45,     0,    46,    47,    48,
       0,     0,    49,     0,     0,    50,    51,     0,    52,     0,
       0,     0,    44,    45,   108,    46,    47,    48,    43,     0,
      49,     0,     0,    50,    51,     0,    52,     0,     0,     0,
       0,   306,     0,     0,     0,     0,     0,     0,     0,     0,
      44,    45,     0,    46,    47,    48,     0,     0,    49,     0,
       0,    50,    51,     0,    52,     0,     0,     0,     0,   324,
     100,   101,     4,     5,     6,     7,     8,     9,    10,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   135,   136,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    43,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    44,    45,     0,    46,    47,
      48,     0,     0,    49,     0,     0,    50,    51,     0,    52,
       0,     0,     0,     0,   358,     0,   135,   136,     0,     0,
       0,     0,     0,   347,     0,     0,   348,     0,     0,     0,
       0,     0,   349,   300,     0,     0,   212,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     197,     0,     0,     0,     0,     0,   347,     0,     0,   348,
       0,     0,     0,     0,     0,   349,     0,     0,     0,   212,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   197,     0,     0,     0,     0,     0,   209,
       0,     0,   210,     0,     0,     0,     0,     0,   211,   300,
       0,     0,   212,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,   100,   101,     4,     5,     6,     7,     8,
       9,    10,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   197,     0,     0,
       0,     0,   209,     0,     0,   210,     0,     0,     0,     0,
       0,   211,     0,     0,     0,   212,    43,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,   197,    44,    45,
       0,    46,    47,    48,     0,     0,    49,     0,     0,    50,
      51,     0,    52,     0,   318,   392,     0,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,   197,   288,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,   197,   295,     0,
     100,   101,     4,     5,     6,     7,     8,     9,    10,     0,
       0,     0,     0,     0,     0,     0,     0,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,   337,     0,
       0,     0,   365,    43,   100,   101,     4,     5,     6,     7,
       8,     9,    10,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    44,    45,     0,    46,    47,
      48,     0,     0,    49,     0,     0,    50,    51,   345,    52,
       0,   318,     0,     0,     0,     0,   367,    43,   100,   101,
       4,     5,     6,     7,     8,     9,    10,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    44,
      45,     0,    46,    47,    48,     0,     0,    49,     0,     0,
      50,    51,     0,    52,     0,     0,     0,     0,     0,     0,
       0,    43,   100,   101,     4,     5,     6,     7,     8,     9,
      10,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    44,    45,     0,    46,    47,    48,     0,
       0,    49,     0,     0,    50,    51,     0,    52,     0,     0,
       0,     0,     0,     0,     0,    43,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    44,    45,     0,
      46,    47,    48,     0,     0,    49,     0,     0,    50,    51,
       0,    52,   271,   100,     3,     4,     5,     6,     7,     8,
       9,    10,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,     0,    43,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   197,     0,     0,     0,     0,    44,    45,
       0,    46,    47,    48,     0,     0,    49,     0,     0,    50,
      51,     0,    52,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,   100,   101,     4,     5,     6,     7,     8,
       9,    10,     0,     0,     0,     0,     0,     0,   100,   101,
       4,     5,     6,     7,     8,     9,    10,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   356,     0,     0,     0,    43,   100,   101,     4,
       5,     6,     7,     8,     9,    10,     0,     0,     0,     0,
       0,    43,     0,     0,     0,     0,     0,     0,    44,    45,
       0,    46,    47,    48,     0,     0,    49,     0,     0,    50,
      51,     0,    52,    44,    45,     0,    46,    47,    48,     0,
      43,    49,     0,     0,    50,    51,     0,   117,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   197,     0,     0,
       0,     0,    44,    45,     0,    46,    47,    48,     0,     0,
      49,     0,     0,    50,    51,     0,   119,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41
};

static const yytype_int16 yycheck[] =
{
      21,     0,    72,    17,    74,   137,    61,   209,   199,   115,
      98,   132,     3,   308,     3,   206,     3,    88,   128,    47,
     149,   318,    15,    47,    83,    95,    91,   132,   323,   212,
      89,    52,    42,    43,    82,   105,    82,    17,    86,    65,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   234,    45,    52,   183,    81,    84,    56,    91,    61,
      84,    88,   132,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    82,   128,    82,    96,    86,   209,    99,    46,
      47,    48,    49,    50,    51,   382,   191,     3,     4,    89,
     196,    91,   283,    84,   226,    84,   117,    84,   119,   205,
     221,   284,   231,   294,    83,   209,   210,    82,   291,    82,
      89,    83,    88,    68,    69,    70,   221,    89,   117,    73,
      74,    75,    83,   193,     3,     4,   309,    82,    89,     3,
     151,    86,    85,   132,    83,    90,   138,   139,     3,     4,
      89,    83,    83,   198,     0,   347,    85,    89,    89,    82,
      89,   221,    82,    86,    88,    89,    86,    73,   341,    87,
      76,    89,    64,   184,    77,   297,    82,    85,   189,    78,
     150,    89,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   318,
     278,   301,   191,   297,    73,    76,   198,    76,   212,   166,
     167,   168,   331,    82,    10,    85,    91,    86,    73,    89,
       3,    76,   211,    58,    59,   347,    84,    82,     3,     4,
     234,    84,   221,    84,   226,   280,    91,    85,   230,    66,
      67,    88,   212,    71,    72,    83,   216,    88,    89,    14,
       3,     4,     3,   347,   348,   377,   301,   335,     3,   216,
     156,   157,    47,   382,   234,    83,   326,    42,    43,    60,
      61,    62,    63,    88,    89,    83,   336,    88,    89,    84,
     284,   162,   163,   377,   164,   165,    83,   291,   280,    42,
      43,   158,   159,   160,   161,    82,    82,    91,    73,    83,
      89,    76,    83,    47,    56,   309,    88,    82,    87,   301,
      87,   371,    83,    87,   284,    83,   327,   328,    88,   308,
      73,   291,    83,    76,    83,   329,    88,    83,    88,    82,
     340,    89,   293,    86,   323,    83,   201,   341,   398,   309,
     230,   401,   221,   232,   353,   279,   150,   391,   340,   152,
     117,   153,    -1,   154,    -1,   155,   348,   349,    -1,   329,
     349,   372,    -1,    -1,   353,    -1,    -1,    -1,    -1,    -1,
      -1,   341,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     391,    -1,    -1,     0,     1,   377,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    -1,    46,
      -1,     1,    -1,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    -1,    71,    72,    73,     3,     4,    76,
      -1,    -1,    79,    80,    -1,    82,    -1,    84,    -1,    -1,
      -1,    88,    -1,    -1,    -1,    -1,    46,    -1,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    68,    69,
      -1,    71,    72,    73,    -1,    -1,    76,    -1,    -1,    79,
      80,    -1,    82,    -1,    -1,    -1,    -1,    -1,    88,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,
      76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     1,    91,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    -1,    46,
      -1,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    -1,    71,    72,    73,    -1,    -1,    76,
      -1,    -1,    79,    80,    -1,    82,    -1,    84,    -1,    -1,
       1,    88,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    -1,    13,    -1,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    -1,    46,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    -1,
      71,    72,    73,    -1,    -1,    76,    -1,    -1,    79,    80,
      -1,    82,    -1,    84,    85,    -1,     1,    88,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    -1,    13,    -1,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      -1,    46,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    68,    69,    -1,    71,    72,    73,    -1,
      -1,    76,    -1,    -1,    79,    80,    -1,    82,    -1,    84,
      -1,    -1,     1,    88,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    -1,    -1,    46,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,
      69,    -1,    71,    72,    73,    -1,    -1,    76,    -1,    -1,
      79,    80,    -1,    82,    -1,    -1,    -1,    -1,     1,    88,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    -1,
      13,    -1,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    -1,    -1,     3,     4,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    44,    -1,    46,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    -1,    68,    69,    -1,    71,    72,
      73,    -1,    -1,    76,    -1,    -1,    79,    80,    -1,    82,
      -1,    84,    -1,    -1,    -1,    88,    -1,    -1,     3,     4,
      -1,    -1,    -1,    73,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    -1,    82,    83,    -1,    -1,    86,    -1,    88,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    -1,    -1,    73,    -1,
      -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
      -1,    -1,    46,    88,    -1,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    -1,    -1,    -1,    68,    69,    -1,    71,    72,    73,
      -1,    -1,    76,    -1,    -1,    79,    80,    -1,    82,    -1,
      -1,    -1,    68,    69,    88,    71,    72,    73,    46,    -1,
      76,    -1,    -1,    79,    80,    -1,    82,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      68,    69,    -1,    71,    72,    73,    -1,    -1,    76,    -1,
      -1,    79,    80,    -1,    82,    -1,    -1,    -1,    -1,    87,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    -1,    68,    69,    -1,    71,    72,
      73,    -1,    -1,    76,    -1,    -1,    79,    80,    -1,    82,
      -1,    -1,    -1,    -1,    87,    -1,     3,     4,    -1,    -1,
      -1,    -1,    -1,    73,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    -1,    82,    83,    -1,    -1,    86,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       4,    -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,    76,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    -1,    86,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     4,    -1,    -1,    -1,    -1,    -1,    73,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    83,
      -1,    -1,    86,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,    -1,    -1,
      -1,    -1,    73,    -1,    -1,    76,    -1,    -1,    -1,    -1,
      -1,    82,    -1,    -1,    -1,    86,    46,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,     4,    68,    69,
      -1,    71,    72,    73,    -1,    -1,    76,    -1,    -1,    79,
      80,    -1,    82,    -1,    84,    85,    -1,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,     4,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,     4,    85,    -1,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    -1,    85,    -1,
      -1,    -1,     1,    46,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    68,    69,    -1,    71,    72,
      73,    -1,    -1,    76,    -1,    -1,    79,    80,    85,    82,
      -1,    84,    -1,    -1,    -1,    -1,     1,    46,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,
      69,    -1,    71,    72,    73,    -1,    -1,    76,    -1,    -1,
      79,    80,    -1,    82,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    46,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    68,    69,    -1,    71,    72,    73,    -1,
      -1,    76,    -1,    -1,    79,    80,    -1,    82,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    -1,
      71,    72,    73,    -1,    -1,    76,    -1,    -1,    79,    80,
      -1,    82,    83,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    -1,    -1,    46,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     4,    -1,    -1,    -1,    -1,    68,    69,
      -1,    71,    72,    73,    -1,    -1,    76,    -1,    -1,    79,
      80,    -1,    82,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    83,    -1,    -1,    -1,    46,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    -1,    -1,    -1,    -1,
      -1,    46,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,
      -1,    71,    72,    73,    -1,    -1,    76,    -1,    -1,    79,
      80,    -1,    82,    68,    69,    -1,    71,    72,    73,    -1,
      46,    76,    -1,    -1,    79,    80,    -1,    82,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,    -1,    -1,
      -1,    -1,    68,    69,    -1,    71,    72,    73,    -1,    -1,
      76,    -1,    -1,    79,    80,    -1,    82,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    46,    68,    69,    71,    72,    73,    76,
      79,    80,    82,    84,    88,    96,    97,    98,    99,   100,
     102,   103,   104,   105,   106,   107,   108,   113,   129,   130,
     131,   136,   137,   141,   144,   146,   151,   152,   153,   154,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   168,   169,   170,    88,    91,    82,    82,    82,    82,
       3,     4,   154,   155,   168,    91,    88,    88,    88,   152,
       3,    84,     3,    84,     3,    84,     3,    82,   168,    82,
     168,   168,   166,   166,   166,   166,   166,   166,   103,   152,
     167,   132,   133,     0,    98,     3,     4,    73,    76,    82,
      88,   104,   116,   117,   120,   121,   129,   129,    88,    89,
      65,    81,    64,    77,    78,    76,    58,    59,    60,    61,
      62,    63,    66,    67,    71,    72,    73,    74,    75,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      68,    69,    70,    82,    86,    90,    10,   129,   152,   138,
     136,   149,   152,    91,   129,    88,    84,     4,   103,   109,
     110,    84,     3,   114,   115,    84,   109,    88,   167,    73,
      76,    82,    86,   127,   128,    83,    83,    85,   100,   102,
     129,   134,   135,     4,   107,   120,   123,   120,   120,    88,
      89,    47,   101,    82,    86,    14,    15,    45,   145,   153,
     157,   152,   158,   159,   160,   161,   162,   162,   163,   163,
     163,   163,   164,   164,   165,   165,   166,   166,   166,   153,
     153,   153,   153,   153,   153,   153,   153,   153,   153,   153,
       3,    83,   153,   171,   152,     3,    83,   152,   139,   102,
     103,    83,   129,   109,    91,   111,   112,   120,    85,   110,
     114,    47,    85,    89,   109,    85,    83,   123,   128,   128,
      83,   103,   124,   125,   126,   127,    87,   155,    82,    86,
     166,    85,   135,   107,   120,    83,   117,   120,    84,   118,
     153,   131,    83,   122,    87,   155,   142,    82,    82,    91,
      83,    89,    87,    83,   136,   139,   143,    85,   155,    88,
      89,    91,    85,   155,   115,    85,   128,    73,    76,    82,
     120,   127,    83,    89,    83,    87,    83,   124,    87,   155,
     118,   119,   124,    87,   129,     1,   152,     1,   152,   154,
     153,   147,   148,   136,   129,   112,   155,   123,   126,    83,
      87,    85,    89,    83,    83,    83,    83,    83,   129,   140,
     152,   150,    85,   118,    88,    88,    88,    88,    83,   140,
     129,    83,   129
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    95,    96,    97,    97,    98,    98,    98,    98,    99,
     101,   100,   102,   102,   103,   103,   104,   104,   104,   105,
     105,   105,   105,   105,   106,   106,   106,   106,   106,   106,
     106,   106,   106,   106,   106,   106,   106,   107,   107,   108,
     108,   108,   108,   108,   108,   109,   109,   110,   111,   111,
     112,   112,   112,   113,   113,   113,   114,   114,   115,   115,
     116,   116,   117,   117,   118,   118,   118,   119,   119,   120,
     120,   120,   120,   121,   121,   121,   121,   121,   121,   122,
     121,   123,   123,   124,   125,   125,   126,   126,   126,   127,
     127,   127,   127,   127,   127,   127,   128,   128,   128,   128,
     128,   128,   128,   128,   128,   129,   129,   129,   129,   129,
     129,   130,   130,   130,   132,   131,   133,   131,   134,   134,
     135,   135,   135,   136,   136,   136,   137,   138,   139,   140,
     140,   141,   142,   141,   143,   141,   144,   145,   145,   145,
     145,   147,   146,   146,   148,   146,   149,   150,   146,   151,
     151,   151,   151,   151,   152,   152,   153,   153,   153,   153,
     153,   153,   153,   153,   153,   153,   153,   153,   154,   154,
     155,   156,   156,   157,   157,   158,   158,   159,   159,   160,
     160,   161,   161,   161,   162,   162,   162,   162,   162,   163,
     163,   163,   164,   164,   164,   165,   165,   165,   165,   166,
     166,   167,   167,   168,   168,   168,   168,   168,   168,   168,
     168,   168,   168,   168,   169,   169,   169,   169,   169,   169,
     169,   169,   170,   170,   170,   170,   170,   170,   170,   170,
     170,   170,   170,   171,   171
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     2,     1,     1,     1,     1,     1,
       0,     4,     2,     3,     1,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       4,     5,     2,     4,     5,     1,     2,     3,     1,     3,
       1,     2,     3,     4,     2,     5,     1,     3,     1,     3,
       1,     3,     1,     3,     1,     3,     4,     1,     3,     1,
       2,     3,     2,     1,     1,     3,     3,     4,     3,     0,
       5,     1,     2,     1,     1,     3,     2,     2,     1,     1,
       1,     1,     2,     2,     3,     2,     3,     2,     3,     3,
       4,     2,     3,     3,     4,     1,     1,     1,     1,     1,
       1,     3,     4,     3,     0,     3,     0,     4,     1,     2,
       1,     1,     1,     1,     2,     2,     4,     0,     0,     0,
       1,     2,     0,     5,     0,     6,     1,     5,     5,     5,
       5,     0,     7,     3,     0,     9,     0,     0,    10,     3,
       2,     2,     2,     3,     1,     3,     1,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     1,     5,
       1,     1,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     3,     3,     1,     3,     3,     3,     3,     1,
       3,     3,     1,     3,     3,     1,     3,     3,     3,     1,
       4,     1,     2,     1,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     4,     1,     4,     3,     4,     3,     3,
       2,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     3,     1,     3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* program: translation_unit  */
#line 750 "../src/parser.y"
                     {
        (yyval.node) = createNode(NODE_PROGRAM, "program");
        addChild((yyval.node), (yyvsp[0].node));
        ast_root = (yyval.node); // Set the global AST root
        if (error_count == 0) {
            printf("\n=== PARSING COMPLETED SUCCESSFULLY ===\n");
        }
    }
#line 2522 "parser.tab.c"
    break;

  case 3: /* translation_unit: external_declaration  */
#line 761 "../src/parser.y"
                         {
        (yyval.node) = createNode(NODE_PROGRAM, "translation_unit");
        if ((yyvsp[0].node)) addChild((yyval.node), (yyvsp[0].node));
    }
#line 2531 "parser.tab.c"
    break;

  case 4: /* translation_unit: translation_unit external_declaration  */
#line 765 "../src/parser.y"
                                            {
        (yyval.node) = (yyvsp[-1].node);
        if ((yyvsp[0].node)) addChild((yyval.node), (yyvsp[0].node));
    }
#line 2540 "parser.tab.c"
    break;

  case 5: /* external_declaration: function_definition  */
#line 772 "../src/parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2546 "parser.tab.c"
    break;

  case 6: /* external_declaration: declaration  */
#line 773 "../src/parser.y"
                  { (yyval.node) = (yyvsp[0].node); }
#line 2552 "parser.tab.c"
    break;

  case 7: /* external_declaration: statement  */
#line 774 "../src/parser.y"
                { (yyval.node) = (yyvsp[0].node); }
#line 2558 "parser.tab.c"
    break;

  case 8: /* external_declaration: preprocessor_directive  */
#line 775 "../src/parser.y"
                             { (yyval.node) = NULL; }
#line 2564 "parser.tab.c"
    break;

  case 9: /* preprocessor_directive: PREPROCESSOR  */
#line 779 "../src/parser.y"
                 { 
        // Check if this is #include <stdio.h> and insert standard library functions
        if ((yyvsp[0].node) && (yyvsp[0].node)->value && strstr((yyvsp[0].node)->value, "stdio.h")) {
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
        if ((yyvsp[0].node) && (yyvsp[0].node)->value && strstr((yyvsp[0].node)->value, "stdlib.h")) {
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
        (yyval.node) = NULL; 
    }
#line 2616 "parser.tab.c"
    break;

  case 10: /* $@1: %empty  */
#line 830 "../src/parser.y"
    {
        // Now we know it's a definition (has compound_statement coming)
        // Extract function name
        const char* funcName = extractIdentifierName((yyvsp[0].node));
        if (funcName) {
            // Extract return type from declaration_specifiers
            char funcRetType[128];
            strcpy(funcRetType, currentType);
            
            // Try to find the base type from declaration_specifiers
            if ((yyvsp[-1].node) && (yyvsp[-1].node)->childCount > 0) {
                for (int i = 0; i < (yyvsp[-1].node)->childCount; i++) {
                    TreeNode* spec = (yyvsp[-1].node)->children[i];
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
            insertVariable(pending_params[i].name, pending_params[i].type, 0, NULL, 0, pending_params[i].ptrLevel, 0, 0, 0, pending_params[i].isReference);
        }
        // Mark them as parameters
        markRecentSymbolsAsParameters(pending_param_count);
        param_count_temp = pending_param_count;  // Update param_count_temp for IR generation
        pending_param_count = 0;  // Reset for next function
        
        in_function_body = 1;
    }
#line 2664 "parser.tab.c"
    break;

  case 11: /* function_definition: declaration_specifiers declarator $@1 compound_statement  */
#line 874 "../src/parser.y"
    {
        (yyval.node) = createNode(NODE_FUNCTION_DEFINITION, "function");
        addChild((yyval.node), (yyvsp[-3].node)); // declaration_specifiers
        addChild((yyval.node), (yyvsp[-2].node)); // declarator (name)
        addChild((yyval.node), (yyvsp[0].node)); // compound_statement (body)
        
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
#line 2690 "parser.tab.c"
    break;

  case 12: /* declaration: declaration_specifiers SEMICOLON  */
#line 898 "../src/parser.y"
                                     {
        // ERROR C: Check for typedef with missing declarator
        if (in_typedef) {
            type_error(yylineno, "typedef declaration does not declare anything");
        }
        
        (yyval.node) = createNode(NODE_DECLARATION, "declaration");
        addChild((yyval.node), (yyvsp[-1].node));
        in_typedef = 0;
        is_static = 0;
        has_const_before_ptr = 0;
        has_const_after_ptr = 0;
        recovering_from_error = 0;
        pending_param_count = 0;  // Reset pending parameters (in case this was a function declaration)
    }
#line 2710 "parser.tab.c"
    break;

  case 13: /* declaration: declaration_specifiers init_declarator_list SEMICOLON  */
#line 913 "../src/parser.y"
                                                            {
        (yyval.node) = createNode(NODE_DECLARATION, "declaration");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[-1].node));
        in_typedef = 0;
        is_static = 0;
        has_const_before_ptr = 0;
        has_const_after_ptr = 0;
        recovering_from_error = 0;
        pending_param_count = 0;  // Reset pending parameters (in case this was a function declaration)
    }
#line 2726 "parser.tab.c"
    break;

  case 14: /* declaration_specifiers: specifier  */
#line 927 "../src/parser.y"
              {
        (yyval.node) = createNode(NODE_DECLARATION_SPECIFIERS, "decl_specs");
        addChild((yyval.node), (yyvsp[0].node));
        // Don't overwrite function return type if we're inside parameter parsing
        // parsing_function_decl will be set by function_definition rule
        // Store the current type in the node's dataType field AND save it globally
        (yyval.node)->dataType = strdup(currentType);
        // Only save to saved_declaration_type if we're NOT parsing function pointer parameters
        if (!parsing_parameters) {
            strncpy(saved_declaration_type, currentType, sizeof(saved_declaration_type) - 1);
            saved_declaration_type[sizeof(saved_declaration_type) - 1] = '\0';
        }
    }
#line 2744 "parser.tab.c"
    break;

  case 15: /* declaration_specifiers: declaration_specifiers specifier  */
#line 940 "../src/parser.y"
                                       {
        (yyval.node) = (yyvsp[-1].node);
        addChild((yyval.node), (yyvsp[0].node));
        // Update the dataType field with the current type AND save it globally
        if ((yyval.node)->dataType) free((yyval.node)->dataType);
        (yyval.node)->dataType = strdup(currentType);
        // Only save to saved_declaration_type if we're NOT parsing function pointer parameters
        if (!parsing_parameters) {
            strncpy(saved_declaration_type, currentType, sizeof(saved_declaration_type) - 1);
            saved_declaration_type[sizeof(saved_declaration_type) - 1] = '\0';
        }
    }
#line 2761 "parser.tab.c"
    break;

  case 16: /* specifier: storage_class_specifier  */
#line 955 "../src/parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2767 "parser.tab.c"
    break;

  case 17: /* specifier: type_specifier  */
#line 956 "../src/parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2773 "parser.tab.c"
    break;

  case 18: /* specifier: type_qualifier  */
#line 957 "../src/parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2779 "parser.tab.c"
    break;

  case 19: /* storage_class_specifier: TYPEDEF  */
#line 961 "../src/parser.y"
            {
        in_typedef = 1;
        (yyval.node) = createNode(NODE_STORAGE_CLASS_SPECIFIER, "typedef");
    }
#line 2788 "parser.tab.c"
    break;

  case 20: /* storage_class_specifier: EXTERN  */
#line 965 "../src/parser.y"
             { (yyval.node) = createNode(NODE_STORAGE_CLASS_SPECIFIER, "extern"); }
#line 2794 "parser.tab.c"
    break;

  case 21: /* storage_class_specifier: STATIC  */
#line 966 "../src/parser.y"
             {
        is_static = 1;
        (yyval.node) = createNode(NODE_STORAGE_CLASS_SPECIFIER, "static");
    }
#line 2803 "parser.tab.c"
    break;

  case 22: /* storage_class_specifier: AUTO  */
#line 970 "../src/parser.y"
           { (yyval.node) = createNode(NODE_STORAGE_CLASS_SPECIFIER, "auto"); }
#line 2809 "parser.tab.c"
    break;

  case 23: /* storage_class_specifier: REGISTER  */
#line 971 "../src/parser.y"
               { (yyval.node) = createNode(NODE_STORAGE_CLASS_SPECIFIER, "register"); }
#line 2815 "parser.tab.c"
    break;

  case 24: /* type_specifier: VOID  */
#line 975 "../src/parser.y"
         {
        setCurrentType("void");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "void");
    }
#line 2824 "parser.tab.c"
    break;

  case 25: /* type_specifier: CHAR_TOKEN  */
#line 979 "../src/parser.y"
                 {
        setCurrentType("char");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "char");
    }
#line 2833 "parser.tab.c"
    break;

  case 26: /* type_specifier: SHORT  */
#line 983 "../src/parser.y"
            {
        setCurrentType("short");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "short");
    }
#line 2842 "parser.tab.c"
    break;

  case 27: /* type_specifier: INT  */
#line 987 "../src/parser.y"
          {
        setCurrentType("int");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "int");
    }
#line 2851 "parser.tab.c"
    break;

  case 28: /* type_specifier: LONG  */
#line 991 "../src/parser.y"
           {
        setCurrentType("long");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "long");
    }
#line 2860 "parser.tab.c"
    break;

  case 29: /* type_specifier: FLOAT_TOKEN  */
#line 995 "../src/parser.y"
                  {
        setCurrentType("float");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "float");
    }
#line 2869 "parser.tab.c"
    break;

  case 30: /* type_specifier: DOUBLE  */
#line 999 "../src/parser.y"
             {
        setCurrentType("double");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "double");
    }
#line 2878 "parser.tab.c"
    break;

  case 31: /* type_specifier: SIGNED  */
#line 1003 "../src/parser.y"
             {
        setCurrentType("signed");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "signed");
    }
#line 2887 "parser.tab.c"
    break;

  case 32: /* type_specifier: UNSIGNED  */
#line 1007 "../src/parser.y"
               {
        setCurrentType("unsigned");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "unsigned");
    }
#line 2896 "parser.tab.c"
    break;

  case 33: /* type_specifier: BOOL  */
#line 1011 "../src/parser.y"
           {
        setCurrentType("bool");
        (yyval.node) = createNode(NODE_TYPE_SPECIFIER, "bool");
    }
#line 2905 "parser.tab.c"
    break;

  case 34: /* type_specifier: struct_or_union_specifier  */
#line 1015 "../src/parser.y"
                                { (yyval.node) = (yyvsp[0].node); }
#line 2911 "parser.tab.c"
    break;

  case 35: /* type_specifier: enum_specifier  */
#line 1016 "../src/parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2917 "parser.tab.c"
    break;

  case 36: /* type_specifier: TYPE_NAME  */
#line 1017 "../src/parser.y"
                {
        if ((yyvsp[0].node) && (yyvsp[0].node)->value) setCurrentType((yyvsp[0].node)->value);
        (yyval.node) = (yyvsp[0].node);
    }
#line 2926 "parser.tab.c"
    break;

  case 37: /* type_qualifier: CONST  */
#line 1024 "../src/parser.y"
          { 
        has_const_before_ptr = 1;  // Track that const appeared in declaration
        (yyval.node) = createNode(NODE_TYPE_QUALIFIER, "const"); 
    }
#line 2935 "parser.tab.c"
    break;

  case 38: /* type_qualifier: VOLATILE  */
#line 1028 "../src/parser.y"
               { (yyval.node) = createNode(NODE_TYPE_QUALIFIER, "volatile"); }
#line 2941 "parser.tab.c"
    break;

  case 39: /* struct_or_union_specifier: STRUCT IDENTIFIER  */
#line 1032 "../src/parser.y"
                      {
        // Reference to existing struct
        char structType[256];
        sprintf(structType, "struct %s", (yyvsp[0].node)->value);
        setCurrentType(structType);
        (yyval.node) = createNode(NODE_STRUCT_SPECIFIER, "struct");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 2954 "parser.tab.c"
    break;

  case 40: /* struct_or_union_specifier: STRUCT LBRACE struct_declaration_list RBRACE  */
#line 1040 "../src/parser.y"
                                                   {
        // Anonymous struct - generate a name for size calculation
        char anonName[128];
        sprintf(anonName, "__anon_struct_%d", anonymous_struct_counter++);
        
        // Parse struct members and insert into struct table
        parseStructDefinition(anonName, (yyvsp[-1].node));
        
        char structType[256];
        sprintf(structType, "struct %s", anonName);
        setCurrentType(structType);
        (yyval.node) = createNode(NODE_STRUCT_SPECIFIER, "struct");
        addChild((yyval.node),(yyvsp[-1].node));
    }
#line 2973 "parser.tab.c"
    break;

  case 41: /* struct_or_union_specifier: STRUCT IDENTIFIER LBRACE struct_declaration_list RBRACE  */
#line 1054 "../src/parser.y"
                                                              {
        // Struct definition - record it
        char structType[256];
        sprintf(structType, "struct %s", (yyvsp[-3].node)->value);
        setCurrentType(structType);
        
        // Parse struct members and insert into struct table
        parseStructDefinition((yyvsp[-3].node)->value, (yyvsp[-1].node));
        
        (yyval.node) = createNode(NODE_STRUCT_SPECIFIER, "struct_definition");
        addChild((yyval.node), (yyvsp[-3].node));
        addChild((yyval.node), (yyvsp[-1].node));
    }
#line 2991 "parser.tab.c"
    break;

  case 42: /* struct_or_union_specifier: UNION IDENTIFIER  */
#line 1067 "../src/parser.y"
                       {
        char unionType[256];
        sprintf(unionType, "union %s", (yyvsp[0].node)->value);
        setCurrentType(unionType);
        (yyval.node) = createNode(NODE_STRUCT_SPECIFIER, "union");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3003 "parser.tab.c"
    break;

  case 43: /* struct_or_union_specifier: UNION LBRACE struct_declaration_list RBRACE  */
#line 1074 "../src/parser.y"
                                                  {
        // Anonymous union - generate a name for size calculation
        char anonName[128];
        sprintf(anonName, "__anon_union_%d", anonymous_union_counter++);
        
        // Parse union members and insert into union table
        parseUnionDefinition(anonName, (yyvsp[-1].node));
        
        char unionType[256];
        sprintf(unionType, "union %s", anonName);
        setCurrentType(unionType);
        (yyval.node) = createNode(NODE_STRUCT_SPECIFIER, "union");
        addChild((yyval.node),(yyvsp[-1].node));
    }
#line 3022 "parser.tab.c"
    break;

  case 44: /* struct_or_union_specifier: UNION IDENTIFIER LBRACE struct_declaration_list RBRACE  */
#line 1088 "../src/parser.y"
                                                             {
        char unionType[256];
        sprintf(unionType, "union %s", (yyvsp[-3].node)->value);
        setCurrentType(unionType);
        
        // Parse union members and insert into union table
        parseUnionDefinition((yyvsp[-3].node)->value, (yyvsp[-1].node));
        
        (yyval.node) = createNode(NODE_STRUCT_SPECIFIER, "union_definition");
        addChild((yyval.node), (yyvsp[-3].node));
        addChild((yyval.node), (yyvsp[-1].node));
    }
#line 3039 "parser.tab.c"
    break;

  case 45: /* struct_declaration_list: struct_declaration  */
#line 1103 "../src/parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 3045 "parser.tab.c"
    break;

  case 46: /* struct_declaration_list: struct_declaration_list struct_declaration  */
#line 1104 "../src/parser.y"
                                                 { (yyval.node) = (yyvsp[-1].node); addChild((yyval.node), (yyvsp[0].node)); }
#line 3051 "parser.tab.c"
    break;

  case 47: /* struct_declaration: declaration_specifiers struct_declarator_list SEMICOLON  */
#line 1108 "../src/parser.y"
                                                            {
        (yyval.node) = createNode(NODE_DECLARATION, "struct_decl");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[-1].node));
    }
#line 3061 "parser.tab.c"
    break;

  case 48: /* struct_declarator_list: struct_declarator  */
#line 1116 "../src/parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 3067 "parser.tab.c"
    break;

  case 49: /* struct_declarator_list: struct_declarator_list COMMA struct_declarator  */
#line 1117 "../src/parser.y"
                                                     { (yyval.node) = (yyvsp[-2].node); addChild((yyval.node), (yyvsp[0].node)); }
#line 3073 "parser.tab.c"
    break;

  case 50: /* struct_declarator: declarator  */
#line 1121 "../src/parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3079 "parser.tab.c"
    break;

  case 51: /* struct_declarator: COLON constant_expression  */
#line 1122 "../src/parser.y"
                                { (yyval.node) = (yyvsp[0].node); }
#line 3085 "parser.tab.c"
    break;

  case 52: /* struct_declarator: declarator COLON constant_expression  */
#line 1123 "../src/parser.y"
                                           { (yyval.node) = (yyvsp[-2].node); addChild((yyval.node), (yyvsp[0].node)); }
#line 3091 "parser.tab.c"
    break;

  case 53: /* enum_specifier: ENUM LBRACE enumerator_list RBRACE  */
#line 1127 "../src/parser.y"
                                       {
        setCurrentType("enum");
        current_enum_value = 0;  // Reset enum counter
        (yyval.node) = createNode(NODE_ENUM_SPECIFIER, "enum");
        addChild((yyval.node), (yyvsp[-1].node));
    }
#line 3102 "parser.tab.c"
    break;

  case 54: /* enum_specifier: ENUM IDENTIFIER  */
#line 1133 "../src/parser.y"
                      {
        setCurrentType("enum");
        (yyval.node) = createNode(NODE_ENUM_SPECIFIER, "enum");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3112 "parser.tab.c"
    break;

  case 55: /* enum_specifier: ENUM IDENTIFIER LBRACE enumerator_list RBRACE  */
#line 1138 "../src/parser.y"
                                                    {
        setCurrentType("enum");
        current_enum_value = 0;  // Reset enum counter
        (yyval.node) = createNode(NODE_ENUM_SPECIFIER, "enum");
        addChild((yyval.node), (yyvsp[-3].node));
        addChild((yyval.node), (yyvsp[-1].node));
    }
#line 3124 "parser.tab.c"
    break;

  case 56: /* enumerator_list: enumerator  */
#line 1148 "../src/parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3130 "parser.tab.c"
    break;

  case 57: /* enumerator_list: enumerator_list COMMA enumerator  */
#line 1149 "../src/parser.y"
                                       { (yyval.node) = (yyvsp[-2].node); addChild((yyval.node), (yyvsp[0].node)); }
#line 3136 "parser.tab.c"
    break;

  case 58: /* enumerator: IDENTIFIER  */
#line 1153 "../src/parser.y"
               {
        if ((yyvsp[0].node) && (yyvsp[0].node)->value) {
            // Enum constants are integers, so size is 4 bytes
            insertVariable((yyvsp[0].node)->value, "int", 0, NULL, 0, 0, 0, 0, 0, 0);  // Enum constants are not static or references
            // Update the last inserted symbol's kind to enum_constant and set its value
            if (symCount > 0 && strcmp(symtab[symCount - 1].name, (yyvsp[0].node)->value) == 0) {
                strcpy(symtab[symCount - 1].kind, "enum_constant");
                symtab[symCount - 1].offset = current_enum_value;  // Store enum value in offset field
            }
            current_enum_value++;  // Increment for next enum constant
        }
        (yyval.node) = (yyvsp[0].node);
    }
#line 3154 "parser.tab.c"
    break;

  case 59: /* enumerator: IDENTIFIER ASSIGN constant_expression  */
#line 1166 "../src/parser.y"
                                            {
        if ((yyvsp[-2].node) && (yyvsp[-2].node)->value) {
            // Evaluate the constant expression to get the enum value
            int enum_val = evaluateConstantExpression((yyvsp[0].node));
            
            // Enum constants are integers, so size is 4 bytes
            insertVariable((yyvsp[-2].node)->value, "int", 0, NULL, 0, 0, 0, 0, 0, 0);  // Enum constants are not static or references
            // Update the last inserted symbol's kind to enum_constant and set its value
            if (symCount > 0 && strcmp(symtab[symCount - 1].name, (yyvsp[-2].node)->value) == 0) {
                strcpy(symtab[symCount - 1].kind, "enum_constant");
                symtab[symCount - 1].offset = enum_val;  // Store enum value in offset field
            }
            current_enum_value = enum_val + 1;  // Next enum constant will be enum_val + 1
        }
        (yyval.node) = (yyvsp[-2].node);
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3176 "parser.tab.c"
    break;

  case 60: /* init_declarator_list: init_declarator  */
#line 1186 "../src/parser.y"
                    {
        (yyval.node) = (yyvsp[0].node);
    }
#line 3184 "parser.tab.c"
    break;

  case 61: /* init_declarator_list: init_declarator_list COMMA init_declarator  */
#line 1189 "../src/parser.y"
                                                 {
        (yyval.node) = (yyvsp[-2].node);
        if ((yyvsp[0].node)) addChild((yyval.node), (yyvsp[0].node));
    }
#line 3193 "parser.tab.c"
    break;

  case 62: /* init_declarator: declarator  */
#line 1196 "../src/parser.y"
               {
        const char* varName = extractIdentifierName((yyvsp[0].node));
        
        // Use the saved declaration type (saved before parsing declarator)
        char savedCurrentType[256];
        strncpy(savedCurrentType, saved_declaration_type, 255);
        savedCurrentType[255] = '\0';
        
        // Skip insertion for function declarators, but not for function pointers
        // Function pointers have parameters AND a pointer level > 0
        int ptrLevel = countPointerLevels((yyvsp[0].node));
        int refLevel = countReferenceLevels((yyvsp[0].node));
        int isRef = (refLevel > 0);
        int isFuncPtr = isFunctionPointer((yyvsp[0].node)) || (ptrLevel > 0 && pending_param_count > 0);
        
        if (varName && (pending_param_count == 0 || isFuncPtr)) {
            // ERROR A: Check for redeclaration of typedef name as variable
            if (!in_typedef) {
                // Check if varName is already a typedef in current or outer scope
                for (int i = symCount - 1; i >= 0; i--) {
                    if (strcmp(symtab[i].name, varName) == 0) {
                        if (strcmp(symtab[i].kind, "typedef") == 0) {
                            type_error(yylineno, "redeclaration of '%s' as different kind of symbol (was typedef)", varName);
                            break;
                        }
                    }
                }
            }
            
            // FIX 2: Check for conflicting storage class on redeclaration/shadowing
            Symbol* existing = lookupSymbol(varName);
            if (existing && existing->scope_level != current_scope) {
                // This is shadowing (declaring in inner scope)
                // Check if storage classes conflict
                if (existing->is_static != is_static) {
                    type_error(yylineno, "Conflicting storage class for re-declaration of '%s'", varName);
                }
            }
            
            int isArray = hasArrayBrackets((yyvsp[0].node));
            int arrayDims[10] = {0};
            int numDims = 0;
            int hasEmptyBrackets = hasEmptyArrayBrackets((yyvsp[0].node));
            
            if (isArray) {
                numDims = extractArrayDimensions((yyvsp[0].node), arrayDims, 10);
                
                // Check for error: array has empty brackets but no initializer
                if (hasEmptyBrackets && numDims == 0) {
                    type_error(yylineno, "array size missing and no initializer");
                }
            }
            
            char fullType[256];
            if (isFuncPtr) {
                // Build function pointer type using saved return type
                buildFunctionPointerType(fullType, savedCurrentType, (yyvsp[0].node));
            } else if (isRef) {
                // Build type with reference
                buildFullTypeWithRefs(fullType, savedCurrentType, (yyvsp[0].node));
            } else {
                buildFullType(fullType, savedCurrentType, ptrLevel);
            }
            
            if (in_typedef) {
                // Insert the symbol and then manually set its kind to "typedef".
                insertVariable(varName, fullType, isArray, arrayDims, numDims, ptrLevel, 0, has_const_before_ptr, has_const_after_ptr, isRef);  // Typedefs are not static
                if (symCount > 0 && strcmp(symtab[symCount - 1].name, varName) == 0) {
                    strcpy(symtab[symCount - 1].kind, "typedef");
                }
            } else {
                // Always insert the variable into the symbol table
                // The symbol table will handle duplicates within the same scope/block
                // has_const_before_ptr -> points_to_const, has_const_after_ptr -> is_const_ptr
                insertVariable(varName, fullType, isArray, arrayDims, numDims, ptrLevel, is_static, has_const_before_ptr, has_const_after_ptr, isRef);
                // Mark as function pointer if needed
                if (isFuncPtr && symCount > 0 && strcmp(symtab[symCount - 1].name, varName) == 0) {
                    strcpy(symtab[symCount - 1].kind, "function_pointer");
                    registerFunctionPointer(varName);  // Register for IR generation
                }
                
                // Reset pending parameters for function pointers (they don't define functions)
                if (isFuncPtr) {
                    pending_param_count = 0;
                }
            }
        }
        (yyval.node) = (yyvsp[0].node);
    }
#line 3287 "parser.tab.c"
    break;

  case 63: /* init_declarator: declarator ASSIGN initializer  */
#line 1285 "../src/parser.y"
                                    {
        const char* varName = extractIdentifierName((yyvsp[-2].node));
        
        // Use the saved declaration type (saved before parsing declarator)
        char savedCurrentType[256];
        strncpy(savedCurrentType, saved_declaration_type, 255);
        savedCurrentType[255] = '\0';
        
        if (varName && !in_typedef) {
            // ERROR A: Check for redeclaration of typedef name as variable
            // Check if varName is already a typedef in current or outer scope
            for (int i = symCount - 1; i >= 0; i--) {
                if (strcmp(symtab[i].name, varName) == 0) {
                    if (strcmp(symtab[i].kind, "typedef") == 0) {
                        type_error(yylineno, "redeclaration of '%s' as different kind of symbol (was typedef)", varName);
                        break;
                    }
                }
            }
            
            // FIX 2: Check for conflicting storage class on redeclaration/shadowing
            Symbol* existing = lookupSymbol(varName);
            if (existing && existing->scope_level != current_scope) {
                // This is shadowing (declaring in inner scope)
                // Check if storage classes conflict
                if (existing->is_static != is_static) {
                    type_error(yylineno, "Conflicting storage class for re-declaration of '%s'", varName);
                }
            }
            
            // FIX 3: Check for non-constant initializer on static variable
            if (is_static && !isConstantExpression((yyvsp[0].node))) {
                type_error(yylineno, "Initializer for static storage must be constant");
            }
            
            int ptrLevel = countPointerLevels((yyvsp[-2].node));
            int refLevel = countReferenceLevels((yyvsp[-2].node));
            int isRef = (refLevel > 0);
            int isArray = hasArrayBrackets((yyvsp[-2].node));
            int arrayDims[10] = {0};
            int numDims = 0;
            int hasEmptyBrackets = hasEmptyArrayBrackets((yyvsp[-2].node));
            int isFuncPtr = isFunctionPointer((yyvsp[-2].node)) || (ptrLevel > 0 && pending_param_count > 0);
            
            if (isArray) {
                numDims = extractArrayDimensions((yyvsp[-2].node), arrayDims, 10);
                
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
                    int initCount = countInitializerElements((yyvsp[0].node));
                    if (initCount > 0) {
                        // Infer the size from initializer
                        arrayDims[0] = initCount;
                        numDims = 1;
                    } else {
                        type_error(yylineno, "array size missing and no initializer");
                    }
                } else if (numDims > 0 && arrayDims[0] > 0) {
                    // Validate: explicit size exists, check initializer count
                    int initCount = countInitializerElements((yyvsp[0].node));
                    if (initCount > arrayDims[0]) {
                        char fullType[256];
                        buildFullType(fullType, savedCurrentType, ptrLevel);
                        type_error(yylineno, "too many initializers for '%s[%d]'", 
                                  fullType, arrayDims[0]);
                    }
                    // Partial initialization is OK (initCount < arrayDims[0])
                }
            }
            
            char fullType[256];
            if (isFuncPtr) {
                // Build function pointer type using saved return type
                buildFunctionPointerType(fullType, savedCurrentType, (yyvsp[-2].node));
            } else if (isRef) {
                // Build type with reference
                buildFullTypeWithRefs(fullType, savedCurrentType, (yyvsp[-2].node));
            } else {
                buildFullType(fullType, savedCurrentType, ptrLevel);
            }
            
            // Type check initialization for function pointers
            if (isFuncPtr && (yyvsp[0].node)->dataType) {
                // Function pointer initialization: check if initializer is a function
                // For now, allow initialization of function pointers with identifiers
                // This will need more sophisticated checking in a complete implementation
                if ((yyvsp[0].node)->type == NODE_IDENTIFIER) {
                    // Check if the identifier is a function
                    Symbol* sym = lookupSymbol((yyvsp[0].node)->value);
                    if (!sym || !sym->is_function) {
                        // Don't error here - might be a valid function name
                        // The error will be caught later if it's truly invalid
                    }
                }
            }
            // Type check initialization: check if trying to initialize scalar with array
            else if (!isArray && ptrLevel == 0 && (yyvsp[0].node)->dataType && isArrayType((yyvsp[0].node)->dataType)) {
                // Trying to initialize non-pointer scalar with array
                char* base_type = getArrayBaseType((yyvsp[0].node)->dataType);
                if (base_type) {
                    type_error(yylineno, "cannot convert array type '%s' to '%s'", (yyvsp[0].node)->dataType, base_type);
                } else {
                    type_error(yylineno, "cannot convert array type '%s' to '%s'", (yyvsp[0].node)->dataType, fullType);
                }
            }
            
            // Type check initialization: pointer = non-zero integer (error) - but not for function pointers
            else if (!isFuncPtr && ptrLevel > 0 && (yyvsp[0].node)->dataType && !isNullPointer((yyvsp[0].node))) {
                char* init_decayed = decayArrayToPointer((yyvsp[0].node)->dataType);
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
            if (!isArray && ptrLevel == 0 && (yyvsp[0].node)->dataType && isArithmeticType(fullType)) {
                char* init_decayed = decayArrayToPointer((yyvsp[0].node)->dataType);
                if (strstr(init_decayed, "*")) {
                    // Trying to initialize integer with pointer
                    type_error(yylineno, "initialization makes integer from pointer without a cast");
                }
                free(init_decayed);
            }
            
            // Always insert the variable into the symbol table
            // The symbol table will handle duplicates within the same scope/block
            insertVariable(varName, fullType, isArray, arrayDims, numDims, ptrLevel, is_static, has_const_before_ptr, has_const_after_ptr, isRef);
            // Mark as function pointer if needed
            if (isFuncPtr && symCount > 0 && strcmp(symtab[symCount - 1].name, varName) == 0) {
                strcpy(symtab[symCount - 1].kind, "function_pointer");
                registerFunctionPointer(varName);  // Register for IR generation
            }
            /* emit("ASSIGN", ...) REMOVED */
        }        // Create an AST node for the initialization
        (yyval.node) = createNode(NODE_INITIALIZER, "=");
        addChild((yyval.node), (yyvsp[-2].node)); // The declarator (ID)
        addChild((yyval.node), (yyvsp[0].node)); // The initializer (expression)
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType;
    }
#line 3445 "parser.tab.c"
    break;

  case 64: /* initializer: assignment_expression  */
#line 1441 "../src/parser.y"
                          { (yyval.node) = (yyvsp[0].node); }
#line 3451 "parser.tab.c"
    break;

  case 65: /* initializer: LBRACE initializer_list RBRACE  */
#line 1442 "../src/parser.y"
                                     { (yyval.node) = (yyvsp[-1].node); }
#line 3457 "parser.tab.c"
    break;

  case 66: /* initializer: LBRACE initializer_list COMMA RBRACE  */
#line 1443 "../src/parser.y"
                                           { (yyval.node) = (yyvsp[-2].node); }
#line 3463 "parser.tab.c"
    break;

  case 67: /* initializer_list: initializer  */
#line 1447 "../src/parser.y"
                {
        (yyval.node) = createNode(NODE_INITIALIZER, "init_list");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3472 "parser.tab.c"
    break;

  case 68: /* initializer_list: initializer_list COMMA initializer  */
#line 1451 "../src/parser.y"
                                         {
        (yyval.node) = (yyvsp[-2].node);
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3481 "parser.tab.c"
    break;

  case 69: /* declarator: direct_declarator  */
#line 1458 "../src/parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 3487 "parser.tab.c"
    break;

  case 70: /* declarator: MULTIPLY declarator  */
#line 1459 "../src/parser.y"
                          {
        (yyval.node) = createNode(NODE_POINTER, "*");
        addChild((yyval.node), (yyvsp[0].node));
        /* TODO: This should update the pointer level for symbol table */
    }
#line 3497 "parser.tab.c"
    break;

  case 71: /* declarator: MULTIPLY type_qualifier_list declarator  */
#line 1464 "../src/parser.y"
                                              {
        has_const_after_ptr = 1;  // const appears after pointer (e.g., int* const)
        (yyval.node) = createNode(NODE_POINTER, "*");
        addChild((yyval.node), (yyvsp[-1].node)); // type_qualifier_list
        addChild((yyval.node), (yyvsp[0].node)); // declarator
        /* TODO: This should update the pointer level for symbol table */
    }
#line 3509 "parser.tab.c"
    break;

  case 72: /* declarator: BITWISE_AND declarator  */
#line 1471 "../src/parser.y"
                             {
        // Reference declarator (C++ style: int &x)
        (yyval.node) = createNode(NODE_POINTER, "&");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3519 "parser.tab.c"
    break;

  case 73: /* direct_declarator: IDENTIFIER  */
#line 1479 "../src/parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3525 "parser.tab.c"
    break;

  case 74: /* direct_declarator: TYPE_NAME  */
#line 1480 "../src/parser.y"
                { 
        // Allow TYPE_NAME in declarator position for better error messages
        // The semantic analyzer will catch if this is a redeclaration
        (yyval.node) = (yyvsp[0].node); 
    }
#line 3535 "parser.tab.c"
    break;

  case 75: /* direct_declarator: LPAREN declarator RPAREN  */
#line 1485 "../src/parser.y"
                               { (yyval.node) = (yyvsp[-1].node); }
#line 3541 "parser.tab.c"
    break;

  case 76: /* direct_declarator: direct_declarator LBRACKET RBRACKET  */
#line 1486 "../src/parser.y"
                                          { 
        (yyval.node) = createNode(NODE_DIRECT_DECLARATOR, "array[]");
        addChild((yyval.node), (yyvsp[-2].node));
    }
#line 3550 "parser.tab.c"
    break;

  case 77: /* direct_declarator: direct_declarator LBRACKET constant_expression RBRACKET  */
#line 1490 "../src/parser.y"
                                                              { 
        (yyval.node) = createNode(NODE_DIRECT_DECLARATOR, "array");
        addChild((yyval.node), (yyvsp[-3].node));
        
        // Validate array size
        if ((yyvsp[-1].node)) {
            // Check if the size expression is of integer type
            if ((yyvsp[-1].node)->dataType && !isIntegerType((yyvsp[-1].node)->dataType)) {
                if ((yyvsp[-1].node)->value) {
                    type_error(yylineno, "invalid array size '%s'", (yyvsp[-1].node)->value);
                } else {
                    type_error(yylineno, "invalid array size (non-integer type)");
                }
            } 
            // Check if size is a negative constant
            else if ((yyvsp[-1].node)->value) {
                int size = atoi((yyvsp[-1].node)->value);
                if (size < 0) {
                    type_error(yylineno, "negative array size");
                }
            }
            addChild((yyval.node), (yyvsp[-1].node));  // Store the dimension
        }
    }
#line 3579 "parser.tab.c"
    break;

  case 78: /* direct_declarator: direct_declarator LPAREN RPAREN  */
#line 1514 "../src/parser.y"
                                      {
        (yyval.node) = (yyvsp[-2].node);
        param_count_temp = 0;  // No parameters
        pending_param_count = 0; // Reset for empty parameter list
    }
#line 3589 "parser.tab.c"
    break;

  case 79: /* $@2: %empty  */
#line 1519 "../src/parser.y"
                               { param_count_temp = 0; pending_param_count = 0; parsing_parameters = 1; }
#line 3595 "parser.tab.c"
    break;

  case 80: /* direct_declarator: direct_declarator LPAREN $@2 parameter_type_list RPAREN  */
#line 1519 "../src/parser.y"
                                                                                                                                     {
        (yyval.node) = (yyvsp[-4].node);
        parsing_parameters = 0;  // Reset flag after parsing parameters
    }
#line 3604 "parser.tab.c"
    break;

  case 81: /* type_qualifier_list: type_qualifier  */
#line 1526 "../src/parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 3610 "parser.tab.c"
    break;

  case 82: /* type_qualifier_list: type_qualifier_list type_qualifier  */
#line 1527 "../src/parser.y"
                                         { (yyval.node) = (yyvsp[-1].node); addChild((yyval.node), (yyvsp[0].node)); }
#line 3616 "parser.tab.c"
    break;

  case 83: /* parameter_type_list: parameter_list  */
#line 1531 "../src/parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 3622 "parser.tab.c"
    break;

  case 84: /* parameter_list: parameter_declaration  */
#line 1535 "../src/parser.y"
                          {
        (yyval.node) = createNode(NODE_PARAMETER_LIST, "params");
        addChild((yyval.node), (yyvsp[0].node));
        // param_count_temp is now incremented in parameter_declaration when in_function_definition is set
    }
#line 3632 "parser.tab.c"
    break;

  case 85: /* parameter_list: parameter_list COMMA parameter_declaration  */
#line 1540 "../src/parser.y"
                                                 {
        (yyval.node) = (yyvsp[-2].node);
        addChild((yyval.node), (yyvsp[0].node));
        // param_count_temp is now incremented in parameter_declaration when in_function_definition is set
    }
#line 3642 "parser.tab.c"
    break;

  case 86: /* parameter_declaration: declaration_specifiers declarator  */
#line 1548 "../src/parser.y"
                                      {
        // FIX 1: Check for illegal 'static' storage class on function parameters
        if (hasStorageClass((yyvsp[-1].node), "static")) {
            type_error(yylineno, "Illegal storage class 'static' on function parameter");
        }
        
        // Store parameter information instead of inserting it immediately
        // This allows us to differentiate between function declarations and definitions
        (yyval.node) = createNode(NODE_PARAMETER_DECLARATION, "param");
        addChild((yyval.node), (yyvsp[-1].node));
        addChild((yyval.node), (yyvsp[0].node));
        
        // Extract identifier name and count pointer/reference levels
        const char* paramName = extractIdentifierName((yyvsp[0].node));
        if (paramName && pending_param_count < 32) {
            int ptrLevel = countPointerLevels((yyvsp[0].node));
            int refLevel = countReferenceLevels((yyvsp[0].node));
            int isRef = (refLevel > 0);
            
            // In function parameters, arrays decay to pointers
            // char* argv[] becomes char**, so count array dimensions as additional pointer levels
            int arrayDims = countArrayDimensions((yyvsp[0].node));
            int totalPtrLevel = ptrLevel + arrayDims;
            
            char fullType[256];
            if (isRef) {
                // Build type with reference
                buildFullTypeWithRefs(fullType, currentType, (yyvsp[0].node));
            } else {
                buildFullType(fullType, currentType, totalPtrLevel);
            }
            
            // Store parameter info for later insertion (if this is a definition)
            strncpy(pending_params[pending_param_count].name, paramName, 255);
            strncpy(pending_params[pending_param_count].type, fullType, 255);
            pending_params[pending_param_count].ptrLevel = totalPtrLevel;
            pending_params[pending_param_count].isReference = isRef;
            pending_param_count++;
        }
    }
#line 3687 "parser.tab.c"
    break;

  case 87: /* parameter_declaration: declaration_specifiers abstract_declarator  */
#line 1588 "../src/parser.y"
                                                 {
        // FIX 1: Check for illegal 'static' storage class on function parameters
        if (hasStorageClass((yyvsp[-1].node), "static")) {
            type_error(yylineno, "Illegal storage class 'static' on function parameter");
        }
        
        (yyval.node) = createNode(NODE_PARAMETER_DECLARATION, "param");
        addChild((yyval.node), (yyvsp[-1].node));
        addChild((yyval.node), (yyvsp[0].node));
        
        // Extract type information for function pointers
        if (pending_param_count < 32) {
            int refLevel = countReferenceLevels((yyvsp[0].node));
            int ptrLevel = countPointerLevels((yyvsp[0].node));
            int isRef = (refLevel > 0);
            int totalPtrLevel = ptrLevel;
            
            char fullType[256];
            if (isRef) {
                buildFullTypeWithRefs(fullType, currentType, (yyvsp[0].node));
            } else {
                buildFullType(fullType, currentType, totalPtrLevel);
            }
            
            strncpy(pending_params[pending_param_count].name, "", 255);  // No name for abstract declarator
            strncpy(pending_params[pending_param_count].type, fullType, 255);
            pending_params[pending_param_count].ptrLevel = totalPtrLevel;
            pending_params[pending_param_count].isReference = isRef;
        }
        
        // Also count parameters without names (for function pointers)
        pending_param_count++;
    }
#line 3725 "parser.tab.c"
    break;

  case 88: /* parameter_declaration: declaration_specifiers  */
#line 1621 "../src/parser.y"
                             { 
        // FIX 1: Check for illegal 'static' storage class on function parameters
        if (hasStorageClass((yyvsp[0].node), "static")) {
            type_error(yylineno, "Illegal storage class 'static' on function parameter");
        }
        
        (yyval.node) = (yyvsp[0].node);
        
        // Extract type information for function pointers (plain type, no pointer/reference)
        if (pending_param_count < 32) {
            strncpy(pending_params[pending_param_count].name, "", 255);
            strncpy(pending_params[pending_param_count].type, currentType, 255);
            pending_params[pending_param_count].ptrLevel = 0;
            pending_params[pending_param_count].isReference = 0;
        }
        
        // Also count parameters with just type (for function pointers)
        pending_param_count++;
    }
#line 3749 "parser.tab.c"
    break;

  case 89: /* abstract_declarator: MULTIPLY  */
#line 1643 "../src/parser.y"
             {
        (yyval.node) = createNode(NODE_POINTER, "*");
        // Don't enter scope for abstract declarators
    }
#line 3758 "parser.tab.c"
    break;

  case 90: /* abstract_declarator: BITWISE_AND  */
#line 1647 "../src/parser.y"
                  {
        (yyval.node) = createNode(NODE_POINTER, "&");
        // Reference in abstract declarator
    }
#line 3767 "parser.tab.c"
    break;

  case 91: /* abstract_declarator: direct_abstract_declarator  */
#line 1651 "../src/parser.y"
                                 { (yyval.node) = (yyvsp[0].node); }
#line 3773 "parser.tab.c"
    break;

  case 92: /* abstract_declarator: MULTIPLY type_qualifier_list  */
#line 1652 "../src/parser.y"
                                   { (yyval.node) = createNode(NODE_POINTER, "*"); addChild((yyval.node), (yyvsp[0].node)); }
#line 3779 "parser.tab.c"
    break;

  case 93: /* abstract_declarator: MULTIPLY direct_abstract_declarator  */
#line 1653 "../src/parser.y"
                                          { (yyval.node) = createNode(NODE_POINTER, "*"); addChild((yyval.node), (yyvsp[0].node)); }
#line 3785 "parser.tab.c"
    break;

  case 94: /* abstract_declarator: MULTIPLY type_qualifier_list direct_abstract_declarator  */
#line 1654 "../src/parser.y"
                                                              { (yyval.node) = createNode(NODE_POINTER, "*"); addChild((yyval.node), (yyvsp[-1].node)); addChild((yyval.node), (yyvsp[0].node)); }
#line 3791 "parser.tab.c"
    break;

  case 95: /* abstract_declarator: BITWISE_AND direct_abstract_declarator  */
#line 1655 "../src/parser.y"
                                             { (yyval.node) = createNode(NODE_POINTER, "&"); addChild((yyval.node), (yyvsp[0].node)); }
#line 3797 "parser.tab.c"
    break;

  case 96: /* direct_abstract_declarator: LPAREN abstract_declarator RPAREN  */
#line 1659 "../src/parser.y"
                                      { (yyval.node) = (yyvsp[-1].node); }
#line 3803 "parser.tab.c"
    break;

  case 97: /* direct_abstract_declarator: LBRACKET RBRACKET  */
#line 1660 "../src/parser.y"
                        { (yyval.node) = createNode(NODE_DIRECT_DECLARATOR, "[]"); }
#line 3809 "parser.tab.c"
    break;

  case 98: /* direct_abstract_declarator: LBRACKET constant_expression RBRACKET  */
#line 1661 "../src/parser.y"
                                            { (yyval.node) = createNode(NODE_DIRECT_DECLARATOR, "[]"); addChild((yyval.node), (yyvsp[-1].node)); }
#line 3815 "parser.tab.c"
    break;

  case 99: /* direct_abstract_declarator: direct_abstract_declarator LBRACKET RBRACKET  */
#line 1662 "../src/parser.y"
                                                   { (yyval.node) = (yyvsp[-2].node); }
#line 3821 "parser.tab.c"
    break;

  case 100: /* direct_abstract_declarator: direct_abstract_declarator LBRACKET constant_expression RBRACKET  */
#line 1663 "../src/parser.y"
                                                                       { (yyval.node) = (yyvsp[-3].node); addChild((yyval.node), (yyvsp[-1].node)); }
#line 3827 "parser.tab.c"
    break;

  case 101: /* direct_abstract_declarator: LPAREN RPAREN  */
#line 1664 "../src/parser.y"
                    { (yyval.node) = createNode(NODE_DIRECT_DECLARATOR, "()"); }
#line 3833 "parser.tab.c"
    break;

  case 102: /* direct_abstract_declarator: LPAREN parameter_type_list RPAREN  */
#line 1665 "../src/parser.y"
                                        { (yyval.node) = createNode(NODE_DIRECT_DECLARATOR, "()"); addChild((yyval.node), (yyvsp[-1].node)); }
#line 3839 "parser.tab.c"
    break;

  case 103: /* direct_abstract_declarator: direct_abstract_declarator LPAREN RPAREN  */
#line 1666 "../src/parser.y"
                                               { (yyval.node) = (yyvsp[-2].node); }
#line 3845 "parser.tab.c"
    break;

  case 104: /* direct_abstract_declarator: direct_abstract_declarator LPAREN parameter_type_list RPAREN  */
#line 1667 "../src/parser.y"
                                                                   { (yyval.node) = (yyvsp[-3].node); addChild((yyval.node), (yyvsp[-1].node)); }
#line 3851 "parser.tab.c"
    break;

  case 105: /* statement: labeled_statement  */
#line 1671 "../src/parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 3857 "parser.tab.c"
    break;

  case 106: /* statement: compound_statement  */
#line 1672 "../src/parser.y"
                         { (yyval.node) = (yyvsp[0].node); }
#line 3863 "parser.tab.c"
    break;

  case 107: /* statement: expression_statement  */
#line 1673 "../src/parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 3869 "parser.tab.c"
    break;

  case 108: /* statement: selection_statement  */
#line 1674 "../src/parser.y"
                          { (yyval.node) = (yyvsp[0].node); }
#line 3875 "parser.tab.c"
    break;

  case 109: /* statement: iteration_statement  */
#line 1675 "../src/parser.y"
                          { (yyval.node) = (yyvsp[0].node); }
#line 3881 "parser.tab.c"
    break;

  case 110: /* statement: jump_statement  */
#line 1676 "../src/parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 3887 "parser.tab.c"
    break;

  case 111: /* labeled_statement: IDENTIFIER COLON statement  */
#line 1680 "../src/parser.y"
                               {
        if ((yyvsp[-2].node) && (yyvsp[-2].node)->value) {
            /* emit("LABEL", ...) REMOVED */
            insertLabel((yyvsp[-2].node)->value);  // Insert as a label in the symbol table
        }
        (yyval.node) = createNode(NODE_LABELED_STATEMENT, "label");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3901 "parser.tab.c"
    break;

  case 112: /* labeled_statement: CASE constant_expression COLON statement  */
#line 1689 "../src/parser.y"
                                               {
        /* emit("LABEL", ...) REMOVED */
        (yyval.node) = createNode(NODE_LABELED_STATEMENT, "case");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3912 "parser.tab.c"
    break;

  case 113: /* labeled_statement: DEFAULT COLON statement  */
#line 1695 "../src/parser.y"
                              {
        /* emit("LABEL", ...) REMOVED */
        (yyval.node) = createNode(NODE_LABELED_STATEMENT, "default");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3922 "parser.tab.c"
    break;

  case 114: /* $@3: %empty  */
#line 1703 "../src/parser.y"
           {
        if (!in_function_body) {
            enterScope();  // Only enter scope for nested blocks, not function body
        }
        in_function_body = 0;  // After first brace, we're past function body level
    }
#line 3933 "parser.tab.c"
    break;

  case 115: /* compound_statement: LBRACE $@3 RBRACE  */
#line 1708 "../src/parser.y"
             {
        (yyval.node) = createNode(NODE_COMPOUND_STATEMENT, "compound");
        if (current_scope > 0) {
            exitScope();
        }
        recovering_from_error = 0;
    }
#line 3945 "parser.tab.c"
    break;

  case 116: /* $@4: %empty  */
#line 1715 "../src/parser.y"
             {
        if (!in_function_body) {
            enterScope();  // Only enter scope for nested blocks, not function body
        }
        in_function_body = 0;  // After first brace, we're past function body level
    }
#line 3956 "parser.tab.c"
    break;

  case 117: /* compound_statement: LBRACE $@4 block_item_list RBRACE  */
#line 1720 "../src/parser.y"
                             {
        (yyval.node) = (yyvsp[-1].node); // Pass up the block_item_list node
        if (current_scope > 0) {
            exitScope();
        }
        recovering_from_error = 0;
    }
#line 3968 "parser.tab.c"
    break;

  case 118: /* block_item_list: block_item  */
#line 1730 "../src/parser.y"
               {
        (yyval.node) = createNode(NODE_COMPOUND_STATEMENT, "block_list");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 3977 "parser.tab.c"
    break;

  case 119: /* block_item_list: block_item_list block_item  */
#line 1734 "../src/parser.y"
                                 {
        (yyval.node) = (yyvsp[-1].node);
        if ((yyvsp[0].node)) addChild((yyval.node), (yyvsp[0].node));
    }
#line 3986 "parser.tab.c"
    break;

  case 120: /* block_item: declaration  */
#line 1741 "../src/parser.y"
                { (yyval.node) = (yyvsp[0].node); }
#line 3992 "parser.tab.c"
    break;

  case 121: /* block_item: statement  */
#line 1742 "../src/parser.y"
                { (yyval.node) = (yyvsp[0].node); }
#line 3998 "parser.tab.c"
    break;

  case 122: /* block_item: function_definition  */
#line 1743 "../src/parser.y"
                          { (yyval.node) = (yyvsp[0].node); }
#line 4004 "parser.tab.c"
    break;

  case 123: /* expression_statement: SEMICOLON  */
#line 1747 "../src/parser.y"
              {
        (yyval.node) = createNode(NODE_EXPRESSION_STATEMENT, ";");
        recovering_from_error = 0;
    }
#line 4013 "parser.tab.c"
    break;

  case 124: /* expression_statement: expression SEMICOLON  */
#line 1751 "../src/parser.y"
                           {
        (yyval.node) = (yyvsp[-1].node);
        recovering_from_error = 0;
    }
#line 4022 "parser.tab.c"
    break;

  case 125: /* expression_statement: error SEMICOLON  */
#line 1755 "../src/parser.y"
                      {
        (yyval.node) = createNode(NODE_EXPRESSION_STATEMENT, "error_recovery");
        yyerrok;
        recovering_from_error = 1;
    }
#line 4032 "parser.tab.c"
    break;

  case 126: /* if_header: IF LPAREN expression RPAREN  */
#line 1763 "../src/parser.y"
                                {
        // Check that the condition expression is not void
        if ((yyvsp[-1].node) && (yyvsp[-1].node)->dataType && strcmp((yyvsp[-1].node)->dataType, "void") == 0) {
            type_error(yylineno, "void value not ignored as it ought to be");
        }
        
        // ERROR 5: Validate that condition is scalar type
        if ((yyvsp[-1].node)) {
            validateConditional((yyvsp[-1].node));
        }
        
        (yyval.node) = (yyvsp[-1].node); // Pass the expression node up
        /* All label and emit logic is REMOVED */
    }
#line 4051 "parser.tab.c"
    break;

  case 127: /* marker_while_start: %empty  */
#line 1779 "../src/parser.y"
                           {
    (yyval.node) = createNode(NODE_MARKER, "while_start");
    /* All label, stack, and emit logic REMOVED */
}
#line 4060 "parser.tab.c"
    break;

  case 128: /* for_label_marker: %empty  */
#line 1784 "../src/parser.y"
                         {
    (yyval.node) = createNode(NODE_MARKER, "for_labels");
    /* All label, stack, and emit logic REMOVED */
}
#line 4069 "parser.tab.c"
    break;

  case 129: /* for_increment_expression: %empty  */
#line 1790 "../src/parser.y"
           { (yyval.node) = NULL; }
#line 4075 "parser.tab.c"
    break;

  case 130: /* for_increment_expression: expression  */
#line 1791 "../src/parser.y"
                 { (yyval.node) = (yyvsp[0].node); }
#line 4081 "parser.tab.c"
    break;

  case 131: /* selection_statement: if_header statement  */
#line 1795 "../src/parser.y"
                                              {
        /* emit("LABEL", ...) REMOVED */
        (yyval.node) = createNode(NODE_SELECTION_STATEMENT, "if");
        addChild((yyval.node), (yyvsp[-1].node)); // The expression from if_header
        addChild((yyval.node), (yyvsp[0].node)); // The 'then' statement
        recovering_from_error = 0;
    }
#line 4093 "parser.tab.c"
    break;

  case 132: /* $@5: %empty  */
#line 1802 "../src/parser.y"
                               {
        /* All emit logic from this MRA is REMOVED */
      }
#line 4101 "parser.tab.c"
    break;

  case 133: /* selection_statement: if_header statement ELSE $@5 statement  */
#line 1805 "../src/parser.y"
                {
        /* emit("LABEL", ...) REMOVED */
        (yyval.node) = createNode(NODE_SELECTION_STATEMENT, "if_else");
        addChild((yyval.node), (yyvsp[-4].node)); // The expression from if_header
        addChild((yyval.node), (yyvsp[-3].node)); // The 'then' statement
        addChild((yyval.node), (yyvsp[0].node)); // The 'else' statement
        recovering_from_error = 0;
      }
#line 4114 "parser.tab.c"
    break;

  case 134: /* $@6: %empty  */
#line 1813 "../src/parser.y"
                                      {
        switch_depth++;  // Enter switch context
    }
#line 4122 "parser.tab.c"
    break;

  case 135: /* selection_statement: SWITCH LPAREN expression RPAREN $@6 statement  */
#line 1815 "../src/parser.y"
                {
        /* All label, stack, and emit logic REMOVED */
        switch_depth--;  // Exit switch context
        (yyval.node) = createNode(NODE_SELECTION_STATEMENT, "switch");
        addChild((yyval.node), (yyvsp[-3].node)); // The expression
        addChild((yyval.node), (yyvsp[0].node)); // The statement
        recovering_from_error = 0;
    }
#line 4135 "parser.tab.c"
    break;

  case 136: /* do_header: DO  */
#line 1827 "../src/parser.y"
       {
        loop_depth++;  // Enter loop context
        (yyval.node) = createNode(NODE_MARKER, "do_header");
        /* All label, stack, and emit logic REMOVED */
    }
#line 4145 "parser.tab.c"
    break;

  case 137: /* do_trailer: WHILE LPAREN expression RPAREN SEMICOLON  */
#line 1835 "../src/parser.y"
                                             {
        /* emit("IF_TRUE_GOTO", ...) REMOVED */
        // Check that the condition expression is not void
        if ((yyvsp[-2].node) && (yyvsp[-2].node)->dataType && strcmp((yyvsp[-2].node)->dataType, "void") == 0) {
            type_error(yylineno, "void value not ignored as it ought to be");
        }
        
        // ERROR 5: Validate that condition is scalar type
        if ((yyvsp[-2].node)) {
            validateConditional((yyvsp[-2].node));
        }
        
        (yyval.node) = createNode(NODE_ITERATION_STATEMENT, "do_while");
        addChild((yyval.node), (yyvsp[-2].node)); // expression
    }
#line 4165 "parser.tab.c"
    break;

  case 138: /* do_trailer: UNTIL LPAREN expression RPAREN SEMICOLON  */
#line 1850 "../src/parser.y"
                                               {
        /* emit("IF_FALSE_GOTO", ...) REMOVED */
        // Check that the condition expression is not void
        if ((yyvsp[-2].node) && (yyvsp[-2].node)->dataType && strcmp((yyvsp[-2].node)->dataType, "void") == 0) {
            type_error(yylineno, "void value not ignored as it ought to be");
        }
        
        // ERROR 5: Validate that condition is scalar type
        if ((yyvsp[-2].node)) {
            validateConditional((yyvsp[-2].node));
        }
        
        (yyval.node) = createNode(NODE_ITERATION_STATEMENT, "do_until");
        addChild((yyval.node), (yyvsp[-2].node)); // expression
    }
#line 4185 "parser.tab.c"
    break;

  case 139: /* do_trailer: WHILE LPAREN error RPAREN SEMICOLON  */
#line 1865 "../src/parser.y"
                                          {
        // Error recovery: malformed while condition
        (yyval.node) = createNode(NODE_ITERATION_STATEMENT, "do_while_error");
        yyerrok;
    }
#line 4195 "parser.tab.c"
    break;

  case 140: /* do_trailer: UNTIL LPAREN error RPAREN SEMICOLON  */
#line 1870 "../src/parser.y"
                                          {
        // Error recovery: malformed until condition
        (yyval.node) = createNode(NODE_ITERATION_STATEMENT, "do_until_error");
        yyerrok;
    }
#line 4205 "parser.tab.c"
    break;

  case 141: /* $@7: %empty  */
#line 1879 "../src/parser.y"
    {
        /* emit("IF_FALSE_GOTO", ...) REMOVED */
        // Check that the condition expression is not void
        if ((yyvsp[-1].node) && (yyvsp[-1].node)->dataType && strcmp((yyvsp[-1].node)->dataType, "void") == 0) {
            type_error(yylineno, "void value not ignored as it ought to be");
        }
        
        // ERROR 5: Validate that condition is scalar type
        if ((yyvsp[-1].node)) {
            validateConditional((yyvsp[-1].node));
        }
        
        loop_depth++;  // Enter loop context
    }
#line 4224 "parser.tab.c"
    break;

  case 142: /* iteration_statement: WHILE LPAREN marker_while_start expression RPAREN $@7 statement  */
#line 1894 "../src/parser.y"
    {
        /* All emit and popLoopLabels logic REMOVED */
        loop_depth--;  // Exit loop context
        (yyval.node) = createNode(NODE_ITERATION_STATEMENT, "while");
        addChild((yyval.node), (yyvsp[-3].node)); // expression
        addChild((yyval.node), (yyvsp[0].node)); // statement
        addChild((yyval.node), (yyvsp[-4].node)); // Add the marker node
        recovering_from_error = 0;
    }
#line 4238 "parser.tab.c"
    break;

  case 143: /* iteration_statement: do_header statement do_trailer  */
#line 1903 "../src/parser.y"
                                     {
        /* All emit and popLoopLabels logic REMOVED */
        loop_depth--;  // Exit loop context (entered in do_header)
        (yyval.node) = (yyvsp[0].node); // The node (do_while/do_until) created in do_trailer
        addChild((yyval.node), (yyvsp[-2].node)); // do_header marker
        addChild((yyval.node), (yyvsp[-1].node)); // statement
        recovering_from_error = 0;
    }
#line 4251 "parser.tab.c"
    break;

  case 144: /* $@8: %empty  */
#line 1915 "../src/parser.y"
        { 
            /* emit("IF_FALSE_GOTO", ...) REMOVED */
            // Check that the condition expression is not void (if present)
            if ((yyvsp[0].node) && (yyvsp[0].node)->childCount > 0 && (yyvsp[0].node)->children[0]) {
                TreeNode* cond = (yyvsp[0].node)->children[0];
                if (cond->dataType && strcmp(cond->dataType, "void") == 0) {
                    type_error(yylineno, "void value not ignored as it ought to be");
                }
                
                // ERROR 5: Validate that condition is scalar type
                validateConditional(cond);
            }
            loop_depth++;  // Enter loop context
        }
#line 4270 "parser.tab.c"
    break;

  case 145: /* iteration_statement: FOR LPAREN expression_statement for_label_marker expression_statement $@8 for_increment_expression RPAREN statement  */
#line 1932 "../src/parser.y"
        {
            /* All emit and popLoopLabels logic REMOVED */
            loop_depth--;  // Exit loop context
            (yyval.node) = createNode(NODE_ITERATION_STATEMENT, "for");
            addChild((yyval.node), (yyvsp[-6].node));  // init
            addChild((yyval.node), (yyvsp[-4].node));  // cond
            if ((yyvsp[-2].node)) addChild((yyval.node), (yyvsp[-2].node));  // incr (only if present)
            addChild((yyval.node), (yyvsp[0].node));  // statement body
            addChild((yyval.node), (yyvsp[-5].node));  // Add the marker node
            recovering_from_error = 0;
        }
#line 4286 "parser.tab.c"
    break;

  case 146: /* $@9: %empty  */
#line 1944 "../src/parser.y"
        {
            // C99: Variables declared in for loop init are scoped to the loop
            enterScope();
        }
#line 4295 "parser.tab.c"
    break;

  case 147: /* $@10: %empty  */
#line 1951 "../src/parser.y"
        { 
            /* emit("IF_FALSE_GOTO", ...) REMOVED */
            // Check that the condition expression is not void (if present)
            if ((yyvsp[0].node) && (yyvsp[0].node)->childCount > 0 && (yyvsp[0].node)->children[0]) {
                TreeNode* cond = (yyvsp[0].node)->children[0];
                if (cond->dataType && strcmp(cond->dataType, "void") == 0) {
                    type_error(yylineno, "void value not ignored as it ought to be");
                }
                
                // ERROR 5: Validate that condition is scalar type
                validateConditional(cond);
            }
            loop_depth++;  // Enter loop context
        }
#line 4314 "parser.tab.c"
    break;

  case 148: /* iteration_statement: FOR LPAREN $@9 declaration for_label_marker expression_statement $@10 for_increment_expression RPAREN statement  */
#line 1968 "../src/parser.y"
        {
            /* All emit and popLoopLabels logic REMOVED */
            loop_depth--;  // Exit loop context
            (yyval.node) = createNode(NODE_ITERATION_STATEMENT, "for");
            addChild((yyval.node), (yyvsp[-6].node));  // init (declaration)
            addChild((yyval.node), (yyvsp[-4].node));  // cond
            if ((yyvsp[-2].node)) addChild((yyval.node), (yyvsp[-2].node));  // incr (only if present)
            addChild((yyval.node), (yyvsp[0].node));  // statement body
            addChild((yyval.node), (yyvsp[-5].node));  // Add the marker node
            recovering_from_error = 0;
            
            // Exit the for loop scope
            exitScope();
        }
#line 4333 "parser.tab.c"
    break;

  case 149: /* jump_statement: GOTO IDENTIFIER SEMICOLON  */
#line 1985 "../src/parser.y"
                              {
        /* emit("GOTO", ...) REMOVED */
        // Record goto for later validation (after all labels are defined)
        if ((yyvsp[-1].node) && (yyvsp[-1].node)->value && pending_goto_count < 100) {
            strcpy(pending_gotos[pending_goto_count].label_name, (yyvsp[-1].node)->value);
            pending_gotos[pending_goto_count].line_number = yylineno;
            pending_goto_count++;
        }
        (yyval.node) = createNode(NODE_JUMP_STATEMENT, "goto");
        addChild((yyval.node), (yyvsp[-1].node));
    }
#line 4349 "parser.tab.c"
    break;

  case 150: /* jump_statement: CONTINUE SEMICOLON  */
#line 1996 "../src/parser.y"
                         {
        /* All label logic and emit REMOVED */
        // Semantic check: continue must be inside a loop
        if (loop_depth == 0) {
            type_error(yylineno, "'continue' statement not in loop");
        }
        (yyval.node) = createNode(NODE_JUMP_STATEMENT, "continue");
    }
#line 4362 "parser.tab.c"
    break;

  case 151: /* jump_statement: BREAK SEMICOLON  */
#line 2004 "../src/parser.y"
                      {
        /* All label logic and emit REMOVED */
        // Semantic check: break must be inside a loop or switch
        if (loop_depth == 0 && switch_depth == 0) {
            type_error(yylineno, "'break' statement not in loop or switch");
        }
        (yyval.node) = createNode(NODE_JUMP_STATEMENT, "break");
    }
#line 4375 "parser.tab.c"
    break;

  case 152: /* jump_statement: RETURN SEMICOLON  */
#line 2012 "../src/parser.y"
                       {
        /* emit("RETURN", ...) REMOVED */
        (yyval.node) = createNode(NODE_JUMP_STATEMENT, "return");
    }
#line 4384 "parser.tab.c"
    break;

  case 153: /* jump_statement: RETURN expression SEMICOLON  */
#line 2016 "../src/parser.y"
                                  {
        /* emit("RETURN", ...) REMOVED */
        (yyval.node) = createNode(NODE_JUMP_STATEMENT, "return");
        addChild((yyval.node), (yyvsp[-1].node));
    }
#line 4394 "parser.tab.c"
    break;

  case 154: /* expression: assignment_expression  */
#line 2024 "../src/parser.y"
                          {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4402 "parser.tab.c"
    break;

  case 155: /* expression: expression COMMA assignment_expression  */
#line 2027 "../src/parser.y"
                                             {
        (yyval.node) = createNode(NODE_EXPRESSION, ",");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        /* $$->tacResult = ... REMOVED */
    }
#line 4413 "parser.tab.c"
    break;

  case 156: /* assignment_expression: conditional_expression  */
#line 2036 "../src/parser.y"
                           {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4421 "parser.tab.c"
    break;

  case 157: /* assignment_expression: unary_expression ASSIGN assignment_expression  */
#line 2039 "../src/parser.y"
                                                    {
        // Enhanced assignment type checking
        TypeCheckResult result = checkAssignment((yyvsp[-2].node), (yyvsp[0].node));
        
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));

        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4436 "parser.tab.c"
    break;

  case 158: /* assignment_expression: unary_expression PLUS_ASSIGN assignment_expression  */
#line 2049 "../src/parser.y"
                                                         {
        if (!isLValue((yyvsp[-2].node))) {
            semantic_error("lvalue required as left operand of assignment");
        }
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "+=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4450 "parser.tab.c"
    break;

  case 159: /* assignment_expression: unary_expression MINUS_ASSIGN assignment_expression  */
#line 2058 "../src/parser.y"
                                                          {
        if (!isLValue((yyvsp[-2].node))) {
            semantic_error("lvalue required as left operand of assignment");
        }
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "-=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4464 "parser.tab.c"
    break;

  case 160: /* assignment_expression: unary_expression MUL_ASSIGN assignment_expression  */
#line 2067 "../src/parser.y"
                                                        {
        if (!isLValue((yyvsp[-2].node))) {
            semantic_error("lvalue required as left operand of assignment");
        }
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "*=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4478 "parser.tab.c"
    break;

  case 161: /* assignment_expression: unary_expression DIV_ASSIGN assignment_expression  */
#line 2076 "../src/parser.y"
                                                        {
        if (!isLValue((yyvsp[-2].node))) {
            semantic_error("lvalue required as left operand of assignment");
        }
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "/=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4492 "parser.tab.c"
    break;

  case 162: /* assignment_expression: unary_expression MOD_ASSIGN assignment_expression  */
#line 2085 "../src/parser.y"
                                                        {
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "%=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4503 "parser.tab.c"
    break;

  case 163: /* assignment_expression: unary_expression AND_ASSIGN assignment_expression  */
#line 2091 "../src/parser.y"
                                                        {
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "&=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4514 "parser.tab.c"
    break;

  case 164: /* assignment_expression: unary_expression OR_ASSIGN assignment_expression  */
#line 2097 "../src/parser.y"
                                                       {
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "|=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4525 "parser.tab.c"
    break;

  case 165: /* assignment_expression: unary_expression XOR_ASSIGN assignment_expression  */
#line 2103 "../src/parser.y"
                                                        {
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "^=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4536 "parser.tab.c"
    break;

  case 166: /* assignment_expression: unary_expression LSHIFT_ASSIGN assignment_expression  */
#line 2109 "../src/parser.y"
                                                           {
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, "<<=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4547 "parser.tab.c"
    break;

  case 167: /* assignment_expression: unary_expression RSHIFT_ASSIGN assignment_expression  */
#line 2115 "../src/parser.y"
                                                           {
        (yyval.node) = createNode(NODE_ASSIGNMENT_EXPRESSION, ">>=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
    }
#line 4558 "parser.tab.c"
    break;

  case 168: /* conditional_expression: logical_or_expression  */
#line 2124 "../src/parser.y"
                          {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4566 "parser.tab.c"
    break;

  case 169: /* conditional_expression: logical_or_expression QUESTION expression COLON conditional_expression  */
#line 2127 "../src/parser.y"
                                                                             {
        /* emit(...) and newTemp() REMOVED */
        (yyval.node) = createNode(NODE_CONDITIONAL_EXPRESSION, "?:");
        addChild((yyval.node), (yyvsp[-4].node));
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[-2].node)->dataType ? strdup((yyvsp[-2].node)->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
#line 4580 "parser.tab.c"
    break;

  case 170: /* constant_expression: conditional_expression  */
#line 2139 "../src/parser.y"
                           {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4588 "parser.tab.c"
    break;

  case 171: /* logical_or_expression: logical_and_expression  */
#line 2145 "../src/parser.y"
                           {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4596 "parser.tab.c"
    break;

  case 172: /* logical_or_expression: logical_or_expression LOGICAL_OR logical_and_expression  */
#line 2148 "../src/parser.y"
                                                              {
        /* emit(...) and newTemp() REMOVED */
        (yyval.node) = createNode(NODE_LOGICAL_OR_EXPRESSION, "||");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
#line 4609 "parser.tab.c"
    break;

  case 173: /* logical_and_expression: inclusive_or_expression  */
#line 2159 "../src/parser.y"
                            {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4617 "parser.tab.c"
    break;

  case 174: /* logical_and_expression: logical_and_expression LOGICAL_AND inclusive_or_expression  */
#line 2162 "../src/parser.y"
                                                                 {
        /* emit(...) and newTemp() REMOVED */
        (yyval.node) = createNode(NODE_LOGICAL_AND_EXPRESSION, "&&");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
#line 4630 "parser.tab.c"
    break;

  case 175: /* inclusive_or_expression: exclusive_or_expression  */
#line 2173 "../src/parser.y"
                            {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4638 "parser.tab.c"
    break;

  case 176: /* inclusive_or_expression: inclusive_or_expression BITWISE_OR exclusive_or_expression  */
#line 2176 "../src/parser.y"
                                                                 {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("|", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_INCLUSIVE_OR_EXPRESSION, "|");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4652 "parser.tab.c"
    break;

  case 177: /* exclusive_or_expression: and_expression  */
#line 2188 "../src/parser.y"
                   {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4660 "parser.tab.c"
    break;

  case 178: /* exclusive_or_expression: exclusive_or_expression BITWISE_XOR and_expression  */
#line 2191 "../src/parser.y"
                                                         {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("^", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_EXCLUSIVE_OR_EXPRESSION, "^");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4674 "parser.tab.c"
    break;

  case 179: /* and_expression: equality_expression  */
#line 2203 "../src/parser.y"
                        {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4682 "parser.tab.c"
    break;

  case 180: /* and_expression: and_expression BITWISE_AND equality_expression  */
#line 2206 "../src/parser.y"
                                                     {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("&", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_AND_EXPRESSION, "&");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4696 "parser.tab.c"
    break;

  case 181: /* equality_expression: relational_expression  */
#line 2218 "../src/parser.y"
                          {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4704 "parser.tab.c"
    break;

  case 182: /* equality_expression: equality_expression EQ relational_expression  */
#line 2221 "../src/parser.y"
                                                   {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("==", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_EQUALITY_EXPRESSION, "==");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4718 "parser.tab.c"
    break;

  case 183: /* equality_expression: equality_expression NE relational_expression  */
#line 2230 "../src/parser.y"
                                                   {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("!=", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_EQUALITY_EXPRESSION, "!=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4732 "parser.tab.c"
    break;

  case 184: /* relational_expression: shift_expression  */
#line 2242 "../src/parser.y"
                     {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4740 "parser.tab.c"
    break;

  case 185: /* relational_expression: relational_expression LT shift_expression  */
#line 2245 "../src/parser.y"
                                                {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("<", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_RELATIONAL_EXPRESSION, "<");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4754 "parser.tab.c"
    break;

  case 186: /* relational_expression: relational_expression GT shift_expression  */
#line 2254 "../src/parser.y"
                                                {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp(">", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_RELATIONAL_EXPRESSION, ">");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4768 "parser.tab.c"
    break;

  case 187: /* relational_expression: relational_expression LE shift_expression  */
#line 2263 "../src/parser.y"
                                                {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("<=", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_RELATIONAL_EXPRESSION, "<=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4782 "parser.tab.c"
    break;

  case 188: /* relational_expression: relational_expression GE shift_expression  */
#line 2272 "../src/parser.y"
                                                {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp(">=", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_RELATIONAL_EXPRESSION, ">=");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4796 "parser.tab.c"
    break;

  case 189: /* shift_expression: additive_expression  */
#line 2284 "../src/parser.y"
                        {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4804 "parser.tab.c"
    break;

  case 190: /* shift_expression: shift_expression LSHIFT additive_expression  */
#line 2287 "../src/parser.y"
                                                  {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("<<", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_SHIFT_EXPRESSION, "<<");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4818 "parser.tab.c"
    break;

  case 191: /* shift_expression: shift_expression RSHIFT additive_expression  */
#line 2296 "../src/parser.y"
                                                  {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp(">>", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_SHIFT_EXPRESSION, ">>");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4832 "parser.tab.c"
    break;

  case 192: /* additive_expression: multiplicative_expression  */
#line 2308 "../src/parser.y"
                              {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4840 "parser.tab.c"
    break;

  case 193: /* additive_expression: additive_expression PLUS multiplicative_expression  */
#line 2311 "../src/parser.y"
                                                         {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("+", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        if (result == TYPE_ERROR) {
            // Error already reported by checkBinaryOp
        }
        
        (yyval.node) = createNode(NODE_ADDITIVE_EXPRESSION, "+");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4858 "parser.tab.c"
    break;

  case 194: /* additive_expression: additive_expression MINUS multiplicative_expression  */
#line 2324 "../src/parser.y"
                                                          {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("-", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        if (result == TYPE_ERROR) {
            // Error already reported by checkBinaryOp
        }

        (yyval.node) = createNode(NODE_ADDITIVE_EXPRESSION, "-");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4876 "parser.tab.c"
    break;

  case 195: /* multiplicative_expression: cast_expression  */
#line 2340 "../src/parser.y"
                    {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4884 "parser.tab.c"
    break;

  case 196: /* multiplicative_expression: multiplicative_expression MULTIPLY cast_expression  */
#line 2343 "../src/parser.y"
                                                         {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("*", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "*");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4898 "parser.tab.c"
    break;

  case 197: /* multiplicative_expression: multiplicative_expression DIVIDE cast_expression  */
#line 2352 "../src/parser.y"
                                                       {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("/", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "/");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4912 "parser.tab.c"
    break;

  case 198: /* multiplicative_expression: multiplicative_expression MODULO cast_expression  */
#line 2361 "../src/parser.y"
                                                       {
        char* result_type = NULL;
        TypeCheckResult result = checkBinaryOp("%", (yyvsp[-2].node), (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_MULTIPLICATIVE_EXPRESSION, "%");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 4926 "parser.tab.c"
    break;

  case 199: /* cast_expression: unary_expression  */
#line 2373 "../src/parser.y"
                     {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4934 "parser.tab.c"
    break;

  case 200: /* cast_expression: LPAREN type_name RPAREN cast_expression  */
#line 2376 "../src/parser.y"
                                              {
        (yyval.node) = createNode(NODE_CAST_EXPRESSION, "cast");
        addChild((yyval.node), (yyvsp[-2].node)); // The type_name
        addChild((yyval.node), (yyvsp[0].node)); // The expression being cast
        
        // Extract the cast type from the type_name node
        char cast_type[256];
        strcpy(cast_type, currentType);
        
        // Check if abstract_declarator adds pointer levels
        if ((yyvsp[-2].node)->childCount > 1) {
            TreeNode* abs_decl = (yyvsp[-2].node)->children[1];
            int ptr_levels = countPointerLevels(abs_decl);
            for (int i = 0; i < ptr_levels; i++) {
                strcat(cast_type, " *");
            }
        }
        
        (yyval.node)->dataType = strdup(cast_type);
    }
#line 4959 "parser.tab.c"
    break;

  case 201: /* type_name: declaration_specifiers  */
#line 2399 "../src/parser.y"
                           {
        (yyval.node) = createNode(NODE_TYPE_NAME, "type_name");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 4968 "parser.tab.c"
    break;

  case 202: /* type_name: declaration_specifiers abstract_declarator  */
#line 2403 "../src/parser.y"
                                                 {
        (yyval.node) = createNode(NODE_TYPE_NAME, "type_name");
        addChild((yyval.node), (yyvsp[-1].node));
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 4978 "parser.tab.c"
    break;

  case 203: /* unary_expression: postfix_expression  */
#line 2411 "../src/parser.y"
                       {
        (yyval.node) = (yyvsp[0].node);
    }
#line 4986 "parser.tab.c"
    break;

  case 204: /* unary_expression: INCREMENT unary_expression  */
#line 2414 "../src/parser.y"
                                 {
        if (!isLValue((yyvsp[0].node))) {
            semantic_error("lvalue required as increment operand");
        }
        /* newTemp(), emit("ADD"), emit("ASSIGN") REMOVED */
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "++_pre");
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[0].node)->dataType ? strdup((yyvsp[0].node)->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
#line 5001 "parser.tab.c"
    break;

  case 205: /* unary_expression: DECREMENT unary_expression  */
#line 2424 "../src/parser.y"
                                 {
        if (!isLValue((yyvsp[0].node))) {
            semantic_error("lvalue required as decrement operand");
        }
        /* newTemp(), emit("SUB"), emit("ASSIGN") REMOVED */
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "--_pre");
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[0].node)->dataType ? strdup((yyvsp[0].node)->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
#line 5016 "parser.tab.c"
    break;

  case 206: /* unary_expression: BITWISE_AND cast_expression  */
#line 2434 "../src/parser.y"
                                  {
        char* result_type = NULL;
        TypeCheckResult result = checkUnaryOp("&", (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "&");
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
    }
#line 5029 "parser.tab.c"
    break;

  case 207: /* unary_expression: MULTIPLY cast_expression  */
#line 2442 "../src/parser.y"
                               {
        char* result_type = NULL;
        TypeCheckResult result = checkUnaryOp("*", (yyvsp[0].node), &result_type);
        
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "*");
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
        (yyval.node)->isLValue = 1;
    }
#line 5043 "parser.tab.c"
    break;

  case 208: /* unary_expression: PLUS cast_expression  */
#line 2451 "../src/parser.y"
                           {
        (yyval.node) = (yyvsp[0].node);
    }
#line 5051 "parser.tab.c"
    break;

  case 209: /* unary_expression: MINUS cast_expression  */
#line 2454 "../src/parser.y"
                            {
        /* newTemp(), emit("NEG") REMOVED */
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "-_unary");
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = (yyvsp[0].node)->dataType ? strdup((yyvsp[0].node)->dataType) : NULL;
        
        // Compute the negated value ONLY if the operand is a constant literal
        if ((yyvsp[0].node)->value && 
            ((yyvsp[0].node)->type == NODE_CONSTANT || 
             (yyvsp[0].node)->type == NODE_INTEGER_CONSTANT ||
             (yyvsp[0].node)->type == NODE_HEX_CONSTANT ||
             (yyvsp[0].node)->type == NODE_OCTAL_CONSTANT ||
             (yyvsp[0].node)->type == NODE_BINARY_CONSTANT ||
             (yyvsp[0].node)->type == NODE_FLOAT_CONSTANT)) {
            int val = atoi((yyvsp[0].node)->value);
            char negVal[32];
            snprintf(negVal, sizeof(negVal), "%d", -val);
            (yyval.node)->value = strdup(negVal);
        }
        /* $$->tacResult = ... REMOVED */
    }
#line 5077 "parser.tab.c"
    break;

  case 210: /* unary_expression: BITWISE_NOT cast_expression  */
#line 2475 "../src/parser.y"
                                  {
        /* newTemp(), emit("BITNOT") REMOVED */
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "~");
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
#line 5089 "parser.tab.c"
    break;

  case 211: /* unary_expression: LOGICAL_NOT cast_expression  */
#line 2482 "../src/parser.y"
                                  {
        /* newTemp(), emit("NOT") REMOVED */
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "!");
        addChild((yyval.node), (yyvsp[0].node));
        // Logical NOT returns bool if operand is bool, otherwise int
        if ((yyvsp[0].node)->dataType && strcmp((yyvsp[0].node)->dataType, "bool") == 0) {
            (yyval.node)->dataType = strdup("bool");
        } else {
            (yyval.node)->dataType = strdup("int");
        }
        /* $$->tacResult = ... REMOVED */
    }
#line 5106 "parser.tab.c"
    break;

  case 212: /* unary_expression: SIZEOF unary_expression  */
#line 2494 "../src/parser.y"
                              {
        /* newTemp(), sprintf, emit("ASSIGN") REMOVED */
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "sizeof");
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
#line 5118 "parser.tab.c"
    break;

  case 213: /* unary_expression: SIZEOF LPAREN type_name RPAREN  */
#line 2501 "../src/parser.y"
                                     {
        /* newTemp(), emit("ASSIGN") REMOVED */
        (yyval.node) = createNode(NODE_UNARY_EXPRESSION, "sizeof");
        addChild((yyval.node), (yyvsp[-1].node)); // Add the type_name node
        (yyval.node)->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
#line 5130 "parser.tab.c"
    break;

  case 214: /* postfix_expression: primary_expression  */
#line 2511 "../src/parser.y"
                       {
        (yyval.node) = (yyvsp[0].node);
    }
#line 5138 "parser.tab.c"
    break;

  case 215: /* postfix_expression: postfix_expression LBRACKET expression RBRACKET  */
#line 2514 "../src/parser.y"
                                                      {
        // Enhanced array access type checking
        char* result_type = NULL;
        TypeCheckResult result = checkArrayAccess((yyvsp[-3].node), (yyvsp[-1].node), &result_type);
        
        (yyval.node) = createNode(NODE_POSTFIX_EXPRESSION, "[]");
        addChild((yyval.node), (yyvsp[-3].node));
        addChild((yyval.node), (yyvsp[-1].node));
        (yyval.node)->dataType = result_type;
        (yyval.node)->isLValue = 1;
    }
#line 5154 "parser.tab.c"
    break;

  case 216: /* postfix_expression: postfix_expression LPAREN RPAREN  */
#line 2525 "../src/parser.y"
                                       {
        // Enhanced function call type checking (no arguments)
        char* result_type = NULL;
        TypeCheckResult result = checkFunctionCall((yyvsp[-2].node)->value, NULL, &result_type);
        
        (yyval.node) = createNode(NODE_POSTFIX_EXPRESSION, "()");
        addChild((yyval.node), (yyvsp[-2].node));
        (yyval.node)->dataType = result_type;
    }
#line 5168 "parser.tab.c"
    break;

  case 217: /* postfix_expression: postfix_expression LPAREN argument_expression_list RPAREN  */
#line 2534 "../src/parser.y"
                                                                {
        // Enhanced function call type checking
        char* result_type = NULL;
        TypeCheckResult result = checkFunctionCall((yyvsp[-3].node)->value, (yyvsp[-1].node), &result_type);
        
        (yyval.node) = createNode(NODE_POSTFIX_EXPRESSION, "()");
        addChild((yyval.node), (yyvsp[-3].node));
        addChild((yyval.node), (yyvsp[-1].node));
        (yyval.node)->dataType = result_type;
    }
#line 5183 "parser.tab.c"
    break;

  case 218: /* postfix_expression: postfix_expression DOT IDENTIFIER  */
#line 2544 "../src/parser.y"
                                        {
        // Enhanced struct member access type checking
        char* result_type = NULL;
        TypeCheckResult result = checkMemberAccess((yyvsp[-2].node), (yyvsp[0].node)->value, ".", &result_type);
        
        (yyval.node) = createNode(NODE_POSTFIX_EXPRESSION, ".");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
        (yyval.node)->isLValue = 1;
    }
#line 5199 "parser.tab.c"
    break;

  case 219: /* postfix_expression: postfix_expression ARROW IDENTIFIER  */
#line 2555 "../src/parser.y"
                                          {
        // Enhanced pointer-to-struct member access type checking
        char* result_type = NULL;
        TypeCheckResult result = checkMemberAccess((yyvsp[-2].node), (yyvsp[0].node)->value, "->", &result_type);
        
        (yyval.node) = createNode(NODE_POSTFIX_EXPRESSION, "->");
        addChild((yyval.node), (yyvsp[-2].node));
        addChild((yyval.node), (yyvsp[0].node));
        (yyval.node)->dataType = result_type;
        (yyval.node)->isLValue = 1;
    }
#line 5215 "parser.tab.c"
    break;

  case 220: /* postfix_expression: postfix_expression INCREMENT  */
#line 2566 "../src/parser.y"
                                   {
        if (!isLValue((yyvsp[-1].node))) {
            semantic_error("lvalue required as increment operand");
        }
        /* newTemp(), emit("ASSIGN"), emit("ADD"), emit("ASSIGN") REMOVED */
        (yyval.node) = createNode(NODE_POSTFIX_EXPRESSION, "++_post");
        addChild((yyval.node), (yyvsp[-1].node));
        (yyval.node)->dataType = (yyvsp[-1].node)->dataType ? strdup((yyvsp[-1].node)->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
#line 5230 "parser.tab.c"
    break;

  case 221: /* postfix_expression: postfix_expression DECREMENT  */
#line 2576 "../src/parser.y"
                                   {
        if (!isLValue((yyvsp[-1].node))) {
            semantic_error("lvalue required as decrement operand");
        }
        /* newTemp(), emit("ASSIGN"), emit("SUB"), emit("ASSIGN") REMOVED */
        (yyval.node) = createNode(NODE_POSTFIX_EXPRESSION, "--_post");
        addChild((yyval.node), (yyvsp[-1].node));
        (yyval.node)->dataType = (yyvsp[-1].node)->dataType ? strdup((yyvsp[-1].node)->dataType) : NULL;
        /* $$->tacResult = ... REMOVED */
    }
#line 5245 "parser.tab.c"
    break;

  case 222: /* primary_expression: IDENTIFIER  */
#line 2589 "../src/parser.y"
               {
        (yyval.node) = (yyvsp[0].node);
        if ((yyvsp[0].node) && (yyvsp[0].node)->value) {
            Symbol* sym = lookupSymbol((yyvsp[0].node)->value);
            if (sym) {
                // For references, strip the & to get the underlying type
                // References should behave like the type they refer to in expressions
                const char* actualType = sym->is_function ? sym->return_type : sym->type;
                if (sym->is_reference) {
                    (yyval.node)->dataType = stripReferenceType(actualType);
                } else {
                    (yyval.node)->dataType = strdup(actualType);
                }
                (yyval.node)->isLValue = !sym->is_function;
            } else {
                // Undeclared identifier - report semantic error
                type_error(yylineno, "'%s' undeclared (first use in this function)", (yyvsp[0].node)->value);
                (yyval.node)->dataType = strdup("int"); // Assume int to continue parsing
                (yyval.node)->isLValue = 1; // Assume lvalue
            }
            /* $$->tacResult = ... REMOVED */
        }
    }
#line 5273 "parser.tab.c"
    break;

  case 223: /* primary_expression: TYPE_NAME  */
#line 2612 "../src/parser.y"
                {
        // Allow TYPE_NAME in expression context so we can catch misuse
        // (e.g., typedef name used with ++ operator)
        (yyval.node) = (yyvsp[0].node);
        // TYPE_NAME is never an lvalue - it's a type, not a variable
        (yyval.node)->isLValue = 0;
        if ((yyvsp[0].node) && (yyvsp[0].node)->value) {
            (yyval.node)->dataType = strdup((yyvsp[0].node)->value);
        }
    }
#line 5288 "parser.tab.c"
    break;

  case 224: /* primary_expression: INTEGER_CONSTANT  */
#line 2622 "../src/parser.y"
                       {
        (yyval.node) = (yyvsp[0].node);
        /* newTemp(), emit() REMOVED */
        (yyval.node)->dataType = strdup("int");
        /* $$->tacResult = ... REMOVED */
    }
#line 5299 "parser.tab.c"
    break;

  case 225: /* primary_expression: HEX_CONSTANT  */
#line 2628 "../src/parser.y"
                   {
        (yyval.node) = (yyvsp[0].node);
        (yyval.node)->dataType = strdup("int");
    }
#line 5308 "parser.tab.c"
    break;

  case 226: /* primary_expression: OCTAL_CONSTANT  */
#line 2632 "../src/parser.y"
                     {
        (yyval.node) = (yyvsp[0].node);
        (yyval.node)->dataType = strdup("int");
    }
#line 5317 "parser.tab.c"
    break;

  case 227: /* primary_expression: BINARY_CONSTANT  */
#line 2636 "../src/parser.y"
                      {
        (yyval.node) = (yyvsp[0].node);
        (yyval.node)->dataType = strdup("int");
    }
#line 5326 "parser.tab.c"
    break;

  case 228: /* primary_expression: FLOAT_CONSTANT  */
#line 2640 "../src/parser.y"
                     {
        (yyval.node) = (yyvsp[0].node);
        (yyval.node)->dataType = strdup("float");
    }
#line 5335 "parser.tab.c"
    break;

  case 229: /* primary_expression: CHAR_CONSTANT  */
#line 2644 "../src/parser.y"
                    {
        (yyval.node) = (yyvsp[0].node);
        (yyval.node)->dataType = strdup("char");
    }
#line 5344 "parser.tab.c"
    break;

  case 230: /* primary_expression: STRING_LITERAL  */
#line 2648 "../src/parser.y"
                     {
        (yyval.node) = (yyvsp[0].node);
        (yyval.node)->dataType = strdup("char*");
    }
#line 5353 "parser.tab.c"
    break;

  case 231: /* primary_expression: primary_expression STRING_LITERAL  */
#line 2652 "../src/parser.y"
                                           {
        // String literal concatenation: "str1" "str2" becomes "str1str2"
        if ((yyvsp[-1].node) && (yyvsp[-1].node)->dataType && strcmp((yyvsp[-1].node)->dataType, "char*") == 0 && (yyvsp[0].node)) {
            // Both are string literals - concatenate them
            size_t len1 = strlen((yyvsp[-1].node)->value) - 2;  // -2 to exclude quotes
            size_t len2 = strlen((yyvsp[0].node)->value) - 2;  // -2 to exclude quotes
            char* concatenated = (char*)malloc(len1 + len2 + 3); // +3 for quotes and null
            
            // Copy first string without closing quote
            strncpy(concatenated, (yyvsp[-1].node)->value, len1 + 1);
            
            // Append second string without opening quote
            strncpy(concatenated + len1 + 1, (yyvsp[0].node)->value + 1, len2 + 1);
            
            concatenated[len1 + len2 + 2] = '\0';
            
            (yyval.node) = createNode(NODE_STRING_LITERAL, concatenated);
            (yyval.node)->dataType = strdup("char*");
            
            free(concatenated);
            freeNode((yyvsp[-1].node));
            freeNode((yyvsp[0].node));
        } else {
            // Error case - should not happen if lexer is correct
            (yyval.node) = (yyvsp[-1].node);
        }
    }
#line 5385 "parser.tab.c"
    break;

  case 232: /* primary_expression: LPAREN expression RPAREN  */
#line 2679 "../src/parser.y"
                               {
        (yyval.node) = (yyvsp[-1].node);
    }
#line 5393 "parser.tab.c"
    break;

  case 233: /* argument_expression_list: assignment_expression  */
#line 2685 "../src/parser.y"
                          {
        /* emit("ARG", ...) REMOVED */
        (yyval.node) = createNode(NODE_EXPRESSION, "args");
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 5403 "parser.tab.c"
    break;

  case 234: /* argument_expression_list: argument_expression_list COMMA assignment_expression  */
#line 2690 "../src/parser.y"
                                                           {
        /* emit("ARG", ...) REMOVED */
        (yyval.node) = (yyvsp[-2].node);
        addChild((yyval.node), (yyvsp[0].node));
    }
#line 5413 "parser.tab.c"
    break;


#line 5417 "parser.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 2697 "../src/parser.y"


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
