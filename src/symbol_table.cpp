#include "symbol_table.h"
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <cstdarg>

using namespace std;

extern int yylineno;
extern int error_count;

Symbol symtab[MAX_SYMBOLS];
int symCount = 0;
int current_scope = 0;
int parent_scopes[100];  // Stack to track parent scope relationships
int scope_depth = 0;     // Current depth in scope stack
int current_offset = 0;
char currentType[128] = "int";
char current_function[128] = "";  // Track which function we're currently in
int next_scope = 1;               // Always use 1 for function scopes (not sequential)
int current_block_id = 0;         // Current nested block ID
int next_block_id = 1;            // Counter for unique block IDs
int parent_blocks[100];           // Stack of parent block IDs

// Struct definition table
StructDef structTable[MAX_STRUCTS];
int structCount = 0;

// Union definition table
StructDef unionTable[MAX_STRUCTS];
int unionCount = 0;

// Function pointer registry
char function_pointers[MAX_SYMBOLS][128];
int function_pointer_count = 0;

// Format function pointer type string
void formatFunctionPointerType(Symbol* sym, char* out, int out_size) {
    snprintf(out, out_size, "%s (*)(", sym->return_type);
    int len = strlen(out);
    for (int i = 0; i < sym->param_count; i++) {
        if (i > 0) {
            strncat(out, ", ", out_size - len - 1);
            len = strlen(out);
        }
        strncat(out, sym->param_types[i], out_size - len - 1);
        len = strlen(out);
    }
    strncat(out, ")", out_size - len - 1);
}

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
    
    int saved_scope = current_scope;
    int saved_depth = scope_depth;
    current_scope = 0;
    scope_depth = 0;
    
    // Insert struct tag as a type symbol in global scope
    if (symCount < MAX_SYMBOLS) {
        strcpy(symtab[symCount].name, name);
        sprintf(symtab[symCount].type, "struct %s", name);
        strcpy(symtab[symCount].kind, "struct_tag");
        symtab[symCount].scope_level = 0;
        symtab[symCount].parent_scope = -1;
        symtab[symCount].size = offset;
        symtab[symCount].is_function = 0;
        symtab[symCount].is_external = 0;
        symtab[symCount].is_static = 0;
        strcpy(symtab[symCount].function_scope, "none");
        symCount++;
    }
    
    // Restore scope
    current_scope = saved_scope;
    scope_depth = saved_depth;
    
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
    
    int saved_scope = current_scope;
    int saved_depth = scope_depth;
    current_scope = 0;
    scope_depth = 0;
    
    // Insert union tag as a type symbol in global scope
    if (symCount < MAX_SYMBOLS) {
        strcpy(symtab[symCount].name, name);
        sprintf(symtab[symCount].type, "union %s", name);
        strcpy(symtab[symCount].kind, "union_tag");
        symtab[symCount].scope_level = 0;
        symtab[symCount].parent_scope = -1;
        symtab[symCount].size = maxSize;
        symtab[symCount].is_function = 0;
        symtab[symCount].is_external = 0;
        symtab[symCount].is_static = 0;
        strcpy(symtab[symCount].function_scope, "none");
        symCount++;
    }
    
    // Restore scope
    current_scope = saved_scope;
    scope_depth = saved_depth;
    
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
    const char* name = union_name;
    if (strncmp(union_name, "union ", 6) == 0) {
        name = union_name + 6;
    }
    
    StructDef* def = lookupUnion(name);
    if (def) {
        return def->total_size;
    }
    return 0;
}

int getTypeSize(const char* type) {
    if (strcmp(type, "char") == 0) return 1;
    if (strcmp(type, "short") == 0) return 2;
    if (strcmp(type, "int") == 0) return 4;
    if (strcmp(type, "long") == 0) return 8;
    if (strcmp(type, "float") == 0) return 4;
    if (strcmp(type, "double") == 0) return 8;
    if (strstr(type, "*")) return POINTER_SIZE;
    // Check if it's a struct type
    if (strncmp(type, "struct ", 7) == 0) {
        return getStructSize(type);
    }
    
    // Check if it's a union type
    if (strncmp(type, "union ", 6) == 0) {
        return getUnionSize(type);
    }
    
    const char* bracket = strchr(type, '[');
    if (bracket) {
        // Extract base type (everything before '[')
        char baseType[128];
        int baseLen = bracket - type;
        strncpy(baseType, type, baseLen);
        baseType[baseLen] = '\0';
        
        // Get base type size
        int baseSize = getTypeSize(baseType);
        
        // Calculate total size by multiplying all dimensions
        int totalSize = baseSize;
        const char* p = bracket;
        while (*p) {
            if (*p == '[') {
                p++;
                int dim = atoi(p);
                if (dim > 0) {
                    totalSize *= dim;
                }
                while (*p && *p != '[' && *p != ']') p++;
                if (*p == ']') p++;
            } else {
                p++;
            }
        }
        return totalSize;
    }
    
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, type) == 0 && strcmp(symtab[i].kind, "typedef") == 0) {
            return getTypeSize(symtab[i].type);
        }
    }
    
    return 4;
}

void insertVariable(const char* name, const char* type, int is_array, int* dims, int num_dims, int ptr_level, int is_static, int points_to_const, int is_const_ptr, int is_reference) {
    if (symCount >= MAX_SYMBOLS) {
        cerr << "Error: Symbol table overflow" << endl;
        return;
    }
    

    for (int i = symCount - 1; i >= 0; i--) {
        if (strcmp(symtab[i].name, name) == 0 && 
            symtab[i].scope_level == current_scope &&
            symtab[i].block_id == current_block_id &&
            strcmp(symtab[i].function_scope, current_function) == 0) {
            return;
        }
    }
    
    strcpy(symtab[symCount].name, name);
    
    char fullType[256];
    strcpy(fullType, type);
    
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
        char cleaned_type[256];
        int src_idx = 0, dst_idx = 0;
        while (type[src_idx] != '\0') {
            if (type[src_idx] == ' ' && (src_idx == 0 || type[src_idx-1] == '*' || type[src_idx+1] == '*')) {
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
    } else if (strcmp(type, "function_pointer") == 0) {
        char fp_type[256];
        formatFunctionPointerType(&symtab[symCount], fp_type, sizeof(fp_type));
        strcpy(symtab[symCount].type, fp_type);
        strcpy(symtab[symCount].kind, "function_pointer");
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
    symtab[symCount].block_id = (current_scope >= 2) ? current_block_id : 0;
    symtab[symCount].offset = current_offset;
    symtab[symCount].is_array = is_array;
    symtab[symCount].ptr_level = ptr_level;
    symtab[symCount].is_function = 0;
    symtab[symCount].is_external = 0;
    symtab[symCount].is_static = is_static;
    symtab[symCount].points_to_const = points_to_const;
    symtab[symCount].is_const_ptr = is_const_ptr;
    symtab[symCount].is_const = (points_to_const || is_const_ptr);
    symtab[symCount].is_reference = is_reference;
    
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
    } else if (is_reference) {
        // References are implemented as pointers (store an address)
        symtab[symCount].size = POINTER_SIZE;
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
    
    // Check for duplicate in current scope and current function
    for (int i = symCount - 1; i >= 0; i--) {
        if (strcmp(symtab[i].name, name) == 0 && 
            symtab[i].scope_level == current_scope &&
            strcmp(symtab[i].function_scope, current_function) == 0) {
            return; // Already exists in this function scope
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
    symtab[symCount].block_id = 0;  // Parameters are at function scope, no block ID
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
    symtab[symCount].block_id = 0;  // Functions don't have block IDs
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
    symtab[symCount].block_id = 0;  // Functions don't have block IDs
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
    
    // Check for duplicate label in the current function (labels have function scope in C)
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, name) == 0 && 
            strcmp(symtab[i].kind, "label") == 0 &&
            strcmp(symtab[i].function_scope, current_function) == 0) {
            // Duplicate label found in same function
            type_error(yylineno, "Duplicate label '%s'", name);
            return;  // Don't insert duplicate
        }
    }
    
    strcpy(symtab[symCount].name, name);
    strcpy(symtab[symCount].type, "-");  // Labels don't have a type
    strcpy(symtab[symCount].kind, "label");
    symtab[symCount].scope_level = current_scope;
    symtab[symCount].parent_scope = (scope_depth > 0) ? parent_scopes[scope_depth - 1] : -1;
    symtab[symCount].block_id = (current_scope >= 2) ? current_block_id : 0;
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

Symbol* lookupLabel(const char* name) {
    // Labels have function scope in C, so search within current function
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, name) == 0 && 
            strcmp(symtab[i].kind, "label") == 0 &&
            strcmp(symtab[i].function_scope, current_function) == 0) {
            return &symtab[i];
        }
    }
    return NULL;
}

