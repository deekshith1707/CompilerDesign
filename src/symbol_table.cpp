#include "symbol_table.h"
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>

using namespace std;

Symbol symtab[MAX_SYMBOLS];
int symCount = 0;
int current_scope = 0;
int parent_scopes[100];  // Stack to track parent scope relationships
int scope_depth = 0;     // Current depth in scope stack
int current_offset = 0;
char currentType[128] = "int";
char current_function[128] = "";  // Track which function we're currently in
int next_scope = 1;               // Next scope number to assign (starts at 1, 0 is global)

// Struct definition table
StructDef structTable[MAX_STRUCTS];
int structCount = 0;

void insertStruct(const char* name, StructMember* members, int member_count) {
    if (structCount >= MAX_STRUCTS) {
        cerr << "Error: Struct table overflow" << endl;
        return;
    }
    
    strcpy(structTable[structCount].name, name);
    structTable[structCount].member_count = member_count;
    
    int offset = 0;
    for (int i = 0; i < member_count; i++) {
        structTable[structCount].members[i] = members[i];
        structTable[structCount].members[i].offset = offset;
        
        // Calculate size based on type
        int memberSize = getTypeSize(members[i].type);
        structTable[structCount].members[i].size = memberSize;
        
        offset += memberSize;
    }
    structTable[structCount].total_size = offset;
    structCount++;
}

StructDef* lookupStruct(const char* name) {
    for (int i = 0; i < structCount; i++) {
        if (strcmp(structTable[i].name, name) == 0) {
            return &structTable[i];
        }
    }
    return NULL;
}

int getStructSize(const char* struct_name) {
    // Extract struct name from "struct StructName" format
    const char* name = struct_name;
    if (strncmp(struct_name, "struct ", 7) == 0) {
        name = struct_name + 7;
    }
    
    StructDef* def = lookupStruct(name);
    if (def) {
        return def->total_size;
    }
    return 0;  // Unknown struct
}

