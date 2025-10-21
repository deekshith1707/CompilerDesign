#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SYMBOLS 2000
#define POINTER_SIZE 8  // 8 bytes for 64-bit pointers, change to 4 for 32-bit

typedef struct Symbol {
    char name[128];
    char type[128];
    char kind[32];
    int scope_level;      // Hierarchical scope: 0=global, 1=function body, 2+=nested blocks
    int parent_scope;     // Parent scope level (-1 for global scope)
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
} Symbol;

// Global symbol table
extern Symbol symtab[MAX_SYMBOLS];
extern int symCount;
extern int current_scope;
extern int parent_scopes[100];  // Stack to track parent scope relationships
extern int scope_depth;         // Current depth in scope stack
extern int current_offset;
extern char currentType[128];

// Function prototypes
void insertVariable(const char* name, const char* type, int is_array, int* dims, int num_dims, int ptr_level);
void insertParameter(const char* name, const char* type, int ptr_level);  // For function parameters
void insertFunction(const char* name, const char* ret_type, int param_count, char params[][128], char param_names[][128]);
Symbol* lookupSymbol(const char* name);
void enterScope();
void exitScope();
void insertSymbol(const char* name, const char* type, int is_function);
void moveRecentSymbolsToCurrentScope(int count);  // Move last N non-function symbols to current scope
void markRecentSymbolsAsParameters(int count);    // Mark last N symbols as parameters
int is_type_name(const char* name);
int getTypeSize(const char* type);
int isArithmeticType(const char* type);
char* usualArithConv(const char* t1, const char* t2);
int isAssignable(const char* lhs_type, const char* rhs_type);
void setCurrentType(const char* type);
void printSymbolTable();

#ifdef __cplusplus
}
#endif

#endif // SYMBOL_TABLE_H