Symbol* lookupSymbol(const char* name) {
    for (int i = symCount - 1; i >= 0; i--) {
        if (strcmp(symtab[i].name, name) == 0) {
            
            int check_scope = current_scope;
            int check_block = current_block_id;
            int depth_idx = scope_depth - 1; // Index into parent_scopes/parent_blocks arrays
            
            while (check_scope >= 0) {
                if (symtab[i].scope_level == check_scope) {
                    if (symtab[i].block_id == check_block || symtab[i].block_id == 0) {
                        return &symtab[i];
                    } else {
                        
                        break;
                    }
                }
                
                // Move to parent scope/block
                if (check_scope == 0) break;
                
                // Move up one level in the scope hierarchy
                if (depth_idx >= 0) {
                    check_scope = parent_scopes[depth_idx];
                    check_block = parent_blocks[depth_idx];
                    depth_idx--;
                } else {
                    check_scope = 0; // Go to global
                    check_block = 0;
                }
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
    // Push current scope and block onto parent stacks
    if (scope_depth < 100) {
        parent_scopes[scope_depth] = current_scope;
        parent_blocks[scope_depth] = current_block_id;
        scope_depth++;
    }
    // Increment scope level for nested blocks
    current_scope++;
    // Assign unique block ID for this nested block
    current_block_id = next_block_id++;
}

void exitScope() {
    if (scope_depth > 0) {
        scope_depth--;
        current_scope = parent_scopes[scope_depth];
        current_block_id = parent_blocks[scope_depth];
    } else {
        current_scope = 0;
        current_block_id = 0;
    }
}

void moveRecentSymbolsToCurrentScope(int count) {
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
    int marked = 0;
    Symbol* params[16];  // Store pointers to parameter symbols
    
    for (int i = symCount - 1; i >= 0 && marked < count; i--) {
        if (!symtab[i].is_function && symtab[i].scope_level == current_scope) {
            strcpy(symtab[i].kind, "parameter");
            params[marked] = &symtab[i];
            marked++;
        }
    }
    
    for (int i = symCount - 1; i >= 0; i--) {
        if (symtab[i].is_function && strcmp(symtab[i].name, current_function) == 0) {
            symtab[i].param_count = count;
            // Copy parameter types and names (in reverse order since we collected them backwards)
            for (int j = 0; j < count; j++) {
                strcpy(symtab[i].param_types[j], params[count - 1 - j]->type);
                strcpy(symtab[i].param_names[j], params[count - 1 - j]->name);
            }
            break;
        }
    }
}

void insertSymbol(const char* name, const char* type, int is_function, int is_static) {
    if (is_function) {
        insertFunction(name, type, 0, NULL, NULL, is_static);
    } else {
        insertVariable(name, type, 0, NULL, 0, 0, is_static, 0, 0, 0); // Last 0 is for is_reference
    }
}

void registerFunctionPointer(const char* name) {
    if (function_pointer_count < MAX_SYMBOLS) {
        strncpy(function_pointers[function_pointer_count], name, 127);
        function_pointers[function_pointer_count][127] = '\0';
        function_pointer_count++;
    }
}

int isFunctionPointerName(const char* name) {
    for (int i = 0; i < function_pointer_count; i++) {
        if (strcmp(function_pointers[i], name) == 0) {
            return 1;
        }
    }
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

int isArithmeticType(const char* type) {
    if (!type) return 0;
    
    // Resolve typedef first to get underlying type
    char* resolved = resolveTypedef(type);
    const char* actual_type = resolved ? resolved : type;
    
    int result = (strcmp(actual_type, "int") == 0 || strcmp(actual_type, "char") == 0 ||
            strcmp(actual_type, "short") == 0 || strcmp(actual_type, "long") == 0 ||
            strcmp(actual_type, "float") == 0 || strcmp(actual_type, "double") == 0);
    
    if (resolved) free(resolved);
    return result;
}

int isIntegerType(const char* type) {
    if (!type) return 0;
    
    // Resolve typedef first
    char* resolved = resolveTypedef(type);
    const char* actual_type = resolved ? resolved : type;
    
    // Check for basic integer types
    int result = (strcmp(actual_type, "int") == 0 || strcmp(actual_type, "char") == 0 ||
            strcmp(actual_type, "short") == 0 || strcmp(actual_type, "long") == 0 ||
            strcmp(actual_type, "enum") == 0 || strcmp(actual_type, "_Bool") == 0);
    
    if (resolved) free(resolved);
    return result;
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
    cout << "\n=== SYMBOL TABLE ===" << endl;
    cout << left << setw(20) << "Name" << setw(20) << "Type" << setw(20) << "Kind" << setw(5) << "Scope" << setw(20) << "Parent" << setw(4) << "Size" << endl;
    cout << string(89, '-') << endl;
    
    // Count user-defined symbols in global scope
    int global_user_symbols = 0;
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level == 0 && !symtab[i].is_external) {
            global_user_symbols++;
        }
    }
    
    // Print global scope (0) - only user-defined symbols
    if (global_user_symbols > 0) {
        cout << ">>> GLOBAL <<<" << endl;
        for (int i = 0; i < symCount; i++) {
            if (symtab[i].scope_level == 0 && !symtab[i].is_external) {
                const char* display_type;
                if (symtab[i].is_function) {
                    display_type = symtab[i].return_type;
                } else {
                    display_type = getDisplayType(&symtab[i]);
                }
                
                cout << left << setw(20) << symtab[i].name
                     << setw(20) << display_type
                     << setw(20) << symtab[i].kind
                     << setw(5) << symtab[i].scope_level
                     << setw(20) << "none";
                
                // Show "-" for labels, actual size for others
                if (strcmp(symtab[i].kind, "label") == 0) {
                    cout << setw(4) << "-" << endl;
                } else {
                    cout << setw(4) << symtab[i].size << endl;
                }
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
                    cout << ">>> " << func_name << " <<<" << endl;
                    
                    // Print all symbols in this function's scope
                    for (int j = 0; j < symCount; j++) {
                        if (symtab[j].scope_level == 1 && strcmp(symtab[j].function_scope, func_name) == 0) {
                            cout << left << setw(20) << symtab[j].name
                                 << setw(20) << getDisplayType(&symtab[j])
                                 << setw(20) << symtab[j].kind
                                 << setw(5) << symtab[j].scope_level
                                 << setw(20) << func_name;
                            
                            // Show "-" for labels, actual size for others
                            if (strcmp(symtab[j].kind, "label") == 0) {
                                cout << setw(4) << "-" << endl;
                            } else {
                                cout << setw(4) << symtab[j].size << endl;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Find max scope level first
    int max_scope = 0;
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level > max_scope) {
            max_scope = symtab[i].scope_level;
        }
    }
    
    // Print all nested block scopes (2 and above) dynamically
    for (int scope_level = 2; scope_level <= max_scope; scope_level++) {
        // Group by function name and block ID
        struct BlockInfo {
            char func_name[128];
            int block_id;
        };
        BlockInfo processed_blocks[100];
        int processed_count = 0;
        
        for (int i = 0; i < symCount; i++) {
            if (symtab[i].scope_level == scope_level && strlen(symtab[i].function_scope) > 0) {
                const char* func_name = symtab[i].function_scope;
                int block_id = symtab[i].block_id;
                
                // Check if we've already processed this function+block combination
                bool already_processed = false;
                for (int j = 0; j < processed_count; j++) {
                    if (strcmp(processed_blocks[j].func_name, func_name) == 0 && 
                        processed_blocks[j].block_id == block_id) {
                        already_processed = true;
                        break;
                    }
                }
                
                if (!already_processed) {
                    // Add to processed list
                    strcpy(processed_blocks[processed_count].func_name, func_name);
                    processed_blocks[processed_count].block_id = block_id;
                    processed_count++;
                    
                    // Print header for this scope level and block
                    cout << ">>> SCOPE LEVEL " << scope_level << " (" << func_name << " - block_" << block_id << ") <<<" << endl;
                    
                    // Print all symbols in this scope and block
                    for (int j = 0; j < symCount; j++) {
                        if (symtab[j].scope_level == scope_level && 
                            strcmp(symtab[j].function_scope, func_name) == 0 &&
                            symtab[j].block_id == block_id) {
                            
                            cout << left << setw(20) << symtab[j].name
                                 << setw(20) << getDisplayType(&symtab[j])
                                 << setw(20) << symtab[j].kind
                                 << setw(5) << symtab[j].scope_level
                                 << setw(20) << func_name;
                            
                            // Show "-" for labels, actual size for others
                            if (strcmp(symtab[j].kind, "label") == 0) {
                                cout << setw(4) << "-" << endl;
                            } else {
                                cout << setw(4) << symtab[j].size << endl;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Count user-defined symbols
    int user_symbol_count = 0;
    for (int i = 0; i < symCount; i++) {
        if (!symtab[i].is_external) {
            user_symbol_count++;
        }
    }
    
    cout << string(89, '-') << endl;
    cout << "Symbols: " << user_symbol_count << " | Max scope: " << max_scope << " | External: " << (symCount - user_symbol_count) << endl;
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
    
    // Apply array-to-pointer decay for binary operators (not for sizeof or &)
    char* ltype_decayed = decayArrayToPointer(ltype);
    char* rtype_decayed = decayArrayToPointer(rtype);
    
    // Arithmetic operators: +, -, *, /, %
    if (strcmp(op, "+") == 0) {
        // Case 1: both arithmetic
        if (isArithmeticType(ltype_decayed) && isArithmeticType(rtype_decayed)) {
            *result_type = strdup(usualArithConv(ltype_decayed, rtype_decayed));
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        // Case 2: pointer + integer (or array + integer after decay)
        if (strstr(ltype_decayed, "*") && isArithmeticType(rtype_decayed)) {
            *result_type = strdup(ltype_decayed);
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        // Case 3: integer + pointer (or integer + array after decay)
        if (isArithmeticType(ltype_decayed) && strstr(rtype_decayed, "*")) {
            *result_type = strdup(rtype_decayed);
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '+' (have '%s' and '%s')", ltype, rtype);
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "-") == 0) {
        // Case 1: both arithmetic
        if (isArithmeticType(ltype_decayed) && isArithmeticType(rtype_decayed)) {
            *result_type = strdup(usualArithConv(ltype_decayed, rtype_decayed));
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        // Case 2: pointer - integer (or array - integer after decay)
        if (strstr(ltype_decayed, "*") && isArithmeticType(rtype_decayed)) {
            *result_type = strdup(ltype_decayed);
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        // Case 3: pointer - pointer (same base type)
        if (strstr(ltype_decayed, "*") && strstr(rtype_decayed, "*")) {
            if (isPointerCompatible(ltype_decayed, rtype_decayed)) {
                *result_type = strdup("int"); // ptrdiff_t
                free(ltype_decayed);
                free(rtype_decayed);
                return TYPE_OK;
            }
            type_error(yylineno, "invalid operands to binary '-' (incompatible pointer types)");
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_ERROR;
        }
        type_error(yylineno, "invalid operands to binary '-' (have '%s' and '%s')", ltype, rtype);
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
        if (isArithmeticType(ltype_decayed) && isArithmeticType(rtype_decayed)) {
            *result_type = strdup(usualArithConv(ltype_decayed, rtype_decayed));
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "%") == 0) {
        // Modulo requires integer operands (not float/double)
        if (isIntegerType(ltype_decayed) && isIntegerType(rtype_decayed)) {
            *result_type = strdup("int");
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%%' (have '%s' and '%s')", ltype, rtype);
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_ERROR;
    }
    
    // Relational operators: <, >, <=, >=
    if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        // Both arithmetic OR both compatible pointers
        if ((isArithmeticType(ltype_decayed) && isArithmeticType(rtype_decayed)) ||
            (strstr(ltype_decayed, "*") && strstr(rtype_decayed, "*") && isPointerCompatible(ltype_decayed, rtype_decayed))) {
            *result_type = strdup("int");
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_ERROR;
    }
    
    // Equality operators: ==, !=
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
        // Both arithmetic, or both pointers, or pointer vs null
        if ((isArithmeticType(ltype_decayed) && isArithmeticType(rtype_decayed)) ||
            (strstr(ltype_decayed, "*") && strstr(rtype_decayed, "*")) ||
            (strstr(ltype_decayed, "*") && isNullPointer(right)) ||
            (isNullPointer(left) && strstr(rtype_decayed, "*"))) {
            *result_type = strdup("int");
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_ERROR;
    }
    
    // Bitwise operators: &, |, ^, <<, >>
    if (strcmp(op, "&") == 0 || strcmp(op, "|") == 0 || strcmp(op, "^") == 0) {
        // Bitwise operators require integer operands (not float/double)
        if (isIntegerType(ltype_decayed) && isIntegerType(rtype_decayed)) {
            *result_type = strdup("int");
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0) {
        // Shift operators require integer operands (not float/double)
        if (isIntegerType(ltype_decayed) && isIntegerType(rtype_decayed)) {
            *result_type = strdup(ltype_decayed); // Result type is left operand type
            free(ltype_decayed);
            free(rtype_decayed);
            return TYPE_OK;
        }
        type_error(yylineno, "invalid operands to binary '%s' (have '%s' and '%s')", op, ltype, rtype);
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_ERROR;
    }
    
    // Logical operators: &&, ||
    if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        // Any scalar type can be converted to boolean
        *result_type = strdup("int");
        free(ltype_decayed);
        free(rtype_decayed);
        return TYPE_OK;
    }
    
    free(ltype_decayed);
    free(rtype_decayed);
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
            // Check if trying to dereference a void pointer
            if (strncmp(optype, "void*", 5) == 0 || strncmp(optype, "void *", 6) == 0) {
                type_error(yylineno, "invalid use of void expression: cannot dereference void pointer");
                *result_type = strdup("void");
                return TYPE_ERROR;
            }
            
            // Remove one level of pointer indirection
            char* result_type_str = (char*)malloc(strlen(optype) + 1);
            strcpy(result_type_str, optype);
            
            // Find the last '*' and remove it
            char* last_star = strrchr(result_type_str, '*');
            if (last_star) {
                *last_star = '\0';
                // Remove trailing spaces
                while (last_star > result_type_str && *(last_star - 1) == ' ') {
                    *(--last_star) = '\0';
                }
            }
            
            // If result becomes empty or just spaces, default to int
            if (strlen(result_type_str) == 0 || strspn(result_type_str, " ") == strlen(result_type_str)) {
                free(result_type_str);
                *result_type = strdup("int");
            } else {
                *result_type = result_type_str;
            }
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
    
    char* ltype_resolved = resolveTypedef(lhs->dataType);
    char* rtype_resolved = resolveTypedef(rhs->dataType);
    const char* ltype = ltype_resolved ? ltype_resolved : lhs->dataType;
    const char* rtype_raw = rtype_resolved ? rtype_resolved : rhs->dataType;
    
    // Check for const violations
    // Case 1: Direct assignment to const pointer (e.g., int* const ptr = ...; ptr = &y;)
    if (lhs->type == NODE_IDENTIFIER) {
        Symbol* sym = lookupSymbol(lhs->value);
        if (sym && sym->is_const_ptr) {
            type_error(yylineno, "assignment of read-only variable '%s'", lhs->value);
            if (ltype_resolved) free(ltype_resolved);
            if (rtype_resolved) free(rtype_resolved);
            return TYPE_ERROR;
        }
    }
    
    // Case 2: Assignment through pointer to const (e.g., const int* ptr; *ptr = 5;)
    if (lhs->type == NODE_UNARY_EXPRESSION && strcmp(lhs->value, "*") == 0) {
        // This is a dereference - check if the pointer points to const data
        TreeNode* ptr_node = lhs->children[0];
        if (ptr_node->type == NODE_IDENTIFIER) {
            Symbol* sym = lookupSymbol(ptr_node->value);
            if (sym && sym->points_to_const) {
                type_error(yylineno, "assignment of read-only location '*%s'", ptr_node->value);
                if (ltype_resolved) free(ltype_resolved);
                if (rtype_resolved) free(rtype_resolved);
                return TYPE_ERROR;
            }
        }
    }
    
    if (isArrayType(ltype)) {
        type_error(yylineno, "cannot assign arrays");
        if (ltype_resolved) free(ltype_resolved);
        if (rtype_resolved) free(rtype_resolved);
        return TYPE_ERROR;
    }
    
    // Check if trying to assign array to scalar (before decay)
    if (isArrayType(rtype_raw) && !strstr(ltype, "*")) {
        char* base_type = getArrayBaseType(rtype_raw);
        if (base_type) {
            type_error(yylineno, "cannot convert array type '%s' to '%s'", rtype_raw, base_type);
        } else {
            type_error(yylineno, "cannot convert array type '%s' to '%s'", rtype_raw, ltype);
        }
        if (ltype_resolved) free(ltype_resolved);
        if (rtype_resolved) free(rtype_resolved);
        return TYPE_ERROR;
    }
    
    // Apply array-to-pointer decay on RHS (arrays decay when used as rvalues)
    char* rtype_decayed = decayArrayToPointer(rtype_raw);
    const char* rtype = rtype_decayed;
    
    // Same type
    if (strcmp(ltype, rtype) == 0) {
        free(rtype_decayed);
        if (ltype_resolved) free(ltype_resolved);
        if (rtype_resolved) free(rtype_resolved);
        return TYPE_OK;
    }
    
    // Arithmetic conversion
    if (isArithmeticType(ltype) && isArithmeticType(rtype)) {
        if (strcmp(ltype, "char") == 0 && strcmp(rtype, "int") == 0) {
            type_warning(yylineno, "conversion from 'int' to 'char' may alter value");
        }
        free(rtype_decayed);
        if (ltype_resolved) free(ltype_resolved);
        if (rtype_resolved) free(rtype_resolved);
        return TYPE_OK;
    }
    
    // Pointer assignments (including array-to-pointer decay)
    if (strstr(ltype, "*") && strstr(rtype, "*")) {
        if (isPointerCompatible(ltype, rtype)) {
            free(rtype_decayed);
            if (ltype_resolved) free(ltype_resolved);
            if (rtype_resolved) free(rtype_resolved);
            return TYPE_OK;
        }
        type_warning(yylineno, "assignment from incompatible pointer type");
        free(rtype_decayed);
        if (ltype_resolved) free(ltype_resolved);
        if (rtype_resolved) free(rtype_resolved);
        return TYPE_WARNING;
    }
    
    // Pointer = NULL
    if (strstr(ltype, "*") && isNullPointer(rhs)) {
        free(rtype_decayed);
        if (ltype_resolved) free(ltype_resolved);
        if (rtype_resolved) free(rtype_resolved);
        return TYPE_OK;
    }
    
    // Pointer = integer (error unless 0)
    if (strstr(ltype, "*") && isArithmeticType(rtype)) {
        type_error(yylineno, "assignment makes pointer from integer without a cast");
        free(rtype_decayed);
        if (ltype_resolved) free(ltype_resolved);
        if (rtype_resolved) free(rtype_resolved);
        return TYPE_ERROR;
    }
    
    // Integer = pointer (error)
    if (isArithmeticType(ltype) && strstr(rtype, "*")) {
        type_error(yylineno, "assignment makes integer from pointer without a cast");
        free(rtype_decayed);
        if (ltype_resolved) free(ltype_resolved);
        if (rtype_resolved) free(rtype_resolved);
        return TYPE_ERROR;
    }
    
    type_error(yylineno, "incompatible types when assigning to type '%s' from type '%s'", ltype, rtype);
    free(rtype_decayed);
    if (ltype_resolved) free(ltype_resolved);
    if (rtype_resolved) free(rtype_resolved);
    return TYPE_ERROR;
}

typedef struct {
    char specifier;     
    char length[8];     
    int position;       
} FormatSpec;

int parseFormatString(const char* format_str, FormatSpec* specs, int max_specs) {
    int count = 0;
    int len = strlen(format_str);
    
    // Remove surrounding quotes
    int start = (format_str[0] == '"') ? 1 : 0;
    int end = (format_str[len-1] == '"') ? len - 1 : len;
    
    for (int i = start; i < end && count < max_specs; i++) {
        if (format_str[i] == '%') {
            i++; // Move past %
            
            // Skip flags (-, +, space, #, 0)
            while (i < end && (format_str[i] == '-' || format_str[i] == '+' || 
                   format_str[i] == ' ' || format_str[i] == '#' || format_str[i] == '0')) {
                i++;
            }
            
            // Skip width (digits or *)
            while (i < end && (isdigit(format_str[i]) || format_str[i] == '*')) {
                i++;
            }
            
            // Skip precision (.digits or .*)
            if (i < end && format_str[i] == '.') {
                i++;
                while (i < end && (isdigit(format_str[i]) || format_str[i] == '*')) {
                    i++;
                }
            }
            
            // Parse length modifier (l, ll, h, hh, L, z, t, j)
            specs[count].length[0] = '\0';
            if (i < end) {
                if (format_str[i] == 'l') {
                    if (i+1 < end && format_str[i+1] == 'l') {
                        strcpy(specs[count].length, "ll");
                        i += 2;
                    } else {
                        strcpy(specs[count].length, "l");
                        i++;
                    }
                } else if (format_str[i] == 'h') {
                    if (i+1 < end && format_str[i+1] == 'h') {
                        strcpy(specs[count].length, "hh");
                        i += 2;
                    } else {
                        strcpy(specs[count].length, "h");
                        i++;
                    }
                } else if (format_str[i] == 'L' || format_str[i] == 'z' || 
                           format_str[i] == 't' || format_str[i] == 'j') {
                    specs[count].length[0] = format_str[i];
                    specs[count].length[1] = '\0';
                    i++;
                }
            }
            
            // Parse conversion specifier
            if (i < end) {
                char spec = format_str[i];
                if (spec == '%') {
                    // %% is escaped percent, not a specifier
                    continue;
                }
                specs[count].specifier = spec;
                specs[count].position = i;
                count++;
            }
        }
    }
    
    return count;
}

// Get expected type for printf format specifier
const char* getPrintfExpectedType(const FormatSpec* spec) {
    // Handle length + specifier combinations
    if (strcmp(spec->length, "ll") == 0) {
        if (spec->specifier == 'd' || spec->specifier == 'i') return "long long";
        if (spec->specifier == 'u' || spec->specifier == 'x' || 
            spec->specifier == 'X' || spec->specifier == 'o') return "unsigned long long";
    }
    if (strcmp(spec->length, "l") == 0) {
        if (spec->specifier == 'd' || spec->specifier == 'i') return "long";
        if (spec->specifier == 'u' || spec->specifier == 'x' || 
            spec->specifier == 'X' || spec->specifier == 'o') return "unsigned long";
        if (spec->specifier == 'f' || spec->specifier == 'e' || 
            spec->specifier == 'E' || spec->specifier == 'g' || 
            spec->specifier == 'G' || spec->specifier == 'a' || 
            spec->specifier == 'A') return "double";
        if (spec->specifier == 'c') return "wint_t";
        if (spec->specifier == 's') return "wchar_t*";
    }
    if (strcmp(spec->length, "h") == 0) {
        if (spec->specifier == 'd' || spec->specifier == 'i') return "short";
        if (spec->specifier == 'u' || spec->specifier == 'x' || 
            spec->specifier == 'X' || spec->specifier == 'o') return "unsigned short";
    }
    if (strcmp(spec->length, "hh") == 0) {
        if (spec->specifier == 'd' || spec->specifier == 'i') return "char";
        if (spec->specifier == 'u' || spec->specifier == 'x' || 
            spec->specifier == 'X' || spec->specifier == 'o') return "unsigned char";
    }
    if (strcmp(spec->length, "L") == 0) {
        if (spec->specifier == 'f' || spec->specifier == 'e' || 
            spec->specifier == 'E' || spec->specifier == 'g' || 
            spec->specifier == 'G' || spec->specifier == 'a' || 
            spec->specifier == 'A') return "long double";
    }
    
    // No length modifier - use standard types
    switch (spec->specifier) {
        case 'd': case 'i': return "int";
        case 'u': case 'x': case 'X': case 'o': return "unsigned int";
        case 'f': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': return "double";
        case 'c': return "char";
        case 's': return "char*";
        case 'p': return "void*";
        case 'n': return "int*";
        default: return "unknown";
    }
}

// Get expected type for scanf format specifier (returns pointer type)
const char* getScanfExpectedType(const FormatSpec* spec) {
    // Handle length + specifier combinations
    if (strcmp(spec->length, "ll") == 0) {
        if (spec->specifier == 'd' || spec->specifier == 'i') return "long long*";
        if (spec->specifier == 'u' || spec->specifier == 'x' || 
            spec->specifier == 'X' || spec->specifier == 'o') return "unsigned long long*";
    }
    if (strcmp(spec->length, "l") == 0) {
        if (spec->specifier == 'd' || spec->specifier == 'i') return "long*";
        if (spec->specifier == 'u' || spec->specifier == 'x' || 
            spec->specifier == 'X' || spec->specifier == 'o') return "unsigned long*";
        if (spec->specifier == 'f' || spec->specifier == 'e' || 
            spec->specifier == 'E' || spec->specifier == 'g' || 
            spec->specifier == 'G' || spec->specifier == 'a' || 
            spec->specifier == 'A') return "double*";
        if (spec->specifier == 's') return "wchar_t*";
    }
    if (strcmp(spec->length, "h") == 0) {
        if (spec->specifier == 'd' || spec->specifier == 'i') return "short*";
        if (spec->specifier == 'u' || spec->specifier == 'x' || 
            spec->specifier == 'X' || spec->specifier == 'o') return "unsigned short*";
    }
    if (strcmp(spec->length, "hh") == 0) {
        if (spec->specifier == 'd' || spec->specifier == 'i') return "char*";
        if (spec->specifier == 'u' || spec->specifier == 'x' || 
            spec->specifier == 'X' || spec->specifier == 'o') return "unsigned char*";
    }
    if (strcmp(spec->length, "L") == 0) {
        if (spec->specifier == 'f' || spec->specifier == 'e' || 
            spec->specifier == 'E' || spec->specifier == 'g' || 
            spec->specifier == 'G' || spec->specifier == 'a' || 
            spec->specifier == 'A') return "long double*";
    }
    
    // No length modifier - use standard pointer types
    switch (spec->specifier) {
        case 'd': case 'i': return "int*";
        case 'u': case 'x': case 'X': case 'o': return "unsigned int*";
        case 'f': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': return "float*";
        case 'c': return "char*";
        case 's': return "char*";
        case 'p': return "void**";
        case 'n': return "int*";
        default: return "unknown";
    }
}

// Check if argument type is compatible with printf format specifier
int isPrintfTypeCompatible(const char* arg_type, const FormatSpec* spec) {
    const char* expected = getPrintfExpectedType(spec);
    if (strcmp(expected, "unknown") == 0) return 0;
    
    char* resolved = resolveTypedef(arg_type);
    const char* actual_type = resolved ? resolved : arg_type;
    
    // Decay arrays to pointers
    char* decayed = decayArrayToPointer(actual_type);
    
    // For %s, accept char*, const char*, or char[]
    if (spec->specifier == 's') {
        int result = (strcmp(decayed, "char*") == 0 || 
                     strcmp(decayed, "const char*") == 0);
        free(decayed);
        if (resolved) free(resolved);
        return result;
    }
    
    // For %p, accept any pointer type
    if (spec->specifier == 'p') {
        int result = (strstr(decayed, "*") != NULL);
        free(decayed);
        if (resolved) free(resolved);
        return result;
    }
    
    // For integer specifiers, allow implicit conversions between integer types
    if (spec->specifier == 'd' || spec->specifier == 'i' || 
        spec->specifier == 'u' || spec->specifier == 'x' || 
        spec->specifier == 'X' || spec->specifier == 'o') {
        int result = isIntegerType(decayed);
        free(decayed);
        if (resolved) free(resolved);
        return result;
    }
    
    // For floating point specifiers, allow float/double conversions
    if (spec->specifier == 'f' || spec->specifier == 'e' || 
        spec->specifier == 'E' || spec->specifier == 'g' || 
        spec->specifier == 'G' || spec->specifier == 'a' || 
        spec->specifier == 'A') {
        int result = (strcmp(decayed, "float") == 0 || 
                     strcmp(decayed, "double") == 0 ||
                     strcmp(decayed, "long double") == 0);
        free(decayed);
        if (resolved) free(resolved);
        return result;
    }
    
    // For %c, accept char or int
    if (spec->specifier == 'c') {
        int result = (strcmp(decayed, "char") == 0 || 
                     strcmp(decayed, "int") == 0);
        free(decayed);
        if (resolved) free(resolved);
        return result;
    }
    
    // Exact match required for others
    int result = (strcmp(decayed, expected) == 0);
    free(decayed);
    if (resolved) free(resolved);
    return result;
}

// Check if argument type is compatible with scanf format specifier
int isScanfTypeCompatible(const char* arg_type, const FormatSpec* spec) {
    const char* expected = getScanfExpectedType(spec);
    if (strcmp(expected, "unknown") == 0) return 0;
    
    // CRITICAL: Resolve typedef first
    char* resolved = resolveTypedef(arg_type);
    const char* actual_type = resolved ? resolved : arg_type;
    
    // Decay arrays to pointers
    char* decayed = decayArrayToPointer(actual_type);
    
    // For %s, accept char* or char[]
    if (spec->specifier == 's') {
        int result = (strcmp(decayed, "char*") == 0);
        free(decayed);
        if (resolved) free(resolved);
        return result;
    }
    

    char normalized[128];
    strcpy(normalized, decayed);
    char* const_pos = strstr(normalized, "const ");
    if (const_pos) {
        memmove(const_pos, const_pos + 6, strlen(const_pos + 6) + 1);
    }
    
    // Allow int* for %d even if expecting unsigned int* (common practice)
    if (spec->specifier == 'u' || spec->specifier == 'x' || 
        spec->specifier == 'X' || spec->specifier == 'o') {
        if (strcmp(normalized, "int*") == 0 || strcmp(normalized, "unsigned int*") == 0) {
            free(decayed);
            if (resolved) free(resolved);
            return 1;
        }
    }
    
    int result = (strcmp(normalized, expected) == 0);
    free(decayed);
    if (resolved) free(resolved);
    return result;
}

// Validate printf call
TypeCheckResult validatePrintf(TreeNode* args) {
    if (!args || args->childCount == 0) {
        type_error(yylineno, "too few arguments to function 'printf'");
        return TYPE_ERROR;
    }
    
    // First argument must be a string literal (format string)
    TreeNode* format_arg = args->children[0];
    if (format_arg->type != NODE_STRING_LITERAL) {
        type_warning(yylineno, "format not a string literal (potentially insecure)");
        return TYPE_WARNING;
    }
    
    // Parse format string
    FormatSpec specs[32];
    int spec_count = parseFormatString(format_arg->value, specs, 32);
    
    // Check argument count
    int provided_args = args->childCount - 1; // Exclude format string
    if (provided_args < spec_count) {
        type_error(yylineno, "too few arguments for format (expected %d, got %d)", 
                  spec_count, provided_args);
        return TYPE_ERROR;
    }
    if (provided_args > spec_count) {
        type_error(yylineno, "too many arguments for format (expected %d, got %d)", 
                  spec_count, provided_args);
        return TYPE_ERROR;
    }
    
    // Check each argument type
    TypeCheckResult result = TYPE_OK;
    for (int i = 0; i < spec_count; i++) {
        TreeNode* arg = args->children[i + 1];
        if (!arg->dataType) continue;
        
        if (!isPrintfTypeCompatible(arg->dataType, &specs[i])) {
            const char* expected = getPrintfExpectedType(&specs[i]);
            char* arg_decayed = decayArrayToPointer(arg->dataType);
            type_error(yylineno, "format '%%%c' expects argument of type '%s', but argument %d has type '%s'",
                      specs[i].specifier, expected, i + 2, arg_decayed);
            free(arg_decayed);
            result = TYPE_ERROR;
        }
    }
    
    return result;
}

// Validate scanf call
TypeCheckResult validateScanf(TreeNode* args) {
    if (!args || args->childCount == 0) {
        type_error(yylineno, "too few arguments to function 'scanf'");
        return TYPE_ERROR;
    }
    
    // First argument must be a string literal (format string)
    TreeNode* format_arg = args->children[0];
    if (format_arg->type != NODE_STRING_LITERAL) {
        type_warning(yylineno, "format not a string literal (potentially insecure)");
        return TYPE_WARNING;
    }
    
    // Parse format string
    FormatSpec specs[32];
    int spec_count = parseFormatString(format_arg->value, specs, 32);
    
    // Check argument count
    int provided_args = args->childCount - 1; // Exclude format string
    if (provided_args < spec_count) {
        type_error(yylineno, "too few arguments for format (expected %d, got %d)", 
                  spec_count, provided_args);
        return TYPE_ERROR;
    }
    if (provided_args > spec_count) {
        type_error(yylineno, "too many arguments for format (expected %d, got %d)", 
                  spec_count, provided_args);
        return TYPE_ERROR;
    }
    
    // Check each argument type - must be pointers
    TypeCheckResult result = TYPE_OK;
    for (int i = 0; i < spec_count; i++) {
        TreeNode* arg = args->children[i + 1];
        if (!arg->dataType) continue;
        
        // Special case: %s with char[] is acceptable (array decays to pointer)
        char* arg_decayed = decayArrayToPointer(arg->dataType);
        
        // Check if it's a pointer type
        if (!strstr(arg_decayed, "*") && specs[i].specifier != 's') {
            type_error(yylineno, "format '%%%c' expects argument of type '%s', but argument %d has type '%s' (missing '&'?)",
                      specs[i].specifier, getScanfExpectedType(&specs[i]), i + 2, arg_decayed);
            free(arg_decayed);
            result = TYPE_ERROR;
            continue;
        }
        
        // Check pointer type compatibility
        if (!isScanfTypeCompatible(arg_decayed, &specs[i])) {
            const char* expected = getScanfExpectedType(&specs[i]);
            type_error(yylineno, "format '%%%c' expects argument of type '%s', but argument %d has type '%s'",
                      specs[i].specifier, expected, i + 2, arg_decayed);
            free(arg_decayed);
            result = TYPE_ERROR;
        } else {
            free(arg_decayed);
        }
    }
    
    return result;
}

// Function call validation
TypeCheckResult checkFunctionCall(const char* func_name, TreeNode* args, char** result_type) {
    Symbol* func_sym = lookupSymbol(func_name);
    
    if (!func_sym) {
        type_error(yylineno, "implicit declaration of function '%s'", func_name);
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    if (!func_sym->is_function && strcmp(func_sym->kind, "function_pointer") != 0) {
        type_error(yylineno, "called object '%s' is not a function or function pointer", func_name);
        return TYPE_ERROR;
    }
    
    if (strcmp(func_sym->kind, "function_pointer") == 0) {
        *result_type = strdup("int");
    } else {
        *result_type = strdup(func_sym->return_type);
    }
    
    if (func_sym->is_external) {
        // Validate printf family functions
        if (strcmp(func_name, "printf") == 0 || strcmp(func_name, "fprintf") == 0 || 
            strcmp(func_name, "sprintf") == 0 || strcmp(func_name, "snprintf") == 0) {
            // For fprintf/sprintf/snprintf, skip first arg (FILE*/buffer/buffer+size)
            TreeNode* printf_args = args;
            if (strcmp(func_name, "fprintf") == 0 || strcmp(func_name, "sprintf") == 0) {
                // Create a temporary args node starting from second argument
                if (args && args->childCount > 1) {
                    printf_args = createNode(NODE_EXPRESSION, "temp_args");
                    for (int i = 1; i < args->childCount; i++) {
                        addChild(printf_args, args->children[i]);
                    }
                }
            } else if (strcmp(func_name, "snprintf") == 0) {
                // Skip first two arguments (buffer and size)
                if (args && args->childCount > 2) {
                    printf_args = createNode(NODE_EXPRESSION, "temp_args");
                    for (int i = 2; i < args->childCount; i++) {
                        addChild(printf_args, args->children[i]);
                    }
                }
            }
            
            TypeCheckResult printf_result = validatePrintf(printf_args);
            
            // Clean up temporary node if created
            if (printf_args != args && printf_args) {
                // Don't free children, just the container
                free(printf_args);
            }
            
            if (printf_result == TYPE_ERROR) {
                return TYPE_ERROR;
            }
        }
        
        // Validate scanf family functions
        if (strcmp(func_name, "scanf") == 0 || strcmp(func_name, "fscanf") == 0 || 
            strcmp(func_name, "sscanf") == 0) {
            // For fscanf/sscanf, skip first arg (FILE*/string)
            TreeNode* scanf_args = args;
            if (strcmp(func_name, "fscanf") == 0 || strcmp(func_name, "sscanf") == 0) {
                // Create a temporary args node starting from second argument
                if (args && args->childCount > 1) {
                    scanf_args = createNode(NODE_EXPRESSION, "temp_args");
                    for (int i = 1; i < args->childCount; i++) {
                        addChild(scanf_args, args->children[i]);
                    }
                }
            }
            
            TypeCheckResult scanf_result = validateScanf(scanf_args);
            
            // Clean up temporary node if created
            if (scanf_args != args && scanf_args) {
                // Don't free children, just the container
                free(scanf_args);
            }
            
            if (scanf_result == TYPE_ERROR) {
                return TYPE_ERROR;
            }
        }
    }
    
    // Strict argument checking for specific stdlib functions
    if (func_sym->is_external) {
        int provided_args = args ? args->childCount : 0;
        
        // Check atoi, atol, atof - expect single char* argument
        if (strcmp(func_name, "atoi") == 0 || strcmp(func_name, "atol") == 0 || 
            strcmp(func_name, "atof") == 0) {
            if (provided_args != 1) {
                type_error(yylineno, "incorrect number of arguments to '%s' (expected 1, got %d)", 
                          func_name, provided_args);
                return TYPE_ERROR;
            }
            if (args && args->children[0]->dataType) {
                const char* arg_type = args->children[0]->dataType;
                char* arg_decayed = decayArrayToPointer(arg_type);
                
                // Must be char* or const char* (pointer to char)
                if (strcmp(arg_decayed, "char*") != 0 && strcmp(arg_decayed, "const char*") != 0) {
                    type_error(yylineno, "incorrect argument type to '%s' (expected 'char*', have '%s')", 
                              func_name, arg_decayed);
                    free(arg_decayed);
                    return TYPE_ERROR;
                }
                free(arg_decayed);
            }
        }
        // Check abs - expects single int argument
        else if (strcmp(func_name, "abs") == 0) {
            if (provided_args != 1) {
                type_error(yylineno, "incorrect number of arguments to 'abs' (expected 1, got %d)", 
                          provided_args);
                return TYPE_ERROR;
            }
            if (args && args->children[0]->dataType) {
                const char* arg_type = args->children[0]->dataType;
                char* arg_decayed = decayArrayToPointer(arg_type);
                
                // Must be int (not float, double, or other types)
                if (strcmp(arg_decayed, "int") != 0) {
                    type_error(yylineno, "incorrect argument type to 'abs' (expected 'int', have '%s')", 
                              arg_decayed);
                    free(arg_decayed);
                    return TYPE_ERROR;
                }
                free(arg_decayed);
            }
        }
    }
    
    // Check argument count for non-external functions (but not function pointers)
    if (!func_sym->is_external && strcmp(func_sym->kind, "function_pointer") != 0) {
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
                    // Apply array-to-pointer decay on the argument before type checking
                    char* arg_type_decayed = decayArrayToPointer(args->children[i]->dataType);
                    
                    if (!canImplicitConvert(arg_type_decayed, func_sym->param_types[i])) {
                        type_error(yylineno, "incompatible type for argument %d of '%s'", i+1, func_name);
                        free(arg_type_decayed);
                        return TYPE_ERROR;
                    }
                    free(arg_type_decayed);
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
    
    // Apply array-to-pointer decay on index (for syntax like arr[arr2])
    char* index_decayed = decayArrayToPointer(index->dataType);
    
    // Index must be integer type (not float or double)
    if (!isIntegerType(index_decayed)) {
        type_error(yylineno, "array subscript has non-integer type '%s'", index_decayed);
        free(index_decayed);
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    free(index_decayed);
    
    // Array must be array or pointer type (arrays automatically decay to pointers)
    const char* array_type = array->dataType;
    char* array_decayed = decayArrayToPointer(array_type);
    
    if (strstr(array_decayed, "*")) {
        // Extract element type by removing one level of pointer/indirection
        // For "char**" -> "char*", for "int*" -> "int", for "char***" -> "char**"
        
        // Find the LAST asterisk to remove only one level
        const char* last_star = strrchr(array_decayed, '*');
        if (last_star) {
            size_t len = last_star - array_decayed;
            char elem_type[128];
            strncpy(elem_type, array_decayed, len);
            elem_type[len] = '\0';
            
            // Trim trailing spaces
            int end = len - 1;
            while (end >= 0 && elem_type[end] == ' ') {
                elem_type[end] = '\0';
                end--;
            }
            
            *result_type = strdup(elem_type);
        } else {
            *result_type = strdup("int");
        }
        free(array_decayed);
        return TYPE_OK;
    }
    
    type_error(yylineno, "subscripted value is not an array or pointer");
    free(array_decayed);
    return TYPE_ERROR;
}

// Member access validation (struct.member or struct->member)
TypeCheckResult checkMemberAccess(TreeNode* struct_expr, const char* member, const char* op, char** result_type) {
    if (!struct_expr || !struct_expr->dataType || !member || !op) {
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    // CRITICAL: Resolve typedef to base type
    char* resolved_type = resolveTypedef(struct_expr->dataType);
    const char* struct_type = resolved_type ? resolved_type : struct_expr->dataType;
    
    int is_pointer = (strstr(struct_type, "*") != NULL);
    
    // ERROR 4: Check operator validity
    if (strcmp(op, ".") == 0 && is_pointer) {
        // Using DOT on a pointer - should use ARROW
        type_error(yylineno, "member reference type '%s' is a pointer; did you mean to use '->'?", struct_type);
        if (resolved_type) free(resolved_type);
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    if (strcmp(op, "->") == 0 && !is_pointer) {
        // Using ARROW on a non-pointer - should use DOT
        type_error(yylineno, "member reference type '%s' is not a pointer; did you mean to use '.'?", struct_type);
        if (resolved_type) free(resolved_type);
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    // Handle pointer to struct (arrow operator case)
    // If type is "struct Point*", extract "struct Point"
    char base_type[128];
    if (is_pointer) {
        // Remove the pointer part
        const char* star = strchr(struct_type, '*');
        size_t len = star - struct_type;
        strncpy(base_type, struct_type, len);
        base_type[len] = '\0';
        
        // Trim trailing spaces
        int end = len - 1;
        while (end >= 0 && base_type[end] == ' ') {
            base_type[end] = '\0';
            end--;
        }
    } else {
        strcpy(base_type, struct_type);
    }
    
    // Extract struct/union name from "struct StructName" or "union UnionName"
    char tag_name[128];
    int is_union = 0;
    
    if (strncmp(base_type, "struct ", 7) == 0) {
        strcpy(tag_name, base_type + 7);
        is_union = 0;
    } else if (strncmp(base_type, "union ", 6) == 0) {
        strcpy(tag_name, base_type + 6);
        is_union = 1;
    } else {
        // Not a struct or union type
        type_error(yylineno, "request for member '%s' in something not a structure or union", member);
        if (resolved_type) free(resolved_type);
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    // Look up the struct or union definition
    StructDef* def = is_union ? lookupUnion(tag_name) : lookupStruct(tag_name);
    if (!def) {
        type_error(yylineno, "invalid use of undefined %s '%s'", 
                  is_union ? "union" : "struct", tag_name);
        if (resolved_type) free(resolved_type);
        *result_type = strdup("int");
        return TYPE_ERROR;
    }
    
    // Find the member in the struct/union
    for (int i = 0; i < def->member_count; i++) {
        if (strcmp(def->members[i].name, member) == 0) {
            // Found the member - resolve typedef in member type if needed
            char* member_type_resolved = resolveTypedef(def->members[i].type);
            *result_type = member_type_resolved ? member_type_resolved : strdup(def->members[i].type);
            if (resolved_type) free(resolved_type);
            return TYPE_OK;
        }
    }
    
    // Member not found
    type_error(yylineno, "'%s %s' has no member named '%s'", 
              is_union ? "union" : "struct", tag_name, member);
    if (resolved_type) free(resolved_type);
    *result_type = strdup("int");
    return TYPE_ERROR;
}

// Helper functions

// Check if a type is an array type (contains '[')
int isArrayType(const char* type) {
    return (type && strstr(type, "[") != NULL);
}

// Extract base type from array type (e.g., "int[10]" -> "int")
char* getArrayBaseType(const char* arrayType) {
    if (!arrayType) return NULL;
    
    static char baseType[128];
    const char* bracket = strchr(arrayType, '[');
    if (bracket) {
        size_t len = bracket - arrayType;
        if (len < sizeof(baseType)) {
            strncpy(baseType, arrayType, len);
            baseType[len] = '\0';
            return baseType;
        }
    }
    return strdup(arrayType);
}

// Array-to-pointer decay: converts T[N] to T* in expression contexts
// Returns the decayed type (caller must free) or original type if no decay
char* decayArrayToPointer(const char* type) {
    if (!type) return NULL;
    
    if (isArrayType(type)) {
        // Extract base type and make it a pointer
        char* baseType = getArrayBaseType(type);
        char* ptrType = (char*)malloc(strlen(baseType) + 2);
        sprintf(ptrType, "%s*", baseType);
        return ptrType;
    }
    
    // Not an array, return copy of original type
    return strdup(type);
}

int isPointerCompatible(const char* ptr1, const char* ptr2) {
    // Simplified: void* is compatible with any pointer
    if (strstr(ptr1, "void*") || strstr(ptr2, "void*")) {
        return 1;
    }
    // Otherwise, base types must match
    return strcmp(ptr1, ptr2) == 0;
}

int isNullPointer(TreeNode* expr) {
    // Check for integer constant 0 (NULL pointer constant)
    if (expr->type == NODE_CONSTANT && strcmp(expr->value, "0") == 0) {
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
    error_count++;  // Increment error count to prevent IR generation
}

void type_warning(int line, const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    fprintf(stderr, "Type Warning on line %d: ", line);
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// Check if a type is a scalar type (can be used in conditionals)
// Scalar types: arithmetic types (int, float, char, etc.) and pointer types
int isScalarType(const char* type) {
    if (!type) return 0;
    
    // CRITICAL: Resolve typedef first
    char* resolved = resolveTypedef(type);
    const char* actual_type = resolved ? resolved : type;
    
    // Pointer types are scalar
    if (strstr(actual_type, "*") != NULL) {
        if (resolved) free(resolved);
        return 1;
    }
    
    // Arithmetic types are scalar
    if (isArithmeticType(actual_type)) {
        if (resolved) free(resolved);
        return 1;
    }
    
    // Array types decay to pointers, so they're effectively scalar
    if (isArrayType(actual_type)) {
        if (resolved) free(resolved);
        return 1;
    }
    
    // Struct and union types are NOT scalar
    if (strncmp(actual_type, "struct ", 7) == 0 || strncmp(actual_type, "union ", 6) == 0) {
        // Check if it's a pointer to struct/union (already handled above)
        // If we reach here, it's a direct struct/union, which is not scalar
        if (resolved) free(resolved);
        return 0;
    }
    
    if (resolved) free(resolved);
    return 0;  // Unknown types are not scalar
}

// ERROR 5: Validate conditional expression (must be scalar type)
TypeCheckResult validateConditional(TreeNode* expr) {
    if (!expr || !expr->dataType) {
        // No type info, assume OK (error already reported elsewhere)
        return TYPE_OK;
    }
    
    // Warn if array is used directly in conditional (even though it decays to pointer)
    if (isArrayType(expr->dataType)) {
        type_warning(yylineno, "array '%s' used in conditional context will always evaluate to true", 
                     expr->dataType);
    }
    
    // Apply array-to-pointer decay
    char* decayed_type = decayArrayToPointer(expr->dataType);
    
    if (!isScalarType(decayed_type)) {
        type_error(yylineno, "conditional expression has non-scalar type '%s'", decayed_type);
        free(decayed_type);
        return TYPE_ERROR;
    }
    
    free(decayed_type);
    return TYPE_OK;
}

// Recursively resolve typedef aliases to their base type
// If the type is a typedef, follow the chain until reaching a non-typedef base type
char* resolveTypedef(const char* type) {
    if (!type) return NULL;
    
    // Protect against infinite recursion with a depth limit
    int max_depth = 50;
    int depth = 0;
    
    char* current_type = strdup(type);
    char* resolved_type = NULL;
    
    while (depth < max_depth) {
        // Search for typedef with this name in symbol table
        Symbol* typedef_sym = NULL;
        
        // Search from most recent to oldest (to respect scope)
        for (int i = symCount - 1; i >= 0; i--) {
            if (strcmp(symtab[i].kind, "typedef") == 0 && 
                strcmp(symtab[i].name, current_type) == 0) {
                typedef_sym = &symtab[i];
                break;
            }
        }
        
        if (!typedef_sym) {
            // Not a typedef - this is the base type
            return current_type;
        }
        
        // Found a typedef - get its underlying type
        resolved_type = strdup(typedef_sym->type);
        free(current_type);
        current_type = resolved_type;
        depth++;
    }
    
    // Max depth reached - possible circular typedef (shouldn't happen in valid code)
    // Return what we have
    return current_type;
}

// Helper function to check if declaration_specifiers contains a specific storage class
int hasStorageClass(TreeNode* decl_specs, const char* storage_class) {
    if (!decl_specs || !storage_class) return 0;
    
    // Traverse children of declaration_specifiers node
    for (int i = 0; i < decl_specs->childCount; i++) {
        TreeNode* child = decl_specs->children[i];
        if (child->type == NODE_STORAGE_CLASS_SPECIFIER && 
            child->value && strcmp(child->value, storage_class) == 0) {
            return 1;
        }
    }
    return 0;
}


int isConstantExpression(TreeNode* expr) {
    if (!expr) return 1;  
    
    if (expr->type == NODE_CONSTANT ||
        expr->type == NODE_INTEGER_CONSTANT || 
        expr->type == NODE_FLOAT_CONSTANT ||
        expr->type == NODE_CHAR_CONSTANT ||
        expr->type == NODE_HEX_CONSTANT ||
        expr->type == NODE_OCTAL_CONSTANT ||
        expr->type == NODE_BINARY_CONSTANT) {
        return 1;
    }
    
    if (expr->type == NODE_IDENTIFIER) {
        Symbol* sym = lookupSymbol(expr->value);
        if (sym && strcmp(sym->kind, "enum_constant") == 0) {
            return 1; 
        }
        return 0;  
    }
    
    if (expr->childCount > 0) {
        for (int i = 0; i < expr->childCount; i++) {
            if (!isConstantExpression(expr->children[i])) {
                return 0;
            }
        }
        return 1;
    }
    

    if (expr->type == NODE_STRING_LITERAL) {
        return 1;
    }
    
    return 0;
}
