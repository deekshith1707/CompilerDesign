#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SYMBOLS 2000
#define POINTER_SIZE 8
#define MAX_STRUCT_MEMBERS 50
#define MAX_STRUCTS 100

// Structure member information
typedef struct StructMember {
    char name[128];
    char type[128];
    int offset;
    int size;
} StructMember;

// Structure definition table
typedef struct StructDef {
    char name[128];
    StructMember members[MAX_STRUCT_MEMBERS];
    int member_count;
    int total_size;
} StructDef;

typedef struct Symbol {
    char name[128];
    char type[128];
    char kind[32];
    int scope_level;
    int parent_scope;
    int block_id;
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
    char function_scope[128];
    int is_external;
    int is_static;
    int is_const;
    int is_const_ptr;
    int points_to_const;
    int is_reference;
} Symbol;


extern Symbol symtab[MAX_SYMBOLS];
extern int symCount;
extern int current_scope;
extern int parent_scopes[100];
extern int scope_depth;
extern int current_offset;
extern char currentType[128];
extern char current_function[128];
extern int next_scope;
extern int current_block_id;
extern int next_block_id;
extern int parent_blocks[100];

// Struct definition table
extern StructDef structTable[MAX_STRUCTS];
extern int structCount;

// Union definition table
extern StructDef unionTable[MAX_STRUCTS];
extern int unionCount;

// Function pointer registry
extern char function_pointers[MAX_SYMBOLS][128];
extern int function_pointer_count;

// Function prototypes
void insertVariable(const char* name, const char* type, int is_array, int* dims, int num_dims, int ptr_level, int is_static, int points_to_const, int is_const_ptr, int is_reference);
void insertParameter(const char* name, const char* type, int ptr_level); 
void insertFunction(const char* name, const char* ret_type, int param_count, char params[][128], char param_names[][128], int is_static);
void insertExternalFunction(const char* name, const char* ret_type);
void insertLabel(const char* name);
Symbol* lookupLabel(const char* name);
void insertStruct(const char* name, StructMember* members, int member_count);
void insertUnion(const char* name, StructMember* members, int member_count);
StructDef* lookupStruct(const char* name);
StructDef* lookupUnion(const char* name);
int getStructSize(const char* struct_name);
int getUnionSize(const char* union_name);
Symbol* lookupSymbol(const char* name);
void enterScope();
void exitScope();
void enterFunctionScope(const char* func_name);
void exitFunctionScope();
void insertSymbol(const char* name, const char* type, int is_function, int is_static);
void registerFunctionPointer(const char* name);
int isFunctionPointerName(const char* name); 
void moveRecentSymbolsToCurrentScope(int count);
void markRecentSymbolsAsParameters(int count);
int is_type_name(const char* name);
int getTypeSize(const char* type);
int isArithmeticType(const char* type);
int isIntegerType(const char* type);
char* usualArithConv(const char* t1, const char* t2);
int isAssignable(const char* lhs_type, const char* rhs_type);
void setCurrentType(const char* type);
void printSymbolTable();

// Type checking results
typedef enum {
    TYPE_OK,
    TYPE_ERROR,
    TYPE_WARNING
} TypeCheckResult;

// Type checking functions
TypeCheckResult checkBinaryOp(const char* op, TreeNode* left, TreeNode* right, char** result_type);
TypeCheckResult checkUnaryOp(const char* op, TreeNode* operand, char** result_type);
TypeCheckResult checkAssignment(TreeNode* lhs, TreeNode* rhs);
TypeCheckResult checkFunctionCall(const char* func_name, TreeNode* args, char** result_type);
TypeCheckResult checkArrayAccess(TreeNode* array, TreeNode* index, char** result_type);
TypeCheckResult checkMemberAccess(TreeNode* struct_expr, const char* member, const char* op, char** result_type);

// Type compatibility functions
int isArrayType(const char* type);
char* getArrayBaseType(const char* arrayType);
char* decayArrayToPointer(const char* type);
int isPointerCompatible(const char* ptr1, const char* ptr2);
int isNullPointer(TreeNode* expr);
int canImplicitConvert(const char* from_type, const char* to_type);
char* getCommonType(const char* type1, const char* type2);

// Typedef resolution
char* resolveTypedef(const char* type);

// Enhanced validation functions
int validateBreakContinue(const char* stmt_type);
int validateReturn(TreeNode* expr, const char* expected_return_type);
int validateSwitchCase(TreeNode* switch_expr, TreeNode* case_expr);
int validateGotoLabel(const char* label);
int isScalarType(const char* type);
TypeCheckResult validateConditional(TreeNode* expr);

// Storage class validation
int hasStorageClass(TreeNode* decl_specs, const char* storage_class);
int isConstantExpression(TreeNode* expr);

// Type error reporting
void type_error(int line, const char* msg, ...);
void type_warning(int line, const char* msg, ...);

#ifdef __cplusplus
}
#endif

#endif // SYMBOL_TABLE_H