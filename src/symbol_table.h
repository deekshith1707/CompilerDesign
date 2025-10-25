#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SYMBOLS 2000
#define POINTER_SIZE 8  // 8 bytes for 64-bit pointers, change to 4 for 32-bit
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
    int scope_level;      // Hierarchical scope: 0=global, 1=function parameters/locals
    int parent_scope;     // Parent scope level (-1 for global scope)
    int block_id;         // Unique identifier for each nested block
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
    char function_scope[128];  // Name of function this symbol belongs to (for non-global scopes)
    int is_external;           // 1 if this is an external/library function (e.g., printf)
    int is_static;             // 1 if this has static storage class (internal linkage)
    int is_const;              // 1 if this is const-qualified (e.g., const int* or int* const)
    int is_const_ptr;          // 1 if this is a const pointer (pointer itself is const, e.g., int* const)
    int points_to_const;       // 1 if this points to const data (e.g., const int*)
} Symbol;

// Global symbol table
extern Symbol symtab[MAX_SYMBOLS];
extern int symCount;
extern int current_scope;
extern int parent_scopes[100];  // Stack to track parent scope relationships
extern int scope_depth;         // Current depth in scope stack
extern int current_offset;
extern char currentType[128];
extern char current_function[128];  // Track which function we're currently in
extern int next_scope;              // Next scope number to assign (sequential)
extern int current_block_id;        // Unique ID for current nested block
extern int next_block_id;           // Counter for assigning unique block IDs
extern int parent_blocks[100];      // Stack of parent block IDs

// Struct definition table
extern StructDef structTable[MAX_STRUCTS];
extern int structCount;

// Union definition table (same structure as struct, but size calculation differs)
extern StructDef unionTable[MAX_STRUCTS];
extern int unionCount;

// Function pointer registry (for IR generation after scope exit)
extern char function_pointers[MAX_SYMBOLS][128];
extern int function_pointer_count;

// Function prototypes
void insertVariable(const char* name, const char* type, int is_array, int* dims, int num_dims, int ptr_level, int is_static, int points_to_const, int is_const_ptr);
void insertParameter(const char* name, const char* type, int ptr_level);  // For function parameters
void insertFunction(const char* name, const char* ret_type, int param_count, char params[][128], char param_names[][128], int is_static);
void insertExternalFunction(const char* name, const char* ret_type);  // For library/external functions
void insertLabel(const char* name);  // For goto labels
Symbol* lookupLabel(const char* name);  // Lookup a label in current function
void insertStruct(const char* name, StructMember* members, int member_count);
void insertUnion(const char* name, StructMember* members, int member_count);
StructDef* lookupStruct(const char* name);
StructDef* lookupUnion(const char* name);
int getStructSize(const char* struct_name);
int getUnionSize(const char* union_name);
Symbol* lookupSymbol(const char* name);
void enterScope();
void exitScope();
void enterFunctionScope(const char* func_name);  // Enter scope for a specific function
void exitFunctionScope();
void insertSymbol(const char* name, const char* type, int is_function, int is_static);
void registerFunctionPointer(const char* name);  // Register a function pointer for IR generation
int isFunctionPointerName(const char* name);  // Check if a name is a registered function pointer
void moveRecentSymbolsToCurrentScope(int count);  // Move last N non-function symbols to current scope
void markRecentSymbolsAsParameters(int count);    // Mark last N symbols as parameters
int is_type_name(const char* name);
int getTypeSize(const char* type);
int isArithmeticType(const char* type);
int isIntegerType(const char* type);
char* usualArithConv(const char* t1, const char* t2);
int isAssignable(const char* lhs_type, const char* rhs_type);
void setCurrentType(const char* type);
void printSymbolTable();

// ===== ENHANCED TYPE CHECKING FUNCTIONS =====

// Type checking results
typedef enum {
    TYPE_OK,
    TYPE_ERROR,
    TYPE_WARNING
} TypeCheckResult;

// Enhanced type checking functions
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
