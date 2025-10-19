#include "symbol_table.h"
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>

using namespace std;

Symbol symtab[MAX_SYMBOLS];
int symCount = 0;
int current_scope = 0;
int current_offset = 0;
char currentType[128] = "int";

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
    if (symCount >= MAX_SYMBOLS) {
        cerr << "Error: Symbol table overflow" << endl;
        return;
    }
    
    // Check for duplicate in current scope
    for (int i = symCount - 1; i >= 0; i--) {
        if (strcmp(symtab[i].name, name) == 0 && symtab[i].scope_level == current_scope) {
            return; // Already exists
        }
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
    if (symCount >= MAX_SYMBOLS) {
        cerr << "Error: Symbol table overflow" << endl;
        return;
    }
    
    strcpy(symtab[symCount].name, name);
    strcpy(symtab[symCount].return_type, ret_type);
    strcpy(symtab[symCount].kind, "function");
    symtab[symCount].scope_level = 0;
    symtab[symCount].is_function = 1;
    symtab[symCount].param_count = param_count;
    
    if (params && param_names) {
        for (int i = 0; i < param_count; i++) {
            strcpy(symtab[symCount].param_types[i], params[i]);
            strcpy(symtab[symCount].param_names[i], param_names[i]);
        }
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

void enterScope() {
    current_scope++;
}

void exitScope() {
    while (symCount > 0 && symtab[symCount - 1].scope_level == current_scope) {
        symCount--;
    }
    current_scope--;
}

void insertSymbol(const char* name, const char* type, int is_function) {
    if (is_function) {
        insertFunction(name, type, 0, NULL, NULL);
    } else {
        insertVariable(name, type, 0, NULL, 0, 0);
    }
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

int isArithmeticType(const char* type) {
    return (strcmp(type, "int") == 0 || strcmp(type, "char") == 0 ||
            strcmp(type, "short") == 0 || strcmp(type, "long") == 0 ||
            strcmp(type, "float") == 0 || strcmp(type, "double") == 0);
}

char* usualArithConv(const char* t1, const char* t2) {
    static char result[32];
    if (strcmp(t1, "double") == 0 || strcmp(t2, "double") == 0) 
        strcpy(result, "double");
    else if (strcmp(t1, "float") == 0 || strcmp(t2, "float") == 0) 
        strcpy(result, "float");
    else if (strcmp(t1, "long") == 0 || strcmp(t2, "long") == 0) 
        strcpy(result, "long");
    else 
        strcpy(result, "int");
    return result;
}

int isAssignable(const char* lhs_type, const char* rhs_type) {
    if (strcmp(lhs_type, rhs_type) == 0) return 1;
    if (isArithmeticType(lhs_type) && isArithmeticType(rhs_type)) return 1;
    if (strstr(lhs_type, "*") && strstr(rhs_type, "*")) return 1;
    return 0;
}

void setCurrentType(const char* type) {
    strcpy(currentType, type);
}

void printSymbolTable() {
    cout << "\n=== SYMBOL TABLE ===" << endl;
    cout << left << setw(20) << "Name"
         << setw(15) << "Type"
         << setw(10) << "Kind"
         << setw(10) << "Scope"
         << setw(10) << "Size" << endl;
    cout << "------------------------------------------------------------------------" << endl;
    for (int i = 0; i < symCount; i++) {
        cout << left << setw(20) << symtab[i].name
             << setw(15) << (symtab[i].is_function ? symtab[i].return_type : symtab[i].type)
             << setw(10) << symtab[i].kind
             << setw(10) << symtab[i].scope_level
             << setw(10) << symtab[i].size << endl;
    }
    cout << "------------------------------------------------------------------------" << endl;
    cout << "Total symbols: " << symCount << endl << endl;
}