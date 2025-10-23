#include "symbol_table.h"
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <cstdarg>

using namespace std;

extern int yylineno;

Symbol symtab[MAX_SYMBOLS];
int symCount = 0;
int current_scope = 0;
int parent_scopes[100];  // Stack to track parent scope relationships
int scope_depth = 0;     // Current depth in scope stack
int current_offset = 0;
char currentType[128] = "int";
char current_function[128] = "";  // Track which function we're currently in
int next_scope = 1;               // Always use 1 for function scopes (not sequential)

// Struct definition table
StructDef structTable[MAX_STRUCTS];
int structCount = 0;

// Union definition table
StructDef unionTable[MAX_STRUCTS];
int unionCount = 0;

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

// Insert union definition (size = max of member sizes)
void insertUnion(const char* name, StructMember* members, int member_count) {
    if (unionCount >= MAX_STRUCTS) {
        cerr << "Error: Union table overflow" << endl;
        return;
    }
    
    strcpy(unionTable[unionCount].name, name);
    unionTable[unionCount].member_count = member_count;
    
    int maxSize = 0;
    for (int i = 0; i < member_count; i++) {
        unionTable[unionCount].members[i] = members[i];
        unionTable[unionCount].members[i].offset = 0;  // All union members start at offset 0
        
        // Calculate size based on type
        int memberSize = getTypeSize(members[i].type);
        unionTable[unionCount].members[i].size = memberSize;
        
        // Union size is the maximum of all member sizes
        if (memberSize > maxSize) {
            maxSize = memberSize;
        }
    }
    unionTable[unionCount].total_size = maxSize;
    unionCount++;
}

StructDef* lookupUnion(const char* name) {
    for (int i = 0; i < unionCount; i++) {
        if (strcmp(unionTable[i].name, name) == 0) {
            return &unionTable[i];
        }
    }
    return NULL;
}

int getUnionSize(const char* union_name) {
    // Extract union name from "union UnionName" format
    const char* name = union_name;
    if (strncmp(union_name, "union ", 6) == 0) {
        name = union_name + 6;
    }
    
    StructDef* def = lookupUnion(name);
    if (def) {
        return def->total_size;
    }
    return 0;  // Unknown union
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
    
    // Check if it's a union type
    if (strncmp(type, "union ", 6) == 0) {
        return getUnionSize(type);
    }
    
    // Check if it's a typedef - resolve to underlying type
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, type) == 0 && strcmp(symtab[i].kind, "typedef") == 0) {
            // Found typedef, get size of underlying type
            return getTypeSize(symtab[i].type);
        }
    }
    
    return 4;
}

