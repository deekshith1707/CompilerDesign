#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#define MAX_SYMBOLS 2000

typedef struct Symbol {
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
} Symbol;

// Global symbol table
extern Symbol symtab[MAX_SYMBOLS];
extern int symCount;
extern int current_scope;
extern int current_offset;
extern char currentType[128];

// Function prototypes
void insertVariable(const char* name, const char* type, int is_array, int* dims, int num_dims, int ptr_level);
void insertFunction(const char* name, const char* ret_type, int param_count, char params[][128], char param_names[][128]);
Symbol* lookupSymbol(const char* name);
void enterScope();
void exitScope();
void insertSymbol(const char* name, const char* type, int is_function);
int is_type_name(const char* name);
int getTypeSize(const char* type);
int isArithmeticType(const char* type);
char* usualArithConv(const char* t1, const char* t2);
int isAssignable(const char* lhs_type, const char* rhs_type);
void setCurrentType(const char* type);
void printSymbolTable();

#endif // SYMBOL_TABLE_H