int getTypeSize(const char* type) {
    if (strcmp(type, "char") == 0) return 1;
    if (strcmp(type, "short") == 0) return 2;
    if (strcmp(type, "int") == 0) return 4;
    if (strcmp(type, "long") == 0) return 8;
    if (strcmp(type, "float") == 0) return 4;
    if (strcmp(type, "double") == 0) return 8;
    if (strstr(type, "*")) return POINTER_SIZE;  // Use constant for pointer size
    
    // Check if it's a struct type
    if (strncmp(type, "struct ", 7) == 0) {
        return getStructSize(type);
    }
    
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
    
    // Build proper type string
    char fullType[256];
    strcpy(fullType, type);
    
    // For arrays, append dimensions to type (e.g., "int[3]" or "int[3][4]")
    if (is_array && num_dims > 0) {
        for (int i = 0; i < num_dims; i++) {
            char dimStr[32];
            sprintf(dimStr, "[%d]", dims[i]);
            strcat(fullType, dimStr);
        }
        strcpy(symtab[symCount].type, fullType);
        strcpy(symtab[symCount].kind, "variable");  // Arrays are variables, not separate kind
    } else if (ptr_level > 0) {
        // Pointer already has * in type from parser
        strcpy(symtab[symCount].type, type);
        strcpy(symtab[symCount].kind, "pointer");
    } else {
        strcpy(symtab[symCount].type, type);
        strcpy(symtab[symCount].kind, "variable");
    }
    
    symtab[symCount].scope_level = current_scope;
    symtab[symCount].parent_scope = (scope_depth > 0) ? parent_scopes[scope_depth - 1] : -1;
    symtab[symCount].offset = current_offset;
    symtab[symCount].is_array = is_array;
    symtab[symCount].ptr_level = ptr_level;
    symtab[symCount].is_function = 0;
    
    // Set the function scope name
    strcpy(symtab[symCount].function_scope, current_function);
    
    // Calculate size
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

void insertParameter(const char* name, const char* type, int ptr_level) {
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
    strcpy(symtab[symCount].kind, "parameter");  // Mark as parameter, not variable
    symtab[symCount].scope_level = current_scope;
    symtab[symCount].parent_scope = (scope_depth > 0) ? parent_scopes[scope_depth - 1] : -1;
    symtab[symCount].offset = current_offset;
    symtab[symCount].is_array = 0;
    symtab[symCount].ptr_level = ptr_level;
    symtab[symCount].is_function = 0;
    
    // Set the function scope name
    strcpy(symtab[symCount].function_scope, current_function);
    
    // Get size based on type (handles pointers correctly)
    symtab[symCount].size = getTypeSize(type);
    
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
    symtab[symCount].scope_level = 0;  // Functions are always at global scope
    symtab[symCount].parent_scope = -1; // No parent for global scope
    symtab[symCount].is_function = 1;
    symtab[symCount].param_count = param_count;
    strcpy(symtab[symCount].function_scope, "");  // Functions are in global scope
    
    if (params && param_names) {
        for (int i = 0; i < param_count; i++) {
            strcpy(symtab[symCount].param_types[i], params[i]);
            strcpy(symtab[symCount].param_names[i], param_names[i]);
        }
    }
    
    symCount++;
}

Symbol* lookupSymbol(const char* name) {
    // Search from innermost scope outward to global scope
    for (int i = symCount - 1; i >= 0; i--) {
        if (strcmp(symtab[i].name, name) == 0) {
            // Check if this symbol is in current scope or any parent scope
            int check_scope = current_scope;
            while (check_scope >= 0) {
                if (symtab[i].scope_level == check_scope) {
                    return &symtab[i];
                }
                // Move to parent scope
                if (check_scope == 0) break;
                // Find parent scope by looking up the scope stack
                bool found_parent = false;
                for (int j = scope_depth - 1; j >= 0; j--) {
                    if (parent_scopes[j] < check_scope) {
                        check_scope = parent_scopes[j];
                        found_parent = true;
                        break;
                    }
                }
                if (!found_parent) check_scope = 0; // Go to global
            }
        }
    }
    return NULL;
}

void enterFunctionScope(const char* func_name) {
    // Each function gets the next sequential scope number
    current_scope = next_scope;
    next_scope++;
    
    // Track the function name
    strcpy(current_function, func_name);
    
    // Push onto scope stack
    if (scope_depth < 100) {
        parent_scopes[scope_depth] = 0;  // Parent is always global scope (0)
        scope_depth++;
    }
}

void exitFunctionScope() {
    // Exit function scope
    if (scope_depth > 0) {
        scope_depth--;
    }
    current_scope = 0;  // Return to global scope
    strcpy(current_function, "");  // Clear current function
}

void enterScope() {
    // For nested blocks inside functions - also gets next sequential number
    // Push current scope onto parent stack
    if (scope_depth < 100) {
        parent_scopes[scope_depth] = current_scope;
        scope_depth++;
    }
    current_scope = next_scope;
    next_scope++;
}

void exitScope() {
    // Don't remove symbols - keep them for symbol table display
    // Just update the current scope level
    if (scope_depth > 0) {
        scope_depth--;
        current_scope = parent_scopes[scope_depth];
    } else {
        current_scope = 0;
    }
}

void moveRecentSymbolsToCurrentScope(int count) {
    // Move the last 'count' non-function symbols to the current scope
    // This is used to move function parameters from global scope to function scope
    int moved = 0;
    for (int i = symCount - 1; i >= 0 && moved < count; i--) {
        if (!symtab[i].is_function && symtab[i].scope_level == 0) {
            symtab[i].scope_level = current_scope;
            symtab[i].parent_scope = (scope_depth > 0) ? parent_scopes[scope_depth - 1] : -1;
            strcpy(symtab[i].function_scope, current_function);  // Set function scope
            moved++;
        }
    }
}

void markRecentSymbolsAsParameters(int count) {
    // Mark the last 'count' non-function symbols as parameters
    // This is used after moving parameters to function scope
    int marked = 0;
    for (int i = symCount - 1; i >= 0 && marked < count; i--) {
        if (!symtab[i].is_function && symtab[i].scope_level == current_scope) {
            strcpy(symtab[i].kind, "parameter");
            marked++;
        }
    }
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
    cout << "\n=== SYMBOL TABLE (Hierarchical Scope View) ===" << endl;
    cout << left << setw(20) << "Name"
         << setw(15) << "Type"
         << setw(10) << "Kind"
         << setw(10) << "Scope"
         << setw(12) << "Parent"
         << setw(10) << "Size" << endl;
    cout << "--------------------------------------------------------------------------------" << endl;
    
    // Find the maximum scope level in the symbol table
    int max_scope = 0;
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level > max_scope) {
            max_scope = symtab[i].scope_level;
        }
    }
    
    // Group by scope level for better visualization
    for (int scope = 0; scope <= max_scope; scope++) {
        bool has_symbols = false;
        for (int i = 0; i < symCount; i++) {
            if (symtab[i].scope_level == scope) {
                has_symbols = true;
                break;
            }
        }
        
        if (has_symbols) {
            if (scope == 0) {
                cout << ">>> GLOBAL SCOPE (0) <<<" << endl;
            } else {
                // Find the function name and determine scope type
                const char* func_name = "";
                bool has_params = false;
                bool has_vars = false;
                
                for (int i = 0; i < symCount; i++) {
                    if (symtab[i].scope_level == scope) {
                        if (strlen(symtab[i].function_scope) > 0) {
                            func_name = symtab[i].function_scope;
                        }
                        if (strcmp(symtab[i].kind, "parameter") == 0) {
                            has_params = true;
                        }
                        if (strcmp(symtab[i].kind, "variable") == 0 || strcmp(symtab[i].kind, "pointer") == 0) {
                            has_vars = true;
                        }
                    }
                }
                
                // Build descriptive scope header
                if (strlen(func_name) > 0) {
                    cout << ">>> SCOPE LEVEL " << scope << " (" << func_name;
                    if (has_params && !has_vars) {
                        cout << " - parameters";
                    } else if (has_vars && !has_params) {
                        cout << " - locals";
                    }
                    cout << ") <<<" << endl;
                } else {
                    cout << ">>> SCOPE LEVEL " << scope << " <<<" << endl;
                }
            }
            
            for (int i = 0; i < symCount; i++) {
                if (symtab[i].scope_level == scope) {
                    // Add indentation based on whether it's global or not
                    string indent = (scope == 0) ? "" : "  ";
                    cout << indent << left << setw(20 - indent.length()) << symtab[i].name
                         << setw(15) << (symtab[i].is_function ? symtab[i].return_type : symtab[i].type)
                         << setw(10) << symtab[i].kind
                         << setw(10) << symtab[i].scope_level;
                    
                    // Show parent as function name or scope number
                    if (symtab[i].parent_scope == -1) {
                        cout << setw(12) << "none";
                    } else if (strlen(symtab[i].function_scope) > 0 && symtab[i].parent_scope == 0) {
                        // If parent is global and we have a function scope, show function name
                        cout << setw(12) << symtab[i].function_scope;
                    } else {
                        // Show parent scope number for nested blocks
                        cout << setw(12) << symtab[i].parent_scope;
                    }
                    
                    cout << setw(10) << symtab[i].size << endl;
                }
            }
        }
    }
    
    cout << "--------------------------------------------------------------------------------" << endl;
    cout << "Total symbols: " << symCount << " | Max scope level: " << max_scope << endl << endl;
}