void insertVariable(const char* name, const char* type, int is_array, int* dims, int num_dims, int ptr_level, int is_static) {
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
        if (is_static) {
            strcpy(symtab[symCount].kind, "variable (static)");
        } else {
            strcpy(symtab[symCount].kind, "variable");
        }
    } else if (ptr_level > 0) {
        // Clean up pointer type - remove extra spaces between * symbols
        char cleaned_type[256];
        int src_idx = 0, dst_idx = 0;
        while (type[src_idx] != '\0') {
            if (type[src_idx] == ' ' && (src_idx == 0 || type[src_idx-1] == '*' || type[src_idx+1] == '*')) {
                // Skip spaces adjacent to asterisks
                src_idx++;
            } else {
                cleaned_type[dst_idx++] = type[src_idx++];
            }
        }
        cleaned_type[dst_idx] = '\0';
        
        strcpy(symtab[symCount].type, cleaned_type);
        if (is_static) {
            strcpy(symtab[symCount].kind, "variable (static)");
        } else {
            strcpy(symtab[symCount].kind, "variable");
        }
    } else {
        strcpy(symtab[symCount].type, type);
        if (is_static) {
            strcpy(symtab[symCount].kind, "variable (static)");
        } else {
            strcpy(symtab[symCount].kind, "variable");
        }
    }
    
    symtab[symCount].scope_level = current_scope;
    symtab[symCount].parent_scope = (scope_depth > 0) ? parent_scopes[scope_depth - 1] : -1;
    symtab[symCount].offset = current_offset;
    symtab[symCount].is_array = is_array;
    symtab[symCount].ptr_level = ptr_level;
    symtab[symCount].is_function = 0;
    symtab[symCount].is_external = 0;  // Variables are never external
    symtab[symCount].is_static = is_static;  // Set static flag
    
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
    
    // Clean up pointer type - remove extra spaces between * symbols
    char cleaned_type[256];
    if (ptr_level > 0) {
        int src_idx = 0, dst_idx = 0;
        while (type[src_idx] != '\0') {
            if (type[src_idx] == ' ' && (src_idx == 0 || type[src_idx-1] == '*' || type[src_idx+1] == '*')) {
                // Skip spaces adjacent to asterisks
                src_idx++;
            } else {
                cleaned_type[dst_idx++] = type[src_idx++];
            }
        }
        cleaned_type[dst_idx] = '\0';
    } else {
        strcpy(cleaned_type, type);
    }
    
    strcpy(symtab[symCount].name, name);
    strcpy(symtab[symCount].type, cleaned_type);
    strcpy(symtab[symCount].kind, "parameter");  // Mark as parameter, not variable
    symtab[symCount].scope_level = current_scope;
    symtab[symCount].parent_scope = (scope_depth > 0) ? parent_scopes[scope_depth - 1] : -1;
    symtab[symCount].offset = current_offset;
    symtab[symCount].is_array = 0;
    symtab[symCount].ptr_level = ptr_level;
    symtab[symCount].is_function = 0;
    symtab[symCount].is_external = 0;  // Parameters are never external
    symtab[symCount].is_static = 0;  // Parameters are never static
    
    // Set the function scope name
    strcpy(symtab[symCount].function_scope, current_function);
    
    // Get size based on type (handles pointers correctly)
    symtab[symCount].size = getTypeSize(type);
    
    current_offset += symtab[symCount].size;
    symCount++;
}

void insertFunction(const char* name, const char* ret_type, int param_count, char params[][128], char param_names[][128], int is_static) {
    if (symCount >= MAX_SYMBOLS) {
        cerr << "Error: Symbol table overflow" << endl;
        return;
    }
    
    strcpy(symtab[symCount].name, name);
    strcpy(symtab[symCount].return_type, ret_type);
    if (is_static) {
        strcpy(symtab[symCount].kind, "function (static)");
    } else {
        strcpy(symtab[symCount].kind, "function");
    }
    symtab[symCount].scope_level = 0;  // Functions are always at global scope
    symtab[symCount].parent_scope = -1; // No parent for global scope
    symtab[symCount].is_function = 1;
    symtab[symCount].param_count = param_count;
    strcpy(symtab[symCount].function_scope, "");  // Functions are in global scope
    symtab[symCount].is_external = 0;  // User-defined functions are not external
    symtab[symCount].is_static = is_static;  // Set static flag
    symtab[symCount].size = 0;  // Functions have size 0
    
    if (params && param_names) {
        for (int i = 0; i < param_count; i++) {
            strcpy(symtab[symCount].param_types[i], params[i]);
            strcpy(symtab[symCount].param_names[i], param_names[i]);
        }
    }
    
    symCount++;
}

void insertExternalFunction(const char* name, const char* ret_type) {
    if (symCount >= MAX_SYMBOLS) {
        cerr << "Error: Symbol table overflow" << endl;
        return;
    }
    
    // Determine correct return type for known library functions
    const char* actual_ret_type = ret_type;
    char void_ptr_type[16] = "void*";  // No space for consistency
    
    if (strcmp(name, "malloc") == 0 || strcmp(name, "calloc") == 0 || 
        strcmp(name, "realloc") == 0) {
        actual_ret_type = void_ptr_type;
    } else if (strcmp(name, "free") == 0 || strcmp(name, "exit") == 0) {
        actual_ret_type = "void";
    }
    // printf, scanf, etc. default to int (which is correct)
    
    strcpy(symtab[symCount].name, name);
    strcpy(symtab[symCount].return_type, actual_ret_type);
    strcpy(symtab[symCount].kind, "function (external)");  // Mark as external in kind
    symtab[symCount].scope_level = 0;  // Functions are always at global scope
    symtab[symCount].parent_scope = -1; // No parent for global scope
    symtab[symCount].is_function = 1;
    symtab[symCount].param_count = 0;  // Don't know params for external functions
    strcpy(symtab[symCount].function_scope, "");  // Functions are in global scope
    symtab[symCount].is_external = 1;  // Mark as external/library function
    symtab[symCount].is_static = 0;  // External functions are not static
    symtab[symCount].size = 0;  // Functions have size 0
    
    symCount++;
}

void insertLabel(const char* name) {
    if (symCount >= MAX_SYMBOLS) {
        fprintf(stderr, "Error: Symbol table overflow\n");
        return;
    }
    
    strcpy(symtab[symCount].name, name);
    strcpy(symtab[symCount].type, "-");  // Labels don't have a type
    strcpy(symtab[symCount].kind, "label");
    symtab[symCount].scope_level = current_scope;
    symtab[symCount].parent_scope = (scope_depth > 0) ? parent_scopes[scope_depth - 1] : -1;
    symtab[symCount].is_function = 0;
    symtab[symCount].is_array = 0;
    symtab[symCount].ptr_level = 0;
    symtab[symCount].num_dims = 0;
    strcpy(symtab[symCount].function_scope, current_function);
    symtab[symCount].is_external = 0;
    symtab[symCount].is_static = 0;
    symtab[symCount].size = 0;  // Labels don't occupy storage
    
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
    // All functions use scope level 1 (not sequential)
    current_scope = 1;
    
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
    // For nested blocks inside functions - use scope level 2
    // Push current scope onto parent stack
    if (scope_depth < 100) {
        parent_scopes[scope_depth] = current_scope;
        scope_depth++;
    }
    current_scope = 2;  // Nested blocks use scope 2
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

void insertSymbol(const char* name, const char* type, int is_function, int is_static) {
    if (is_function) {
        insertFunction(name, type, 0, NULL, NULL, is_static);
    } else {
        insertVariable(name, type, 0, NULL, 0, 0, is_static);
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

// Helper function to clean up typedef type display
// Removes anonymous struct/union tags for cleaner output
const char* getDisplayType(const Symbol* sym) {
    static char display_type[128];
    
    // If it's a typedef, clean up the type string
    if (strcmp(sym->kind, "typedef") == 0) {
        // Check for anonymous union
        if (strncmp(sym->type, "union __anon_union_", 19) == 0) {
            strcpy(display_type, "union");
            return display_type;
        }
        // Check for anonymous struct
        if (strncmp(sym->type, "struct __anon_struct_", 21) == 0) {
            strcpy(display_type, "struct");
            return display_type;
        }
    }
    
    // For all other cases, return the type as-is
    return sym->type;
}

void printSymbolTable() {
    cout << "\n=== SYMBOL TABLE (Hierarchical Scope View) ===" << endl;
    cout << left << setw(20) << "Name"
         << "  " << setw(20) << "Type"
         << "  " << setw(20) << "Kind"
         << "  " << right << setw(5) << "Scope"
         << "  " << left << setw(10) << "Parent"
         << "  " << right << setw(5) << "Size" << endl;
    cout << "------------------------------------------------------------------------------------------------" << endl;
    
    // Print global scope (0)
    cout << ">>> GLOBAL SCOPE (0) <<<" << endl;
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level == 0) {
            const char* display_type;
            if (symtab[i].is_function) {
                display_type = symtab[i].return_type;
            } else {
                display_type = getDisplayType(&symtab[i]);
            }
            
            cout << left << setw(20) << symtab[i].name
                 << "  " << setw(20) << display_type
                 << "  " << setw(20) << symtab[i].kind
                 << "  " << right << setw(5) << symtab[i].scope_level;
            
            // Show "external" for library functions, "none" for user-defined
            if (symtab[i].is_external) {
                cout << "  " << left << setw(10) << "external";
            } else {
                cout << "  " << left << setw(10) << "none";
            }
            
            // Show "-" for labels, actual size for others
            if (strcmp(symtab[i].kind, "label") == 0) {
                cout << "  " << right << setw(5) << "-" << endl;
            } else {
                cout << "  " << right << setw(5) << symtab[i].size << endl;
            }
        }
    }
    
    // Print function scope (1) - group by function
    bool has_function_scope = false;
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level == 1) {
            has_function_scope = true;
            break;
        }
    }
    
    if (has_function_scope) {
        // Group by function name
        char processed_functions[100][128];
        int processed_count = 0;
        
        for (int i = 0; i < symCount; i++) {
            if (symtab[i].scope_level == 1 && strlen(symtab[i].function_scope) > 0) {
                const char* func_name = symtab[i].function_scope;
                
                // Check if we've already processed this function
                bool already_processed = false;
                for (int j = 0; j < processed_count; j++) {
                    if (strcmp(processed_functions[j], func_name) == 0) {
                        already_processed = true;
                        break;
                    }
                }
                
                if (!already_processed) {
                    // Add to processed list
                    strcpy(processed_functions[processed_count], func_name);
                    processed_count++;
                    
                    // Print header for this function
                    cout << ">>> SCOPE LEVEL 1 (" << func_name << ") <<<" << endl;
                    
                    // Print all symbols in this function's scope
                    for (int j = 0; j < symCount; j++) {
                        if (symtab[j].scope_level == 1 && strcmp(symtab[j].function_scope, func_name) == 0) {
                            cout << "  " << left << setw(18) << symtab[j].name
                                 << "  " << setw(20) << getDisplayType(&symtab[j])
                                 << "  " << setw(20) << symtab[j].kind
                                 << "  " << right << setw(5) << symtab[j].scope_level
                                 << "  " << left << setw(10) << func_name;
                            
                            // Show "-" for labels, actual size for others
                            if (strcmp(symtab[j].kind, "label") == 0) {
                                cout << "  " << right << setw(5) << "-" << endl;
                            } else {
                                cout << "  " << right << setw(5) << symtab[j].size << endl;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Print nested block scope (2)
    bool has_nested_scope = false;
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level == 2) {
            has_nested_scope = true;
            break;
        }
    }
    
    if (has_nested_scope) {
        // Group by function name
        char processed_functions[100][128];
        int processed_count = 0;
        
        for (int i = 0; i < symCount; i++) {
            if (symtab[i].scope_level == 2 && strlen(symtab[i].function_scope) > 0) {
                const char* func_name = symtab[i].function_scope;
                
                // Check if we've already processed this function
                bool already_processed = false;
                for (int j = 0; j < processed_count; j++) {
                    if (strcmp(processed_functions[j], func_name) == 0) {
                        already_processed = true;
                        break;
                    }
                }
                
                if (!already_processed) {
                    // Add to processed list
                    strcpy(processed_functions[processed_count], func_name);
                    processed_count++;
                    
                    // Print header for this function's nested blocks
                    cout << ">>> SCOPE LEVEL 2 (" << func_name << " - nested) <<<" << endl;
                    
                    // Print all symbols in this function's nested scope
                    for (int j = 0; j < symCount; j++) {
                        if (symtab[j].scope_level == 2 && strcmp(symtab[j].function_scope, func_name) == 0) {
                            cout << "    " << left << setw(16) << symtab[j].name
                                 << "  " << setw(20) << getDisplayType(&symtab[j])
                                 << "  " << setw(20) << symtab[j].kind
                                 << "  " << right << setw(5) << symtab[j].scope_level
                                 << "  " << left << setw(10) << func_name;  // Show function name instead of numeric parent_scope
                            
                            // Show "-" for labels, actual size for others
                            if (strcmp(symtab[j].kind, "label") == 0) {
                                cout << "  " << right << setw(5) << "-" << endl;
                            } else {
                                cout << "  " << right << setw(5) << symtab[j].size << endl;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Find max scope level
    int max_scope = 0;
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level > max_scope) {
            max_scope = symtab[i].scope_level;
        }
    }
    
    cout << "------------------------------------------------------------------------------------------------" << endl;
    cout << "Total symbols: " << symCount << " | Max scope level: " << max_scope << endl << endl;
}

// ===== ENHANCED TYPE CHECKING FUNCTIONS =====

// Enhanced binary operator type checking
TypeCheckResult checkBinaryOp(const char* op, TreeNode* left, TreeNode* right, char** result_type) {
    if (!left->dataType || !right->dataType) {
        *result_type = strdup("int"); // fallback
        return TYPE_ERROR;
    }
    
    const char* ltype = left->dataType;
    const char* rtype = right->dataType;
    
    // Arithmetic operators: +, -, *, /, %
    if (strcmp(op, "+") == 0) {
        // Case 1: both arithmetic
        if (isArithmeticType(ltype) && isArithmeticType(rtype)) {
            *result_type = strdup(usualArithConv(ltype, rtype));
            return TYPE_OK;
        }
        // Case 2: pointer + integer
        if (strstr(ltype, "*") && isArithmeticType(rtype)) {
            *result_type = strdup(ltype);
            return TYPE_OK;
        }
        // Case 3: integer + pointer
        if (isArithmeticType(ltype) && strstr(rtype, "*")) {
            *result_type = strdup(rtype);
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '+' (have '%s' and '%s')", ltype, rtype);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "-") == 0) {
        // Case 1: both arithmetic
        if (isArithmeticType(ltype) && isArithmeticType(rtype)) {
            *result_type = strdup(usualArithConv(ltype, rtype));
            return TYPE_OK;
        }
        // Case 2: pointer - integer
        if (strstr(ltype, "*") && isArithmeticType(rtype)) {
            *result_type = strdup(ltype);
            return TYPE_OK;
        }
        // Case 3: pointer - pointer (same base type)
        if (strstr(ltype, "*") && strstr(rtype, "*")) {
            if (isPointerCompatible(ltype, rtype)) {
                *result_type = strdup("int"); // ptrdiff_t
                return TYPE_OK;
            }
            type_error(yylineno, "invalid operands to binary '-' (incompatible pointer types)");
            return TYPE_ERROR;
        }
        type_error(yylineno, "invalid operands to binary '-' (have '%s' and '%s')", ltype, rtype);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
        if (isArithmeticType(ltype) && isArithmeticType(rtype)) {
            *result_type = strdup(usualArithConv(ltype, rtype));
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "%") == 0) {
        if (isArithmeticType(ltype) && isArithmeticType(rtype)) {
            *result_type = strdup("int");
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%%' (have '%s' and '%s')", ltype, rtype);
        return TYPE_ERROR;
    }
    
    // Relational operators: <, >, <=, >=
    if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        // Both arithmetic OR both compatible pointers
        if ((isArithmeticType(ltype) && isArithmeticType(rtype)) ||
            (strstr(ltype, "*") && strstr(rtype, "*") && isPointerCompatible(ltype, rtype))) {
            *result_type = strdup("int");
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        return TYPE_ERROR;
    }
    
    // Equality operators: ==, !=
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
        // Both arithmetic, or both pointers, or pointer vs null
        if ((isArithmeticType(ltype) && isArithmeticType(rtype)) ||
            (strstr(ltype, "*") && strstr(rtype, "*")) ||
            (strstr(ltype, "*") && isNullPointer(right)) ||
            (isNullPointer(left) && strstr(rtype, "*"))) {
            *result_type = strdup("int");
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        return TYPE_ERROR;
    }
    
    // Bitwise operators: &, |, ^, <<, >>
    if (strcmp(op, "&") == 0 || strcmp(op, "|") == 0 || strcmp(op, "^") == 0) {
        if (isArithmeticType(ltype) && isArithmeticType(rtype)) {
            *result_type = strdup("int");
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0) {
        if (isArithmeticType(ltype) && isArithmeticType(rtype)) {
            *result_type = strdup(ltype); // Result type is left operand type
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        return TYPE_ERROR;
    }
    
    // Logical operators: &&, ||
    if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        // Any scalar type can be converted to boolean
        *result_type = strdup("int");
        return TYPE_OK;
    }
    
    *result_type = strdup("int");
    return TYPE_ERROR;
}

// Enhanced unary operator type checking
TypeCheckResult checkUnaryOp(const char* op, TreeNode* operand, char** result_type) {
    if (!operand->dataType) {
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    const char* optype = operand->dataType;
    
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) {
        if (isArithmeticType(optype)) {
            *result_type = strdup(optype);
            return TYPE_OK;
        }
        type_error(yylineno, "wrong type argument to unary '%s' (have '%s')", op, optype);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "!") == 0) {
        // Any scalar type can be converted to boolean
        *result_type = strdup("int");
        return TYPE_OK;
    }
    
    if (strcmp(op, "~") == 0) {
        if (isArithmeticType(optype)) {
            *result_type = strdup("int");
            return TYPE_OK;
        }
        type_error(yylineno, "wrong type argument to unary '~' (have '%s')", optype);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "*") == 0) {
        if (strstr(optype, "*")) {
            // Remove one level of pointer indirection
            // Simplified: assume result is int for now
            *result_type = strdup("int");
            return TYPE_OK;
        }
        type_error(yylineno, "invalid type argument of unary '*' (have '%s')", optype);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "&") == 0) {
        if (!operand->isLValue) {
            type_error(yylineno, "lvalue required as unary '&' operand");
            return TYPE_ERROR;
        }
        // Create pointer type
        char* ptr_type = (char*)malloc(strlen(optype) + 3);
        sprintf(ptr_type, "%s*", optype);
        *result_type = ptr_type;
        return TYPE_OK;
    }
    
    if (strcmp(op, "++") == 0 || strcmp(op, "--") == 0) {
        if (!operand->isLValue) {
            type_error(yylineno, "lvalue required as %s operand", op);
            return TYPE_ERROR;
        }
        if (isArithmeticType(optype) || strstr(optype, "*")) {
            *result_type = strdup(optype);
            return TYPE_OK;
        }
        type_error(yylineno, "wrong type argument to %s (have '%s')", op, optype);
        return TYPE_ERROR;
    }
    
    *result_type = strdup("int");
    return TYPE_ERROR;
}

// Enhanced assignment checking
TypeCheckResult checkAssignment(TreeNode* lhs, TreeNode* rhs) {
    if (!lhs->isLValue) {
        type_error(yylineno, "lvalue required as left operand of assignment");
        return TYPE_ERROR;
    }
    
    if (!lhs->dataType || !rhs->dataType) {
        return TYPE_ERROR;
    }
    
    const char* ltype = lhs->dataType;
    const char* rtype = rhs->dataType;
    
    // Same type
    if (strcmp(ltype, rtype) == 0) {
        return TYPE_OK;
    }
    
    // Arithmetic conversion
    if (isArithmeticType(ltype) && isArithmeticType(rtype)) {
        if (strcmp(ltype, "char") == 0 && strcmp(rtype, "int") == 0) {
            type_warning(yylineno, "conversion from 'int' to 'char' may alter value");
        }
        return TYPE_OK;
    }
    
    // Pointer assignments
    if (strstr(ltype, "*") && strstr(rtype, "*")) {
        if (isPointerCompatible(ltype, rtype)) {
            return TYPE_OK;
        }
        type_warning(yylineno, "assignment from incompatible pointer type");
        return TYPE_WARNING;
    }
    
    // Pointer = NULL
    if (strstr(ltype, "*") && isNullPointer(rhs)) {
        return TYPE_OK;
    }
    
    // Pointer = integer (error unless 0)
    if (strstr(ltype, "*") && isArithmeticType(rtype)) {
        type_error(yylineno, "assignment makes pointer from integer without a cast");
        return TYPE_ERROR;
    }
    
    // Integer = pointer (error)
    if (isArithmeticType(ltype) && strstr(rtype, "*")) {
        type_error(yylineno, "assignment makes integer from pointer without a cast");
        return TYPE_ERROR;
    }
    
    type_error(yylineno, "incompatible types when assigning to type '%s' from type '%s'", ltype, rtype);
    return TYPE_ERROR;
}

// Function call validation
TypeCheckResult checkFunctionCall(const char* func_name, TreeNode* args, char** result_type) {
    Symbol* func_sym = lookupSymbol(func_name);
    
    if (!func_sym) {
        type_error(yylineno, "implicit declaration of function '%s'", func_name);
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    if (!func_sym->is_function) {
        type_error(yylineno, "called object '%s' is not a function", func_name);
        return TYPE_ERROR;
    }
    
    *result_type = strdup(func_sym->return_type);
    
    // Check argument count for non-external functions
    if (!func_sym->is_external) {
        int provided_args = args ? args->childCount : 0;
        if (provided_args != func_sym->param_count) {
            type_error(yylineno, "too %s arguments to function '%s' (expected %d, got %d)",
                      provided_args < func_sym->param_count ? "few" : "many",
                      func_name, func_sym->param_count, provided_args);
            return TYPE_ERROR;
        }
        
        // Check argument types
        if (args) {
            for (int i = 0; i < args->childCount && i < func_sym->param_count; i++) {
                if (args->children[i]->dataType) {
                    if (!canImplicitConvert(args->children[i]->dataType, func_sym->param_types[i])) {
                        type_error(yylineno, "incompatible type for argument %d of '%s'", i+1, func_name);
                        return TYPE_ERROR;
                    }
                }
            }
        }
    }
    
    return TYPE_OK;
}

// Array access validation
TypeCheckResult checkArrayAccess(TreeNode* array, TreeNode* index, char** result_type) {
    if (!array->dataType || !index->dataType) {
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    // Index must be integer type
    if (!isArithmeticType(index->dataType)) {
        type_error(yylineno, "array subscript is not an integer");
        return TYPE_ERROR;
    }
    
    // Array must be array or pointer type
    const char* array_type = array->dataType;
    if (strstr(array_type, "[") || strstr(array_type, "*")) {
        // Simplified: assume element type is int
        *result_type = strdup("int");
        return TYPE_OK;
    }
    
    type_error(yylineno, "subscripted value is not an array or pointer");
    return TYPE_ERROR;
}

// Helper functions
int isPointerCompatible(const char* ptr1, const char* ptr2) {
    // Simplified: void* is compatible with any pointer
    if (strstr(ptr1, "void*") || strstr(ptr2, "void*")) {
        return 1;
    }
    // Otherwise, base types must match
    return strcmp(ptr1, ptr2) == 0;
}

int isNullPointer(TreeNode* expr) {
    if (expr->type == NODE_INTEGER_CONSTANT && strcmp(expr->value, "0") == 0) {
        return 1;
    }
    return 0;
}

int canImplicitConvert(const char* from_type, const char* to_type) {
    if (strcmp(from_type, to_type) == 0) return 1;
    if (isArithmeticType(from_type) && isArithmeticType(to_type)) return 1;
    if (strstr(from_type, "*") && strstr(to_type, "*")) return isPointerCompatible(from_type, to_type);
    return 0;
}

char* getCommonType(const char* type1, const char* type2) {
    if (strcmp(type1, type2) == 0) return strdup(type1);
    if (isArithmeticType(type1) && isArithmeticType(type2)) {
        return strdup(usualArithConv(type1, type2));
    }
    return strdup("int"); // fallback
}

// Error reporting
void type_error(int line, const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    fprintf(stderr, "Type Error on line %d: ", line);
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void type_warning(int line, const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    fprintf(stderr, "Type Warning on line %d: ", line);
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
    va_end(args);
}
