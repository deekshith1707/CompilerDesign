/**
 * MIPS Assembly Code Generator - Implementation
 * Phase 1: Runtime Environment & Stack Frame Management
 * Following Lectures 32-33: Activation Records & Calling Conventions
 */

#include "mips_codegen.h"
#include "ir_context.h"
#include "symbol_table.h"
#include "basic_block.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// External symbol table reference
extern Symbol symtab[MAX_SYMBOLS];
extern int symCount;

// Global activation records for all functions
static ActivationRecord activationRecords[MAX_FUNCTIONS];
static int activationRecordCount = 0;

// ============================================================================
// Task 1.1: Helper Functions & Initialization
// ============================================================================

/**
 * Check if a variable name is a temporary (starts with 't' followed by digit)
 */
bool isTemporary(const char* varName) {
    if (varName == NULL || strlen(varName) == 0) {
        return false;
    }
    
    // Temporary variables: t0, t1, t2, ... t99
    if (varName[0] == 't' && isdigit(varName[1])) {
        return true;
    }
    
    return false;
}

/**
 * Check if a string is a constant (number or literal)
 */
bool isConstantValue(const char* str) {
    if (str == NULL || strlen(str) == 0) {
        return true;
    }
    
    // Check for numbers (including negative)
    if (str[0] == '-' || isdigit(str[0])) {
        return true;
    }
    
    // Check for character literals ('A')
    if (str[0] == '\'') {
        return true;
    }
    
    // Check for string literals ("hello")
    if (str[0] == '"') {
        return true;
    }
    
    // Check for float constants (contains '.' AND is numeric)
    // CRITICAL FIX: Must verify string is numeric to avoid matching variable names like "static_function.local_static"
    if (strchr(str, '.') != NULL) {
        // Contains '.', check if it's a numeric float constant
        if (str[0] == '-' || str[0] == '+' || isdigit(str[0])) {
            // Starts with digit or sign, check if all chars are numeric/dot/scientific notation
            bool isNumeric = true;
            for (int i = 0; str[i] != '\0'; i++) {
                if (!isdigit(str[i]) && str[i] != '.' && str[i] != '-' && str[i] != '+' && 
                    str[i] != 'e' && str[i] != 'E') {
                    isNumeric = false;
                    break;
                }
            }
            if (isNumeric) {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * Check if a constant is a float (contains a decimal point and is numeric)
 */
bool isFloatConstant(const char* str) {
    if (str == NULL || strlen(str) == 0) {
        return false;
    }
    
    // Must start with digit or minus sign for negative numbers
    if (!isdigit(str[0]) && str[0] != '-' && str[0] != '+') {
        return false;
    }
    
    // Check for decimal point and ensure it's a numeric constant
    bool hasDot = false;
    int len = strlen(str);
    
    // Check if ends with 'f' or 'F' suffix (float literal)
    bool hasSuffix = false;
    if (len > 0 && (str[len-1] == 'f' || str[len-1] == 'F')) {
        hasSuffix = true;
        len--; // Don't check the suffix in the loop
    }
    
    for (int i = 0; i < len; i++) {
        if (str[i] == '.') {
            hasDot = true;
        } else if (!isdigit(str[i]) && str[i] != '-' && str[i] != '+' && str[i] != 'e' && str[i] != 'E') {
            // Contains non-numeric character (not part of float notation)
            return false;
        }
    }
    
    return hasDot || hasSuffix;
}

/**
 * Register a variable's type
 */
void registerVariableType(MIPSCodeGenerator* codegen, const char* varName, const char* varType) {
    if (!varName || !varType || codegen->varTypeCount >= MAX_VARIABLES) return;
    
    // Check if already registered - update if found
    for (int i = 0; i < codegen->varTypeCount; i++) {
        if (strcmp(codegen->varTypes[i].varName, varName) == 0) {
            strcpy(codegen->varTypes[i].varType, varType);
            return;
        }
    }
    
    // Add new entry
    strcpy(codegen->varTypes[codegen->varTypeCount].varName, varName);
    strcpy(codegen->varTypes[codegen->varTypeCount].varType, varType);
    codegen->varTypeCount++;
}

/**
 * Get a variable's type
 */
const char* getVariableType(MIPSCodeGenerator* codegen, const char* varName) {
    if (!varName) return "int";  // Default to int
    
    for (int i = 0; i < codegen->varTypeCount; i++) {
        if (strcmp(codegen->varTypes[i].varName, varName) == 0) {
            return codegen->varTypes[i].varType;
        }
    }
    
    return "int";  // Default to int if not found
}

/**
 * Check if instruction is a function begin
 */
bool isFunctionBegin(const Quadruple* quad) {
    return (strcmp(quad->op, "FUNC_BEGIN") == 0 || 
            strcmp(quad->op, "func_begin") == 0);
}

/**
 * Check if instruction is a function end
 */
bool isFunctionEnd(const Quadruple* quad) {
    return (strcmp(quad->op, "FUNC_END") == 0 || 
            strcmp(quad->op, "func_end") == 0);
}

/**
 * Find a function in IR by name, returns start index
 */
int findFunctionInIR(const char* funcName) {
    for (int i = 0; i < irCount; i++) {
        if (isFunctionBegin(&IR[i]) && strcmp(IR[i].arg1, funcName) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Find function end index given start index
 */
int findFunctionEnd(int funcStart) {
    for (int i = funcStart + 1; i < irCount; i++) {
        if (isFunctionEnd(&IR[i])) {
            return i;
        }
    }
    return irCount - 1;
}

/**
 * Get parameter count from symbol table
 */
int getParameterCount(const char* funcName) {
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, funcName) == 0 && symtab[i].is_function) {
            return symtab[i].param_count;
        }
    }
    return 0;
}

/**
 * Get parameter names from symbol table
 */
void getParameterNames(const char* funcName, char params[][128], int* count) {
    *count = 0;
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, funcName) == 0 && symtab[i].is_function) {
            for (int p = 0; p < symtab[i].param_count; p++) {
                strcpy(params[p], symtab[i].param_names[p]);
            }
            *count = symtab[i].param_count;
            return;
        }
    }
}

// ============================================================================
// Task 1.2: Frame Size Calculation
// ============================================================================

/**
 * Count local variables in a function from symbol table
 */
int countLocalsInFunction(const char* funcName) {
    int count = 0;
    
    for (int i = 0; i < symCount; i++) {
        // Check if variable belongs to this function's scope
        if (strcmp(symtab[i].function_scope, funcName) == 0 && 
            !symtab[i].is_function && 
            strcmp(symtab[i].kind, "variable") == 0) {
            count++;
        }
    }
    
    return count;
}

/**
 * Count temporary variables used in a function from IR
 */
int countTemporariesInFunction(int funcStart, int funcEnd) {
    char temps[MAX_VARIABLES][128];
    int tempCount = 0;
    
    // Scan IR instructions in this function
    for (int i = funcStart + 1; i < funcEnd; i++) {
        Quadruple* quad = &IR[i];
        
        // Check result field for temporaries
        if (strlen(quad->result) > 0 && isTemporary(quad->result)) {
            // Check if we've seen this temporary before
            bool found = false;
            for (int t = 0; t < tempCount; t++) {
                if (strcmp(temps[t], quad->result) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found && tempCount < MAX_VARIABLES) {
                strcpy(temps[tempCount++], quad->result);
            }
        }
        
        // Check arg1 field for temporaries (in case of assignment)
        if (strlen(quad->arg1) > 0 && isTemporary(quad->arg1)) {
            bool found = false;
            for (int t = 0; t < tempCount; t++) {
                if (strcmp(temps[t], quad->arg1) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found && tempCount < MAX_VARIABLES) {
                strcpy(temps[tempCount++], quad->arg1);
            }
        }
        
        // Check arg2 field for temporaries
        if (strlen(quad->arg2) > 0 && isTemporary(quad->arg2)) {
            bool found = false;
            for (int t = 0; t < tempCount; t++) {
                if (strcmp(temps[t], quad->arg2) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found && tempCount < MAX_VARIABLES) {
                strcpy(temps[tempCount++], quad->arg2);
            }
        }
    }
    
    return tempCount;
}

/**
 * Calculate frame size for a function
 * 
 * Frame layout (Lecture 32-33):
 * - Saved $ra: 4 bytes
 * - Saved $fp: 4 bytes  
 * - Parameters (if > 4): 4 bytes each for params beyond first 4
 * - Local variables: 4 bytes each (assuming int)
 * - Temporaries: 4 bytes each
 */
int calculateFrameSize(const char* funcName, int funcStart, int funcEnd) {
    int frameSize = 8;  // Start with $ra (4) + $fp (4)
    
    // Count parameters - ALL parameters need stack space since we save them in prologue
    // Even the first 4 (passed in $a0-$a3) are saved to stack for recursive calls
    int paramCount = getParameterCount(funcName);
    frameSize += paramCount * 4;
    
    // Count local variables
    int localCount = countLocalsInFunction(funcName);
    frameSize += localCount * 4;
    
    // Count temporaries
    int tempCount = countTemporariesInFunction(funcStart, funcEnd);
    frameSize += tempCount * 4;
    
    // CRITICAL FIX: Add extra space for register spills during code generation
    // Gotos, branches, and function calls may spill up to 10 $t registers
    // Add 40 bytes (10 registers * 4 bytes) to prevent stack overflow
    frameSize += 40;
    
    // Align to 8-byte boundary (MIPS convention)
    if (frameSize % 8 != 0) {
        frameSize += (8 - (frameSize % 8));
    }
    
    return frameSize;
}

// ============================================================================
// Task 1.3: Variable Offset Assignment
// ============================================================================

/**
 * Assign memory offsets to all variables in a function
 * 
 * Stack layout (from high to low address):
 * - Saved $ra at -4($fp)
 * - Saved $fp at -8($fp)
 * - Parameters 5+ (if any) at -12($fp), -16($fp), ...
 * - Local variables at subsequent offsets
 * - Temporaries at subsequent offsets
 */
void assignVariableOffsets(ActivationRecord* record, const char* funcName, 
                           int funcStart, int funcEnd) {
    record->varCount = 0;
    int offset = -12;  // Start after $ra(-4) and $fp(-8)
    
    // 1. Handle parameters
    char params[16][128];
    int paramCount = 0;
    getParameterNames(funcName, params, &paramCount);
    record->numParams = paramCount;
    
    // CRITICAL FIX: Allocate stack space for ALL parameters, including first 4
    // Even though first 4 come in $a0-$a3, they need stack slots for:
    // 1. Saving across function calls (especially recursive calls)
    // 2. Taking addresses of parameters (&param)
    // 3. Preserving values when $a0-$a3 get reused for nested calls
    for (int p = 0; p < paramCount; p++) {
        if (record->varCount < MAX_VARIABLES) {
            strcpy(record->variables[record->varCount].varName, params[p]);
            record->variables[record->varCount].offset = offset;
            record->variables[record->varCount].size = 4;
            record->varCount++;
            offset -= 4;
        }
    }
    
    // 2. Assign offsets to local variables (from symbol table)
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].function_scope, funcName) == 0 && 
            !symtab[i].is_function && 
            strcmp(symtab[i].kind, "variable") == 0) {
            
            // Skip if it's a parameter (already handled)
            bool isParam = false;
            for (int p = 0; p < paramCount; p++) {
                if (strcmp(symtab[i].name, params[p]) == 0) {
                    isParam = true;
                    break;
                }
            }
            
            if (!isParam && record->varCount < MAX_VARIABLES) {
                strcpy(record->variables[record->varCount].varName, symtab[i].name);
                
                // Calculate aligned size (CRITICAL FIX: ensure 4-byte alignment)
                int varSize = symtab[i].size > 0 ? symtab[i].size : 4;
                int alignedSize = ((varSize + 3) / 4) * 4;  // Round up to multiple of 4
                
                //  CRITICAL FIX: For arrays, offset should point to the BEGINNING of the array
                // such that all elements are at negative offsets from $fp.
                // Arrays grow upward in memory (arr[0] at offset, arr[1] at offset+4, etc.)
                if (varSize > 4) {
                    // This is likely an array - place it so all elements fit below current offset
                    offset -= alignedSize;
                    record->variables[record->varCount].offset = offset;

                    // CRITICAL FIX: Move offset down by 4 more bytes to ensure next variable
                    // doesn't overlap with the array base
                    offset -= 4;

                } else {
                    // Regular variable - use old logic (assign then decrement) for compatibility
                    record->variables[record->varCount].offset = offset;
                    offset -= alignedSize;

                }
                
                record->variables[record->varCount].size = varSize;  // Store actual size
                record->varCount++;
            }
        }
    }
    // numLocals is varCount minus all parameters (since all params are now in activation record)
    record->numLocals = record->varCount - paramCount;
    
    // 3. Assign offsets to temporary variables
    char temps[MAX_VARIABLES][128];
    int tempCount = 0;
    
    // Collect unique temporaries
    for (int i = funcStart + 1; i < funcEnd; i++) {
        Quadruple* quad = &IR[i];
        
        // Check result for temporaries
        if (strlen(quad->result) > 0 && isTemporary(quad->result)) {
            bool found = false;
            for (int t = 0; t < tempCount; t++) {
                if (strcmp(temps[t], quad->result) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found && tempCount < MAX_VARIABLES) {
                strcpy(temps[tempCount++], quad->result);
            }
        }
    }
    
    // Assign offsets to temporaries
    for (int t = 0; t < tempCount; t++) {
        if (record->varCount < MAX_VARIABLES) {
            strcpy(record->variables[record->varCount].varName, temps[t]);
            record->variables[record->varCount].offset = offset;
            record->variables[record->varCount].size = 4;
            record->varCount++;
            offset -= 4;
        }
    }
    record->maxTemps = tempCount;
}

/**
 * Get variable offset for code generation
 */
int getVariableOffset(MIPSCodeGenerator* codegen, const char* varName) {
    if (codegen->currentFunction == NULL) {
        return 0;
    }
    
    ActivationRecord* record = codegen->currentFunction;
    
    // Search in activation record
    for (int i = 0; i < record->varCount; i++) {
        if (strcmp(record->variables[i].varName, varName) == 0) {
            return record->variables[i].offset;
        }
    }
    
    // Not found - might be a parameter in register
    return 0;
}

/**
 * Get element size and type for an array from symbol table
 * Returns: element size in bytes
 * Sets isCharArray to true if it's a char array
 */
int getArrayElementInfo(const char* arrayName, bool* isCharArray) {
    *isCharArray = false;  // Default to non-char array
    
    // Search symbol table for the array
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, arrayName) == 0) {
            // CRITICAL FIX: For arrays, distinguish between:
            // - char arr[] : array of chars, element size = 1 byte
            // - char* arr[] : array of char pointers, element size = 4 bytes
            // Check if it's an ARRAY of pointers (ptr_level > 0 && is_array)
            if (symtab[i].is_array && symtab[i].ptr_level > 0) {
                // Array of pointers: char* arr[], int* arr[], etc.
                // Each element is a pointer (4 bytes in MIPS32)
                *isCharArray = false;
                return 4;
            }
            
            // Check if it's a pure char array (not char*)
            if (strstr(symtab[i].type, "char") != NULL && symtab[i].ptr_level == 0) {
                *isCharArray = true;
                return 1;  // char elements are 1 byte
            }
            
            // Check for pointer types (int*, char*, etc.) that are NOT arrays
            if (strstr(symtab[i].type, "*") != NULL || symtab[i].ptr_level > 0) {
                // In MIPS32, all pointers are 4 bytes (32-bit addresses)
                // For pointer arithmetic (ptr + 1), we need the size of the pointed-to type
                if (strstr(symtab[i].type, "char") != NULL && symtab[i].ptr_level == 1) {
                    return 1;  // char* pointer arithmetic: ptr+1 adds 1 byte
                }
                return 4;  // int*, float*, other* pointer arithmetic: ptr+1 adds 4 bytes
            }
            // Default: int, float, etc. are 4 bytes
            return 4;
        }
    }
    
    // Not found in symbol table - default to 4 bytes
    return 4;
}

/**
 * Check if a variable is a pointer (not an array)
 * Returns true for int* ptr, false for int arr[]
 */
bool isPointerVariable(const char* varName) {
    // Search symbol table (without scope restrictions, like getArrayElementInfo)
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, varName) == 0) {
            // It's a pointer if ptr_level > 0 AND not an array
            return (symtab[i].ptr_level > 0 && !symtab[i].is_array);
        }
    }
    return false;
}

// ============================================================================
// Task 1.4: Build Activation Records for All Functions
// ============================================================================

/**
 * Scan IR and build activation records for all functions
 */
void computeActivationRecords() {
    activationRecordCount = 0;
    
    // Find all functions in IR
    int i = 0;
    while (i < irCount) {
        if (isFunctionBegin(&IR[i])) {
            const char* funcName = IR[i].arg1;
            int funcStart = i;
            int funcEnd = findFunctionEnd(funcStart);
            
            if (activationRecordCount < MAX_FUNCTIONS) {
                ActivationRecord* record = &activationRecords[activationRecordCount];
                
                // Set function name
                strcpy(record->funcName, funcName);
                
                // Calculate frame size
                record->frameSize = calculateFrameSize(funcName, funcStart, funcEnd);
                
                // Assign variable offsets
                assignVariableOffsets(record, funcName, funcStart, funcEnd);
                
                activationRecordCount++;
            }
            
            i = funcEnd + 1;
        } else {
            i++;
        }
    }
}

// ============================================================================
// Task 1.5: Debug Output & Testing
// ============================================================================

/**
 * Print activation records for debugging
 */
void printActivationRecords() {
    printf("\n========================================\n");
    printf("ACTIVATION RECORDS (Stack Frame Layout)\n");
    printf("========================================\n\n");
    
    for (int f = 0; f < activationRecordCount; f++) {
        ActivationRecord* record = &activationRecords[f];
        
        printf("Function: %s\n", record->funcName);
        printf("  Frame Size: %d bytes\n", record->frameSize);
        printf("  Parameters: %d (first %d in $a0-$a3)\n", 
               record->numParams, 
               record->numParams < 4 ? record->numParams : 4);
        printf("  Local Variables: %d\n", record->numLocals);
        printf("  Temporaries: %d\n", record->maxTemps);
        printf("\n  Stack Layout:\n");
        printf("    $ra at offset -4($fp)\n");
        printf("    $fp at offset -8($fp)\n");
        
        if (record->varCount > 0) {
            printf("\n  Variables:\n");
            for (int v = 0; v < record->varCount; v++) {
                printf("    %-20s at offset %4d($fp)  [size: %d bytes]\n",
                       record->variables[v].varName,
                       record->variables[v].offset,
                       record->variables[v].size);
            }
        }
        
        printf("\n  Stack Pointer ($sp) at offset -%d($fp)\n", record->frameSize);
        printf("  ----------------------------------------\n\n");
    }
    
    printf("========================================\n");
    printf("Total Functions: %d\n", activationRecordCount);
    printf("========================================\n\n");
}

/**
 * Get activation record for a specific function
 */
ActivationRecord* getActivationRecord(const char* funcName) {
    for (int i = 0; i < activationRecordCount; i++) {
        if (strcmp(activationRecords[i].funcName, funcName) == 0) {
            return &activationRecords[i];
        }
    }
    return NULL;
}

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize MIPS code generator
 */
void initMIPSCodeGen(MIPSCodeGenerator* codegen) {
    if (codegen == NULL) {
        return;
    }
    
    // Initialize all fields
    codegen->IR = IR;
    codegen->irCount = irCount;
    codegen->blocks = blocks;
    codegen->blockCount = blockCount;
    codegen->currentBlock = 0;
    codegen->inFunction = false;
    codegen->currentFuncName[0] = '\0';
    codegen->currentFunction = NULL;
    codegen->funcCount = 0;
    
    // Initialize register descriptors (all registers free initially)
    for (int i = 0; i < 32; i++) {
        codegen->regDescriptors[i].varCount = 0;
        codegen->regDescriptors[i].isDirty = false;
        for (int j = 0; j < 10; j++) {

            codegen->regDescriptors[i].varNames[j][0] = '\0';
        }
    }
    
    // Initialize address descriptors
    codegen->addrDescCount = 0;
    
    // Initialize function call state (Phase 3)
    codegen->currentParamCount = 0;
    codegen->stringCount = 0;
    codegen->floatConstCount = 0;
    codegen->varTypeCount = 0;
    for (int i = 0; i < 10; i++) {
        codegen->paramRegisterMap[i] = -1;
    }
    for (int i = 0; i < 100; i++) {
        codegen->stringLiterals[i][0] = '\0';
        codegen->floatConstants[i][0] = '\0';
    }
    for (int i = 0; i < MAX_VARIABLES; i++) {
        codegen->varTypes[i].varName[0] = '\0';
        codegen->varTypes[i].varType[0] = '\0';
    }
    
    // printf("MIPS Code Generator initialized.\n");  // Commented for clean output
}

/**
 * Main entry point for testing Phase 1
 */
void testActivationRecords() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  PHASE 1: Runtime Environment & Stack Frame Management   ║\n");

    printf("║  Testing Activation Record Generation                     ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Compute activation records for all functions
    computeActivationRecords();
    
    // Print the results
    printActivationRecords();
}

// ============================================================================
// PHASE 2: MIPS Code Generation with Register Allocation (Lectures 34-35)
// ============================================================================

// ============================================================================
// Task 2.1: Descriptor Management
// ============================================================================

/**
 * Initialize descriptors for code generation
 */
void initDescriptors(MIPSCodeGenerator* codegen) {
    // Initialize all register descriptors (all registers start empty)
    for (int i = 0; i < 32; i++) {
        codegen->regDescriptors[i].varCount = 0;
        codegen->regDescriptors[i].isDirty = false;
        for (int j = 0; j < 10; j++) {
            codegen->regDescriptors[i].varNames[j][0] = '\0';
        }
    }
    
    // Initialize address descriptors
    codegen->addrDescCount = 0;
}

/**
 * Find or create address descriptor for a variable
 */
int findOrCreateAddrDesc(MIPSCodeGenerator* codegen, const char* varName) {
    // Search for existing descriptor
    for (int i = 0; i < codegen->addrDescCount; i++) {
        if (strcmp(codegen->addrDescriptors[i].varName, varName) == 0) {
            return i;
        }
    }
    
    // Create new descriptor
    if (codegen->addrDescCount >= MAX_VARIABLES) {
        fprintf(stderr, "Error: Too many variables for address descriptors\n");
        return -1;
    }
    
    int idx = codegen->addrDescCount++;
    strcpy(codegen->addrDescriptors[idx].varName, varName);
    
    // CRITICAL FIX: Temporaries start in registers, not memory
    // Only assume in memory if it's a regular variable (from symbol table or activation record)
    bool isTemp = (varName[0] == 't' && isdigit(varName[1]));
    codegen->addrDescriptors[idx].inMemory = !isTemp;   // Temps NOT in memory initially
    codegen->addrDescriptors[idx].inRegister = -1;    // Not in register yet
    codegen->addrDescriptors[idx].memoryOffset = 0;
    codegen->addrDescriptors[idx].isGlobal = false;
    
    // Get offset from current function's activation record
    if (codegen->currentFunction) {
        for (int v = 0; v < codegen->currentFunction->varCount; v++) {
            if (strcmp(codegen->currentFunction->variables[v].varName, varName) == 0) {
                codegen->addrDescriptors[idx].memoryOffset = 
                    codegen->currentFunction->variables[v].offset;
                // If found in activation record, it has a memory location
                codegen->addrDescriptors[idx].inMemory = true;
                break;
            }
        }
    }
    
    return idx;
}

/**
 * Update descriptors when a variable is loaded into a register
 */
void updateDescriptors(MIPSCodeGenerator* codegen, int regNum, const char* varName) {
    // Update register descriptor - add variable to register
    bool found = false;
    for (int i = 0; i < codegen->regDescriptors[regNum].varCount; i++) {
        if (strcmp(codegen->regDescriptors[regNum].varNames[i], varName) == 0) {
            found = true;
            break;
        }
    }
    
    if (!found && codegen->regDescriptors[regNum].varCount < 10) {
        strcpy(codegen->regDescriptors[regNum].varNames[codegen->regDescriptors[regNum].varCount], 
               varName);
        codegen->regDescriptors[regNum].varCount++;
    }
    
    // CRITICAL FIX: Mark register as dirty when a value is written to it
    // This ensures the value is spilled before function calls or control flow changes
    codegen->regDescriptors[regNum].isDirty = true;
    
    // Update address descriptor - variable is now in this register
    int addrIdx = findOrCreateAddrDesc(codegen, varName);
    if (addrIdx >= 0) {
        codegen->addrDescriptors[addrIdx].inRegister = regNum;
    }
}

/**
 * Clear register descriptor
 * CRITICAL FIX: Also invalidate all address descriptors pointing to this register
 * This ensures consistency - if register is cleared, no variable should think it's still there
 */
void clearRegisterDescriptor(MIPSCodeGenerator* codegen, int regNum) {
    codegen->regDescriptors[regNum].varCount = 0;
    codegen->regDescriptors[regNum].isDirty = false;
    
    // CRITICAL FIX: Invalidate all address descriptors pointing to this register
    // This prevents stale "inRegister" references after register is cleared
    for (int i = 0; i < codegen->addrDescCount; i++) {
        if (codegen->addrDescriptors[i].inRegister == regNum) {
            codegen->addrDescriptors[i].inRegister = -1;
        }
    }
    
    for (int i = 0; i < 10; i++) {
        codegen->regDescriptors[regNum].varNames[i][0] = '\0';
    }
}

/**
 * Find variable in address descriptors
 */
int findAddressDescriptor(MIPSCodeGenerator* codegen, const char* varName) {
    for (int i = 0; i < codegen->addrDescCount; i++) {
        if (strcmp(codegen->addrDescriptors[i].varName, varName) == 0) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Emit a MIPS instruction to output file
 */
void emitMIPS(MIPSCodeGenerator* codegen, const char* instruction) {
    if (codegen->outputFile) {
        fprintf(codegen->outputFile, "%s\n", instruction);
    }
}

/**
 * Get register name string (e.g., "$t0")
 */
const char* getRegisterName(int regNum) {
    static char regName[8];
    
    switch (regNum) {
        case REG_ZERO: return "$zero";
        case REG_V0: return "$v0";
        case REG_V1: return "$v1";
        case REG_A0: return "$a0";
        case REG_A1: return "$a1";
        case REG_A2: return "$a2";
        case REG_A3: return "$a3";
        case REG_T0: return "$t0";
        case REG_T1: return "$t1";
        case REG_T2: return "$t2";
        case REG_T3: return "$t3";
        case REG_T4: return "$t4";
        case REG_T5: return "$t5";
        case REG_T6: return "$t6";
        case REG_T7: return "$t7";
        case REG_S0: return "$s0";
        case REG_S1: return "$s1";
        case REG_S2: return "$s2";
        case REG_S3: return "$s3";
        case REG_S4: return "$s4";
        case REG_S5: return "$s5";
        case REG_S6: return "$s6";
        case REG_S7: return "$s7";
        case REG_T8: return "$t8";
        case REG_T9: return "$t9";
        case REG_SP: return "$sp";
        case REG_FP: return "$fp";
        case REG_RA: return "$ra";
        default:
            sprintf(regName, "$%d", regNum);
            return regName;
    }
}

/**
 * Check if variable is global or static (should be in .data section)
 */
bool isGlobalVariable(MIPSCodeGenerator* codegen, const char* varName) {
    // Check symbol table for global variables
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, varName) == 0) {
            if (symtab[i].scope_level == 0 && !symtab[i].is_function) {
                return true;
            }
        }
    }
    
    // Check staticVars array for static local variables (e.g., "func.var")
    extern StaticVarInfo staticVars[];
    extern int staticVarCount;
    for (int i = 0; i < staticVarCount; i++) {
        if (strcmp(staticVars[i].name, varName) == 0) {
            return true;
        }
    }
    
    // Also check if variable name contains '.' which indicates static local variable
    // (e.g., "static_function.local_static", "main.main_static")
    // BUT: Make sure it's not a floating point constant (e.g., "10.5f", "3.14")
    if (strchr(varName, '.') != NULL && !isConstantValue(varName)) {
        return true;
    }
    
    return false;
}

/**
 * Get memory location string for a variable
 */
void getMemoryLocation(MIPSCodeGenerator* codegen, const char* varName, char* location) {
    // Check if it's a global variable
    if (isGlobalVariable(codegen, varName)) {
        sprintf(location, "%s", varName);
        return;
    }
    
    // CRITICAL FIX: Always look up offset directly from activation record for local variables
    // This ensures we get the correct, authoritative memory location for each variable
    // Don't rely on address descriptor's memoryOffset which may be stale or incorrect
    if (codegen->currentFunction != NULL) {
        for (int i = 0; i < codegen->currentFunction->varCount; i++) {
            if (strcmp(codegen->currentFunction->variables[i].varName, varName) == 0) {
                sprintf(location, "%d($fp)", codegen->currentFunction->variables[i].offset);
                return;
            }
        }
    }
    
    // Fallback: check address descriptor (for variables not in activation record)
    int addrIdx = findAddressDescriptor(codegen, varName);
    if (addrIdx >= 0 && codegen->addrDescriptors[addrIdx].memoryOffset != 0) {
        sprintf(location, "%d($fp)", codegen->addrDescriptors[addrIdx].memoryOffset);
        return;
    }
    
    // Still not found - this is an error, but provide a fallback
    fprintf(stderr, "Warning: Variable '%s' not found in activation record\n", varName);
    sprintf(location, "-12($fp)");
}

// ============================================================================
// Task 2.2: getReg Algorithm (Lecture 35) - THE CORE ALGORITHM
// ============================================================================

/**
 * Spill a register to memory
 */
void spillRegister(MIPSCodeGenerator* codegen, int regNum) {
    RegisterDescriptor* regDesc = &codegen->regDescriptors[regNum];
    
    for (int i = 0; i < regDesc->varCount; i++) {
        const char* varName = regDesc->varNames[i];
        
        // CRITICAL: Skip constants - they should never be stored back to memory
        if (isConstantValue(varName)) {
            continue;
        }
        
        // Find address descriptor
        int addrIdx = findAddressDescriptor(codegen, varName);
        if (addrIdx < 0) continue;
        
        // Only store if NOT already in memory or if dirty
        if (!codegen->addrDescriptors[addrIdx].inMemory || regDesc->isDirty) {
            storeVariable(codegen, varName, regNum);
            codegen->addrDescriptors[addrIdx].inMemory = true;
        }
        
        // Remove register from address descriptor
        codegen->addrDescriptors[addrIdx].inRegister = -1;
    }
    
    // Clear register descriptor
    clearRegisterDescriptor(codegen, regNum);
}

/**
 * Load variable from memory into register
 */
void loadVariable(MIPSCodeGenerator* codegen, const char* varName, int regNum) {
    char instr[256];
    
    // Check if it's a constant
    if (isConstantValue(varName)) {
        // Handle string literals - CRITICAL FIX
        // String literals start with " and cannot be loaded with li instruction
        // They must be handled via la (load address) from .data section
        if (varName[0] == '"') {
            // String literal - skip loading, should be handled in data section
            sprintf(instr, "    # String literal %s - use la to load address from .data", varName);
            emitMIPS(codegen, instr);
            return;
        }
        
        // Handle float constants - load from .data section
        if (isFloatConstant(varName)) {
            // Find or create float constant in data section
            int floatIndex = -1;
            for (int i = 0; i < codegen->floatConstCount; i++) {
                if (strcmp(codegen->floatConstants[i], varName) == 0) {
                    floatIndex = i;
                    break;
                }
            }
            
            if (floatIndex < 0 && codegen->floatConstCount < 100) {
                // Add new float constant
                floatIndex = codegen->floatConstCount;
                strcpy(codegen->floatConstants[floatIndex], varName);
                codegen->floatConstCount++;
            }
            
            if (floatIndex >= 0) {
                // Load float bits from data section as integer for storage
                sprintf(instr, "    lw %s, _float%d    # Load float %s", 
                        getRegisterName(regNum), floatIndex, varName);
            } else {
                // Fallback - convert to integer
                double floatValue = atof(varName);
                sprintf(instr, "    li %s, %d    # Float %s (fallback)", 
                        getRegisterName(regNum), (int)floatValue, varName);
            }
            emitMIPS(codegen, instr);
            return;
        }
        
        // Handle character literals
        if (varName[0] == '\'') {
            // Extract ASCII value from 'X' or '\X' for escape sequences
            if (strlen(varName) >= 3) {
                int asciiValue;
                char displayChar;
                
                if (varName[1] == '\\' && strlen(varName) >= 4) {
                    // Escape sequence like '\0', '\n', '\t', etc.
                    switch (varName[2]) {
                        case '0': asciiValue = 0; displayChar = '0'; break;   // null
                        case 'n': asciiValue = 10; displayChar = 'n'; break;  // newline
                        case 't': asciiValue = 9; displayChar = 't'; break;   // tab
                        case 'r': asciiValue = 13; displayChar = 'r'; break;  // carriage return
                        case '\\': asciiValue = 92; displayChar = '\\'; break; // backslash
                        case '\'': asciiValue = 39; displayChar = '\''; break; // single quote
                        case '"': asciiValue = 34; displayChar = '"'; break;  // double quote
                        default: asciiValue = (int)varName[2]; displayChar = varName[2]; break;
                    }
                    sprintf(instr, "    li %s, %d    # '\\%c'", 
                            getRegisterName(regNum), asciiValue, displayChar);
                } else {
                    // Regular character like 'A', 'B', etc.
                    asciiValue = (int)varName[1];
                    sprintf(instr, "    li %s, %d    # '%c'", 
                            getRegisterName(regNum), asciiValue, varName[1]);
                }
                emitMIPS(codegen, instr);
                return;
            }
        }
        
        // Load integer immediate value
        sprintf(instr, "    li %s, %s", getRegisterName(regNum), varName);
        emitMIPS(codegen, instr);
        return;
    }
    
    // Get memory location
    char location[128];
    getMemoryLocation(codegen, varName, location);
    
    // Generate load instruction
    sprintf(instr, "    lw %s, %s", getRegisterName(regNum), location);
    emitMIPS(codegen, instr);
}

/**
 * Store register value to variable's memory location
 */
void storeVariable(MIPSCodeGenerator* codegen, const char* varName, int regNum) {
    char instr[256];
    char location[128];
    
    getMemoryLocation(codegen, varName, location);
    sprintf(instr, "    sw %s, %s", getRegisterName(regNum), location);
    emitMIPS(codegen, instr);
}

/**
 * getReg Algorithm (Lecture 35)
 * Find a register to hold a variable
 * Returns register number (REG_T0 to REG_T9)
 */
int getReg(MIPSCodeGenerator* codegen, const char* varName, int irIndex) {
    char instr[256];
    
    // Skip if varName is empty or a constant
    if (varName == NULL || strlen(varName) == 0) {
        return REG_T0;  // Default register
    }
    
    // CRITICAL FIX: Check if this variable is an ARRAY
    // In C, when an array name is used in an expression, it decays to a pointer to its first element
    // So we need to load the ADDRESS, not a value from memory
    bool isArray = false;
    
    // First check symbol table for is_array flag (most reliable)
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, varName) == 0) {
            // Check if it's in current function scope (or global if no current function)
            bool inScope = false;
            if (codegen->currentFunction == NULL) {
                // Global scope
                inScope = (strcmp(symtab[i].function_scope, "") == 0 || 
                          strcmp(symtab[i].function_scope, "global") == 0);
            } else {
                // Local scope
                inScope = (strcmp(symtab[i].function_scope, codegen->currentFunction->funcName) == 0);
            }
            
            if (inScope && symtab[i].is_array && symtab[i].ptr_level == 0) {
                isArray = true;
                break;
            }
        }
    }
    
    // If it's an array, we need to compute its ADDRESS, not load a value
    if (isArray) {
        // Find an empty register or reuse if already computed
        for (int r = REG_T0; r <= REG_T9; r++) {
            // Check if we already have this array's address in a register
            for (int v = 0; v < codegen->regDescriptors[r].varCount; v++) {
                if (strcmp(codegen->regDescriptors[r].varNames[v], varName) == 0) {
                    return r;  // Already have the address!
                }
            }
        }
        
        // Find empty register or spill one
        int targetReg = REG_T0;
        for (int r = REG_T0; r <= REG_T9; r++) {
            if (codegen->regDescriptors[r].varCount == 0) {
                targetReg = r;
                break;
            }
        }
        
        if (codegen->regDescriptors[targetReg].varCount > 0) {
            // Must spill
            spillRegister(codegen, targetReg);
        }
        
        // Compute array base address: addiu reg, $fp, offset
        char location[128];
        getMemoryLocation(codegen, varName, location);
        
        // Parse offset from location string like "-24($fp)"
        int offset = 0;
        if (strstr(location, "($fp)") != NULL) {
            sscanf(location, "%d($fp)", &offset);
            sprintf(instr, "    addiu %s, $fp, %d    # Load address of array %s", 
                    getRegisterName(targetReg), offset, varName);
            emitMIPS(codegen, instr);
        } else {
            // Global array - use la
            sprintf(instr, "    la %s, %s    # Load address of global array %s", 
                    getRegisterName(targetReg), varName, varName);
            emitMIPS(codegen, instr);
        }
        
        // CRITICAL: Do NOT update descriptors with the array variable name!
        // The register holds the array's ADDRESS, not the array data itself
        // If we associate the register with "arr", it will try to spill back to arr's location
        // which would overwrite the array data with its own address!
        // Instead, just mark the register as in use but don't track it
        // (This makes the address computation ephemeral - used once then discarded)
        
        return targetReg;
    }
    
    if (isConstantValue(varName)) {
        // For constants, find an empty register and load the constant
        for (int r = REG_T0; r <= REG_T9; r++) {
            if (codegen->regDescriptors[r].varCount == 0) {
                loadVariable(codegen, varName, r);
                // CRITICAL FIX: Must update descriptors to prevent register reallocation during same instruction
                // Constants need temporary tracking within an instruction to avoid clobbering
                // Use a special marker (constant itself) to mark register as busy
                // BUT: Do NOT mark as dirty - constants should never be stored back to memory!
                updateDescriptors(codegen, r, varName);
                codegen->regDescriptors[r].isDirty = false;  // Constants are read-only
                return r;
            }
        }
        // If no empty register, must spill one
        // DON'T blindly use REG_T0 - that might hold a live variable!
        // Use the same spilling logic as below
        int victimReg = REG_T0;
        
        // Try to find a better victim using next-use information
        int maxNextUse = -1;
        for (int r = REG_T0; r <= REG_T9; r++) {
            if (codegen->regDescriptors[r].varCount > 0) {
                const char* victimVar = codegen->regDescriptors[r].varNames[0];
                
                // Check if variable is dead (no next use)
                bool isLive = false;
                int nextUseIdx = -1;
                getNextUseInfo(irIndex, victimVar, &isLive, &nextUseIdx);
                
                if (!isLive || nextUseIdx < 0) {
                    // Dead variable - perfect victim!
                    victimReg = r;
                    break;
                }
                
                // Otherwise, pick variable with furthest next use
                if (nextUseIdx > maxNextUse) {
                    maxNextUse = nextUseIdx;
                    victimReg = r;
                }
            }
        }
        
        // Spill the victim, CLEAR the register descriptor, and load constant
        spillRegister(codegen, victimReg);
        clearRegisterDescriptor(codegen, victimReg);  // CRITICAL: Clear before loading constant!
        loadVariable(codegen, varName, victimReg);
        // Update descriptors to mark register as busy with this constant
        // BUT: Do NOT mark as dirty - constants should never be stored back!
        updateDescriptors(codegen, victimReg, varName);
        codegen->regDescriptors[victimReg].isDirty = false;  // Constants are read-only
        return victimReg;
    }
    
    // CASE 1: Variable already in a register? (BEST CASE)
    int addrIdx = findAddressDescriptor(codegen, varName);
    if (addrIdx >= 0) {
        int reg = codegen->addrDescriptors[addrIdx].inRegister;
        if (reg >= REG_T0 && reg <= REG_T9) {
            return reg;  // Already loaded!
        }
    }
    
    // CRITICAL FIX: Address descriptor might be stale - search ALL registers
    // to see if any of them currently holds this variable
    for (int r = REG_T0; r <= REG_T9; r++) {
        for (int v = 0; v < codegen->regDescriptors[r].varCount; v++) {
            if (strcmp(codegen->regDescriptors[r].varNames[v], varName) == 0) {
                // Found it! Update address descriptor and return
                if (addrIdx >= 0) {
                    codegen->addrDescriptors[addrIdx].inRegister = r;
                }
                return r;
            }
        }
    }
    
    // CASE 2: Find an empty temporary register
    for (int r = REG_T0; r <= REG_T9; r++) {
        if (codegen->regDescriptors[r].varCount == 0) {
            // Empty register found!
            // Load from memory if:
            // 1. Variable has inMemory flag set (was previously stored), OR  
            // 2. Variable is a global/static variable (in .data section), OR
            // 3. Variable is a parameter (saved to stack in prologue)
            //
            // CRITICAL FIX: Do NOT load temporaries that have never been stored (inMemory = false)
            // Temporaries only exist in registers until explicitly spilled
            bool shouldLoad = false;
            
            if (addrIdx >= 0 && codegen->addrDescriptors[addrIdx].inMemory) {
                shouldLoad = true;
            } else if (isGlobalVariable(codegen, varName)) {
                shouldLoad = true;
            } else if (addrIdx < 0 && codegen->currentFunction != NULL) {
                // No address descriptor - check if this is a declared local variable or parameter
                // If it has a stack location, we should load from memory (might have been written by scanf/etc)
                for (int v = 0; v < codegen->currentFunction->varCount; v++) {
                    if (strcmp(codegen->currentFunction->variables[v].varName, varName) == 0) {
                        // Found in activation record - it's a real variable with storage, load it
                        shouldLoad = true;
                        break;
                    }
                }
            } else if (codegen->currentFunction != NULL) {
                // Check if this is a parameter - parameters are saved in prologue
                char params[16][128];
                int paramCount = 0;
                getParameterNames(codegen->currentFunction->funcName, params, &paramCount);
                
                for (int p = 0; p < paramCount; p++) {
                    if (strcmp(params[p], varName) == 0) {
                        shouldLoad = true;
                        break;
                    }
                }
            }
            
            if (shouldLoad) {
                loadVariable(codegen, varName, r);
            }
            updateDescriptors(codegen, r, varName);
            return r;
        }
    }
    
    // CASE 3: Must spill a register (all registers full)
    // Simple heuristic: spill $t0 (in real implementation, use next-use info)
    int victimReg = REG_T0;
    
    // Try to find a better victim using next-use information
    int maxNextUse = -1;
    for (int r = REG_T0; r <= REG_T9; r++) {
        if (codegen->regDescriptors[r].varCount > 0) {
            const char* victimVar = codegen->regDescriptors[r].varNames[0];
            
            // Check if variable is dead (no next use)
            bool isLive = false;
            int nextUseIdx = -1;
            getNextUseInfo(irIndex, victimVar, &isLive, &nextUseIdx);
            
            if (!isLive || nextUseIdx < 0) {
                // Dead variable - perfect victim!
                victimReg = r;
                break;
            }
            
            // Otherwise, pick variable with furthest next use
            if (nextUseIdx > maxNextUse) {
                maxNextUse = nextUseIdx;
                victimReg = r;
            }
        }
    }
    
    // Spill the victim register
    spillRegister(codegen, victimReg);
    
    // FIX: Only load from memory if variable has been initialized
    if (addrIdx >= 0 && codegen->addrDescriptors[addrIdx].inMemory) {
        loadVariable(codegen, varName, victimReg);
    }
    updateDescriptors(codegen, victimReg, varName);
    
    return victimReg;
}

// ============================================================================
// Task 2.3 & 2.4: Instruction Translation
// ============================================================================

/**
 * Translate arithmetic operation (ADD, SUB, MUL, DIV, MOD)
 */
void translateArithmetic(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    int regResult;  // Declare here, assign in each branch
    
    // FLOAT ARITHMETIC DETECTION: Check if operation involves floats via type annotation or constant detection
    bool isFloatOp = false;
    if (quad->resultType[0] != '\0') {
        // Use type annotation if available
        isFloatOp = (strcmp(quad->resultType, "float") == 0 || strcmp(quad->resultType, "double") == 0);
    } else {
        // Fallback: check if either operand is a float constant or float variable
        isFloatOp = isFloatConstant(quad->arg1) || isFloatConstant(quad->arg2) ||
                    strcmp(getVariableType(codegen, quad->arg1), "float") == 0 ||
                    strcmp(getVariableType(codegen, quad->arg2), "float") == 0;
    }
    
    if (isFloatOp && strlen(quad->arg2) > 0) {
        // Handle float arithmetic using floating-point coprocessor
        // Load arg1 into float register
        int freg1 = 0;  // Use $f0-$f3 for temporaries
        int regArg1Temp = getReg(codegen, quad->arg1, irIndex);
        sprintf(instr, "    mtc1 %s, $f%d    # Move arg1 to float reg", 
                getRegisterName(regArg1Temp), freg1);
        emitMIPS(codegen, instr);
        
        // Load arg2 into float register
        int freg2 = 2;
        int regArg2Temp = getReg(codegen, quad->arg2, irIndex);
        sprintf(instr, "    mtc1 %s, $f%d    # Move arg2 to float reg", 
                getRegisterName(regArg2Temp), freg2);
        emitMIPS(codegen, instr);
        
        // Perform floating-point operation
        int fregResult = 4;
        if (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "+") == 0) {
            sprintf(instr, "    add.s $f%d, $f%d, $f%d", fregResult, freg1, freg2);
        } else if (strcmp(quad->op, "SUB") == 0 || strcmp(quad->op, "-") == 0) {
            sprintf(instr, "    sub.s $f%d, $f%d, $f%d", fregResult, freg1, freg2);
        } else if (strcmp(quad->op, "MUL") == 0 || strcmp(quad->op, "*") == 0) {
            sprintf(instr, "    mul.s $f%d, $f%d, $f%d", fregResult, freg1, freg2);
        } else if (strcmp(quad->op, "DIV") == 0 || strcmp(quad->op, "/") == 0) {
            sprintf(instr, "    div.s $f%d, $f%d, $f%d", fregResult, freg1, freg2);
        } else {
            // Fallback to integer operations
            isFloatOp = false;
            goto integer_arithmetic;
        }
        emitMIPS(codegen, instr);
        
        // Move result back to integer register for storage
        regResult = getReg(codegen, quad->result, irIndex);
        sprintf(instr, "    mfc1 %s, $f%d    # Move float result to int reg", 
                getRegisterName(regResult), fregResult);
        emitMIPS(codegen, instr);
        
        // Update descriptors and store
        updateDescriptors(codegen, regResult, quad->result);
        codegen->regDescriptors[regResult].isDirty = true;
        storeVariable(codegen, quad->result, regResult);
        int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
        }
        
        // Register result as float type
        registerVariableType(codegen, quad->result, "float");
        return;
    }
    
integer_arithmetic:
    // CRITICAL FIX: Detect pointer arithmetic FIRST before loading arg1
    // When doing ptr+1 or arr+3, we need to get the ADDRESS, not the VALUE
    bool isPointerArithmetic = false;
    int pointerScale = 1;
    bool arg1IsArray = false;
    
    // PTR_ADD and PTR_SUB operations are ALWAYS pointer arithmetic
    if (strcmp(quad->op, "PTR_ADD") == 0 || strcmp(quad->op, "PTR_SUB") == 0) {
        isPointerArithmetic = true;
        // Determine element size based on pointer type
        bool isCharArray = false;
        int elementSize = getArrayElementInfo(quad->arg1, &isCharArray);
        pointerScale = elementSize;
    }
    // Also check for regular ADD/SUB operations that are actually pointer arithmetic
    else {
        // Use scope-independent lookup to check if arg1 is a pointer or array
        bool arg1IsPtr = isPointerVariable(quad->arg1);
        
        // Also check if it's an array by searching symbol table without scope restrictions
        bool arg1IsArr = false;
        for (int i = 0; i < symCount; i++) {
            if (strcmp(symtab[i].name, quad->arg1) == 0) {
                // It's an array ONLY if is_array is true AND ptr_level is 0
                // For pointer parameters (int* arr), ptr_level should be 1, so NOT an array
                if (symtab[i].is_array && symtab[i].ptr_level == 0) {
                    arg1IsArr = true;
                    arg1IsArray = true;  // Remember if it's an array
                }
                break;
            }
        }
        
        if (arg1IsPtr || arg1IsArr) {
            isPointerArithmetic = true;
            // Determine element size based on pointer/array type
            bool isCharArray = false;
            int elementSize = getArrayElementInfo(quad->arg1, &isCharArray);
            pointerScale = elementSize;  // 1 for char, 4 for int/float/pointer
        }
    }
    
    // Load arg1: For arrays in pointer arithmetic, compute ADDRESS; for others, load VALUE
    int regArg1;
    if (isPointerArithmetic && arg1IsArray) {
        // arg1 is an array name - compute its address, not load its value
        int offset = getVariableOffset(codegen, quad->arg1);
        regArg1 = REG_T0;  // Use a temporary register
        for (int r = REG_T0; r <= REG_T9; r++) {
            if (codegen->regDescriptors[r].varCount == 0) {
                regArg1 = r;
                break;
            }
        }
        sprintf(instr, "    addiu %s, $fp, %d", getRegisterName(regArg1), offset);
        emitMIPS(codegen, instr);
        // Don't update descriptors - this is a temporary address computation
    } else {
        // Regular variable or pointer - load its value
        regArg1 = getReg(codegen, quad->arg1, irIndex);
    }
    
    // Handle binary operations
    if (strlen(quad->arg2) > 0 && strcmp(quad->arg2, "") != 0) {
        // Binary operation: result = arg1 op arg2
        
        // CRITICAL FIX for float constants: must load both operands into DIFFERENT registers first
        // For float constants like 0.5, isConstantValue() returns true BUT isFloatConstant() also returns true
        // We cannot use immediate instructions with floats, so we MUST load both operands into registers
        // The bug was that `f = f + 0.5` would load f into $t0, then load 0.5 into $t0 (clobber), causing `add $t0, $t0, $t0`
        
        // Check if arg2 is a constant (can use immediate instruction)
        // BUT: Only use addi for integer constants, not floats
        if (isConstantValue(quad->arg2) && 
            (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "+") == 0 || 
             strcmp(quad->op, "SUB") == 0 || strcmp(quad->op, "-") == 0 ||
             strcmp(quad->op, "PTR_ADD") == 0 || strcmp(quad->op, "PTR_SUB") == 0) && 
            !isFloatConstant(quad->arg2)) {
            // Use immediate instruction for ADD/SUB with integer constant
            // Get result register AFTER loading arg1
            regResult = getReg(codegen, quad->result, irIndex);
            
            // POINTER ARITHMETIC FIX: Scale the constant by pointer element size
            int scaledValue = atoi(quad->arg2);
            if (isPointerArithmetic) {
                scaledValue *= pointerScale;
            }
            
            sprintf(instr, "    %s %s, %s, %d", 
                   (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "+") == 0 || strcmp(quad->op, "PTR_ADD") == 0 ? "addi" : "addi"),
                   getRegisterName(regResult), 
                   getRegisterName(regArg1), 
                   (strcmp(quad->op, "SUB") == 0 || strcmp(quad->op, "-") == 0 || strcmp(quad->op, "PTR_SUB") == 0 ? -scaledValue : scaledValue));
            emitMIPS(codegen, instr);
        } else {
            // Load arg2 (constant or variable) into a register
            int regArg2 = getReg(codegen, quad->arg2, irIndex);
            
            // CRITICAL FIX: If loading arg2 clobbered arg1's register, reload arg1
            // This can happen when arg2 is a constant and getReg chooses to spill arg1's register
            if (regArg2 == regArg1) {
                // They're in the same register - arg1 was clobbered!
                // Reload arg1 into a different register
                if (isConstantValue(quad->arg2)) {
                    // arg2 is a constant that clobbered arg1
                    // Move the constant to a different register
                    for (int r = REG_T0; r <= REG_T9; r++) {
                        if (r != regArg1 && codegen->regDescriptors[r].varCount == 0) {
                            // Found an empty register different from arg1
                            sprintf(instr, "    move %s, %s", 
                                   getRegisterName(r), getRegisterName(regArg2));
                            emitMIPS(codegen, instr);
                            regArg2 = r;
                            // Now reload arg1 from memory
                            loadVariable(codegen, quad->arg1, regArg1);
                            break;
                        }
                    }
                } else {
                    // Both are variables - shouldn't happen but reload arg1 to be safe
                    loadVariable(codegen, quad->arg1, regArg1);
                }
            }
            
            // NOW get result register (might reuse one of the operand registers if result == operand)
            regResult = getReg(codegen, quad->result, irIndex);
            
            // POINTER ARITHMETIC FIX: If arg2 is a variable, scale it by element size
            if (isPointerArithmetic && 
                (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "+") == 0 || strcmp(quad->op, "PTR_ADD") == 0 ||
                 strcmp(quad->op, "SUB") == 0 || strcmp(quad->op, "-") == 0 || strcmp(quad->op, "PTR_SUB") == 0)) {
                // Scale arg2 by element size (1 for char, 4 for int/float/pointer)
                if (pointerScale == 1) {
                    // No scaling needed for char arrays - use arg2 directly
                    if (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "+") == 0 || strcmp(quad->op, "PTR_ADD") == 0) {
                        sprintf(instr, "    add %s, %s, %s", 
                               getRegisterName(regResult),
                               getRegisterName(regArg1),
                               getRegisterName(regArg2));
                    } else { // SUB or - or PTR_SUB
                        sprintf(instr, "    sub %s, %s, %s", 
                               getRegisterName(regResult),
                               getRegisterName(regArg1),
                               getRegisterName(regArg2));
                    }
                    emitMIPS(codegen, instr);
                } else if (pointerScale == 4) {
                    // Scale arg2 by 4 using shift left by 2 (multiply by 4)
                    sprintf(instr, "    sll $t9, %s, 2", getRegisterName(regArg2));
                    emitMIPS(codegen, instr);
                    
                    // Now do the add/sub with scaled value
                    if (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "+") == 0 || strcmp(quad->op, "PTR_ADD") == 0) {
                        sprintf(instr, "    add %s, %s, $t9", 
                               getRegisterName(regResult),
                               getRegisterName(regArg1));
                    } else { // SUB or - or PTR_SUB
                        sprintf(instr, "    sub %s, %s, $t9", 
                               getRegisterName(regResult),
                               getRegisterName(regArg1));
                    }
                    emitMIPS(codegen, instr);
                } else {
                    // Other scales (e.g., 2, 8) - use multiplication
                    sprintf(instr, "    li $t9, %d", pointerScale);
                    emitMIPS(codegen, instr);
                    sprintf(instr, "    mul $t9, %s, $t9", getRegisterName(regArg2));
                    emitMIPS(codegen, instr);
                    
                    if (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "+") == 0 || strcmp(quad->op, "PTR_ADD") == 0) {
                        sprintf(instr, "    add %s, %s, $t9", 
                               getRegisterName(regResult),
                               getRegisterName(regArg1));
                    } else { // SUB or - or PTR_SUB
                        sprintf(instr, "    sub %s, %s, $t9", 
                               getRegisterName(regResult),
                               getRegisterName(regArg1));
                    }
                    emitMIPS(codegen, instr);
                }
            } else if (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "+") == 0 || strcmp(quad->op, "PTR_ADD") == 0) {
                sprintf(instr, "    add %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            } else if (strcmp(quad->op, "SUB") == 0 || strcmp(quad->op, "-") == 0 || strcmp(quad->op, "PTR_SUB") == 0) {
                sprintf(instr, "    sub %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            } else if (strcmp(quad->op, "MUL") == 0 || strcmp(quad->op, "*") == 0) {
                sprintf(instr, "    mul %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            } else if (strcmp(quad->op, "DIV") == 0 || strcmp(quad->op, "/") == 0) {
                sprintf(instr, "    div %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            } else if (strcmp(quad->op, "MOD") == 0) {
                sprintf(instr, "    rem %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            } else {
                return;  // Unknown operation
            }
            emitMIPS(codegen, instr);
        }
    } else {
        // Unary operation or just assignment
        regResult = getReg(codegen, quad->result, irIndex);
        
        if (strcmp(quad->op, "NEG") == 0) {
            sprintf(instr, "    neg %s, %s", 
                   getRegisterName(regResult),
                   getRegisterName(regArg1));
            emitMIPS(codegen, instr);
        }
    }
    
    // Update descriptors - result is now in register
    updateDescriptors(codegen, regResult, quad->result);
    codegen->regDescriptors[regResult].isDirty = true;
    
    // CRITICAL FIX: Store result to memory immediately for reloadability
    // This ensures the value is available when needed by future instructions (e.g., printf parameters)
    storeVariable(codegen, quad->result, regResult);
    int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
    if (addrIdx >= 0) {
        codegen->addrDescriptors[addrIdx].inMemory = true;
    }
}

/**
 * Translate simple assignment: result = arg1
 */
void translateAssignment(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    // CRITICAL FIX: Handle array access in assignment (arr[i] syntax)
    // If arg1 contains '[', parse it and load the array element directly
    if (strchr(quad->arg1, '[') != NULL) {
        char arrayName[128];
        char indexStr[128];
        
        // Parse "arr[index]" into arrayName and indexStr
        const char* bracket = strchr(quad->arg1, '[');
        int nameLen = bracket - quad->arg1;
        strncpy(arrayName, quad->arg1, nameLen);
        arrayName[nameLen] = '\0';
        
        // Extract index
        const char* closeBracket = strchr(bracket, ']');
        int indexLen = closeBracket - bracket - 1;
        strncpy(indexStr, bracket + 1, indexLen);
        indexStr[indexLen] = '\0';
        
        // Check if arrayName is a pointer variable (not an array)
        bool isPointer = isPointerVariable(arrayName);
        
        if (isPointer) {
            // Pointer indexing: p[i] = *(p + i)
            // Load pointer value (address)
            int ptrReg = getReg(codegen, arrayName, irIndex);
            int resultReg = getReg(codegen, quad->result, irIndex);
            
            // Determine element size
            bool isCharArray = false;
            int elementSize = getArrayElementInfo(arrayName, &isCharArray);
            
            if (isConstantValue(indexStr)) {
                // Constant index: add offset directly
                int index = atoi(indexStr);
                int offset = index * elementSize;
                if (isCharArray) {
                    sprintf(instr, "    lb %s, %d(%s)", getRegisterName(resultReg), offset, getRegisterName(ptrReg));
                } else {
                    sprintf(instr, "    lw %s, %d(%s)", getRegisterName(resultReg), offset, getRegisterName(ptrReg));
                }
                emitMIPS(codegen, instr);
            } else {
                // Variable index: calculate offset
                int indexReg = getReg(codegen, indexStr, irIndex);
                if (elementSize == 4) {
                    sprintf(instr, "    sll $t8, %s, 2", getRegisterName(indexReg));
                } else if (elementSize == 1) {
                    sprintf(instr, "    move $t8, %s", getRegisterName(indexReg));
                } else {
                    sprintf(instr, "    li $t9, %d", elementSize);
                    emitMIPS(codegen, instr);
                    sprintf(instr, "    mul $t8, %s, $t9", getRegisterName(indexReg));
                }
                emitMIPS(codegen, instr);
                
                sprintf(instr, "    add $t8, $t8, %s", getRegisterName(ptrReg));
                emitMIPS(codegen, instr);
                
                if (isCharArray) {
                    sprintf(instr, "    lb %s, 0($t8)", getRegisterName(resultReg));
                } else {
                    sprintf(instr, "    lw %s, 0($t8)", getRegisterName(resultReg));
                }
                emitMIPS(codegen, instr);
            }
            
            // Update descriptors and store
            updateDescriptors(codegen, resultReg, quad->result);
            codegen->regDescriptors[resultReg].isDirty = false;
            storeVariable(codegen, quad->result, resultReg);
            int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
            if (addrIdx >= 0) {
                codegen->addrDescriptors[addrIdx].inMemory = true;
            }
            return;
        }
        
        // Array access (not pointer)
        // Get array base offset
        char arrayLocation[128];
        getMemoryLocation(codegen, arrayName, arrayLocation);
        int baseOffset = 0;
        if (strstr(arrayLocation, "($fp)") != NULL) {
            sscanf(arrayLocation, "%d($fp)", &baseOffset);
        }
        
        // Check element size
        bool isCharArray = false;
        int elementSize = getArrayElementInfo(arrayName, &isCharArray);
        
        // Get register for result
        int resultReg = getReg(codegen, quad->result, irIndex);
        
        // Generate code to load array element
        if (isConstantValue(indexStr)) {
            // Constant index
            int index = atoi(indexStr);
            int totalOffset = baseOffset + (index * elementSize);
            if (isCharArray) {
                sprintf(instr, "    lb %s, %d($fp)", getRegisterName(resultReg), totalOffset);
            } else {
                sprintf(instr, "    lw %s, %d($fp)", getRegisterName(resultReg), totalOffset);
            }
            emitMIPS(codegen, instr);
        } else {
            // Variable index
            int indexReg = getReg(codegen, indexStr, irIndex);
            
            // Calculate offset
            if (elementSize == 4) {
                sprintf(instr, "    sll $t8, %s, 2", getRegisterName(indexReg));
            } else if (elementSize == 1) {
                sprintf(instr, "    move $t8, %s", getRegisterName(indexReg));
            } else {
                sprintf(instr, "    li $t9, %d", elementSize);
                emitMIPS(codegen, instr);
                sprintf(instr, "    mul $t8, %s, $t9", getRegisterName(indexReg));
            }
            emitMIPS(codegen, instr);
            
            sprintf(instr, "    addi $t8, $t8, %d", baseOffset);
            emitMIPS(codegen, instr);
            
            sprintf(instr, "    add $t8, $t8, $fp");
            emitMIPS(codegen, instr);
            
            if (isCharArray) {
                sprintf(instr, "    lb %s, 0($t8)", getRegisterName(resultReg));
            } else {
                sprintf(instr, "    lw %s, 0($t8)", getRegisterName(resultReg));
            }
            emitMIPS(codegen, instr);
        }
        
        // Update descriptors
        updateDescriptors(codegen, resultReg, quad->result);
        codegen->regDescriptors[resultReg].isDirty = false;
        
        // Store to memory immediately for reloadability
        storeVariable(codegen, quad->result, resultReg);
        int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
        }
        return;
    }
    
    // CHECK: If arg1 starts with '&', this is an address-of operation
    // Example: t20 = &input_int
    // CRITICAL: We compute the ADDRESS and store it in the RESULT temporary
    // We do NOT update the address descriptor for the variable being addressed
    if (quad->arg1[0] == '&') {
        const char* varName = quad->arg1 + 1;  // Skip the '&'
        const char* resultVar = quad->result;
        
        // Get variable's memory location
        char location[128];
        getMemoryLocation(codegen, varName, location);
        
        // Parse offset value (e.g., "-12" from "-12($fp)")
        int offset = 0;
        if (strstr(location, "($fp)") != NULL) {
            sscanf(location, "%d($fp)", &offset);
            
            // Calculate address (add offset to $fp)
            int resultReg = getReg(codegen, resultVar, irIndex);
            sprintf(instr, "    addiu %s, $fp, %d    # %s = &%s (address calculation)", 
                    getRegisterName(resultReg), offset, resultVar, varName);
            emitMIPS(codegen, instr);
            
            // CRITICAL FIX: Only update descriptor for RESULT, not for varName
            // The result temp holds an ADDRESS, not the variable's VALUE
            updateDescriptors(codegen, resultReg, resultVar);
            codegen->regDescriptors[resultReg].isDirty = true;
            
            // Store to memory for reloadability
            storeVariable(codegen, resultVar, resultReg);
            int addrIdx2 = findOrCreateAddrDesc(codegen, resultVar);
            if (addrIdx2 >= 0) {
                codegen->addrDescriptors[addrIdx2].inMemory = true;
            }
        } else {
            // Global variable - load address
            int resultReg = getReg(codegen, resultVar, irIndex);
            sprintf(instr, "    la %s, %s    # %s = &%s (global address)", 
                    getRegisterName(resultReg), varName, resultVar, varName);
            emitMIPS(codegen, instr);
            
            // CRITICAL FIX: Only update descriptor for RESULT, not for varName
            updateDescriptors(codegen, resultReg, resultVar);
            codegen->regDescriptors[resultReg].isDirty = true;
            
            // Store to memory for reloadability
            storeVariable(codegen, resultVar, resultReg);
            int addrIdx2 = findOrCreateAddrDesc(codegen, resultVar);
            if (addrIdx2 >= 0) {
                codegen->addrDescriptors[addrIdx2].inMemory = true;
            }
        }
        return;
    }
    
    // CRITICAL FIX: Detect pointer-to-array assignment using activation record info
    // In C, when you assign an array name to a pointer (ptr = array), you're assigning the ADDRESS
    // We detect this by checking sizes in activation record:
    // - Arrays have large size (> 4 bytes)
    // - Pointers/scalars have small size (<= 8 bytes)
    // When assigning large to small, it's likely a pointer-to-array assignment
    
    if (codegen->currentFunction != NULL && !isConstantValue(quad->arg1) && quad->arg1[0] != 't') {
        ActivationRecord* record = codegen->currentFunction;
        
        // Find source and destination in activation record
        int srcIdx = -1, destIdx = -1;
        for (int i = 0; i < record->varCount; i++) {
            if (strcmp(record->variables[i].varName, quad->arg1) == 0) {
                srcIdx = i;
            }
            if (strcmp(record->variables[i].varName, quad->result) == 0) {
                destIdx = i;
            }
        }
        
        // If source has size > 8 and destination has size <= 8, it's likely ptr = array
        // We use > 8 to avoid treating pointers (size 4-8) as arrays
        if (srcIdx >= 0 && destIdx >= 0) {
            int srcSize = record->variables[srcIdx].size;
            int destSize = record->variables[destIdx].size;
            
            if (srcSize > 8 && destSize <= 8 && srcSize > destSize) {
                // This is pointer-to-array assignment - compute address
                int offset = record->variables[srcIdx].offset;
                int regDest = getReg(codegen, quad->result, irIndex);
                
                sprintf(instr, "    addiu %s, $fp, %d    # %s = &%s (array address)", 
                        getRegisterName(regDest), offset, quad->result, quad->arg1);
                emitMIPS(codegen, instr);
                
                // Store to destination
                storeVariable(codegen, quad->result, regDest);
                
                // Update descriptors
                updateDescriptors(codegen, regDest, quad->result);
                codegen->regDescriptors[regDest].isDirty = false;
                
                // Store to memory for reloadability
                storeVariable(codegen, quad->result, regDest);
                int addrIdx3 = findOrCreateAddrDesc(codegen, quad->result);
                if (addrIdx3 >= 0) {
                    codegen->addrDescriptors[addrIdx3].inMemory = true;
                }
                return;
            }
        }
    }
    

    
    // FIX: Handle constant assignments directly (a = 10)
    if (isConstantValue(quad->arg1)) {
        int regSrc;
        
        // CRITICAL FIX: String literals need special handling - load address from data section
        if (quad->arg1[0] == '"') {
            // Find the string in data section
            int stringIndex = -1;
            for (int i = 0; i < codegen->stringCount; i++) {
                if (strcmp(codegen->stringLiterals[i], quad->arg1) == 0) {
                    stringIndex = i;
                    break;
                }
            }
            
            if (stringIndex >= 0) {
                // Get a register for the result
                regSrc = getReg(codegen, quad->result, irIndex);
                // Load address of string from data section
                sprintf(instr, "    la %s, _str%d    # %s = %s", 
                        getRegisterName(regSrc), stringIndex, quad->result, quad->arg1);
                emitMIPS(codegen, instr);
            } else {
                // String not found - load null
                regSrc = getReg(codegen, quad->result, irIndex);
                sprintf(instr, "    li %s, 0    # String not found: %s", 
                        getRegisterName(regSrc), quad->arg1);
                emitMIPS(codegen, instr);
            }
        } else {
            // Non-string constant - load normally
            regSrc = getReg(codegen, quad->arg1, irIndex);
        }
        
        // Store to result variable's memory location
        storeVariable(codegen, quad->result, regSrc);
        
        // Update address descriptor - result is now in memory and in register
        int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
            codegen->addrDescriptors[addrIdx].inRegister = regSrc;
        }
        
        // Update register descriptor
        updateDescriptors(codegen, regSrc, quad->result);
        return;
    }
    
    // Check if arg1 is a function name (for function pointer assignment: func = functionName)
    extern Symbol* lookupSymbol(const char* name);
    Symbol* funcSym = lookupSymbol(quad->arg1);
    if (funcSym && funcSym->is_function) {
        // Load address of function label into result
        int regDest = getReg(codegen, quad->result, irIndex);
        char funcLabel[128];
        // Add underscore prefix to avoid conflicts (except for main)
        if (strcmp(quad->arg1, "main") == 0) {
            sprintf(funcLabel, "_main");
        } else {
            sprintf(funcLabel, "_%s", quad->arg1);
        }
        sprintf(instr, "    la %s, %s    # %s = &%s", 
                getRegisterName(regDest), funcLabel, quad->result, quad->arg1);
        emitMIPS(codegen, instr);
        
        // Store and update descriptors
        storeVariable(codegen, quad->result, regDest);
        int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
            codegen->addrDescriptors[addrIdx].inRegister = regDest;
        }
        updateDescriptors(codegen, regDest, quad->result);
        return;
    }
    
    // Variable assignment: result = variable
    // Get register for arg1 (source)
    int regSrc = getReg(codegen, quad->arg1, irIndex);
    
    // Get register for result (destination)  
    int regDest = getReg(codegen, quad->result, irIndex);
    
    // If different registers, move
    if (regSrc != regDest) {
        sprintf(instr, "    move %s, %s", 
               getRegisterName(regDest), 
               getRegisterName(regSrc));
        emitMIPS(codegen, instr);
    }
    
    // CRITICAL FIX: If result is a global/static variable, store to memory immediately
    // This ensures the value persists across function calls
    // ALSO store ALL variables to memory for correctness (temporaries need to be reloadable)
    storeVariable(codegen, quad->result, regDest);
    
    // Update address descriptor to reflect it's in memory
    int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
    if (addrIdx >= 0) {
        codegen->addrDescriptors[addrIdx].inMemory = true;
    }
    
    // Update descriptors
    updateDescriptors(codegen, regDest, quad->result);
    codegen->regDescriptors[regDest].isDirty = false;  // Not dirty since we just stored it
    
    // Register variable type if available in quad
    if (quad->resultType[0] != '\0') {
        registerVariableType(codegen, quad->result, quad->resultType);
    }
    // Also infer type from constant assignment (e.g., x = 10.0f means x is float)
    else if (isFloatConstant(quad->arg1)) {
        registerVariableType(codegen, quad->result, "float");
    }
    // Propagate type from source variable (e.g., t1 = t0 means t1 has same type as t0)
    else if (!isConstantValue(quad->arg1) && quad->arg1[0] != '&' && quad->arg1[0] != '"') {
        const char* srcType = getVariableType(codegen, quad->arg1);
        if (strcmp(srcType, "float") == 0 || strcmp(srcType, "double") == 0) {
            registerVariableType(codegen, quad->result, srcType);
        }
    }
}

/**
 * Sanitize label name for MIPS compatibility
 * Replace invalid characters like single quotes with underscores
 */
void sanitizeLabelName(const char* input, char* output) {
    int j = 0;
    for (int i = 0; input[i] != '\0' && i < 127; i++) {
        char c = input[i];
        // Replace single quotes, spaces, and other special chars with underscore
        // For characters in quotes like 'a' or '-', convert to ASCII value
        if (c == '\'') {
            // Skip quote, get the character inside, convert to ASCII
            if (input[i+1] != '\'' && input[i+1] != '\0') {
                i++; // Move to char inside quote
                // Convert character to ASCII decimal representation
                int ascii = (int)(unsigned char)input[i];
                char asciiStr[10];
                sprintf(asciiStr, "%d", ascii);
                for (int k = 0; asciiStr[k] != '\0'; k++) {
                    output[j++] = asciiStr[k];
                }
                // Skip closing quote if present
                if (input[i+1] == '\'') {
                    i++;
                }
            } else {
                output[j++] = '_';
            }
        } else if (c == ' ' || c == '"' || c == '.' || c == ',' || c == '-' || c == '+' || c == '*' || c == '/') {
            output[j++] = '_';
        } else {
            output[j++] = c;
        }
    }
    output[j] = '\0';
}

/**
 * Translate label
 * CRITICAL: At label boundaries (basic block entry points), we must spill
 * all dirty registers and clear register descriptors because control flow
 * can reach here from multiple paths. Variables in registers may be stale.
 * 
 * skipSpill: If true, skip register spilling (used when previous instruction was a return)
 */
void translateLabel(MIPSCodeGenerator* codegen, Quadruple* quad, bool skipSpill) {
    char label[128];
    char sanitized[128];
    
    // Labels can be stored in different formats:
    // 1. op="LABEL", arg1="L0" (standard format from IR file)
    // 2. op="L0", result="..." (alternative format)
    const char* labelName = NULL;
    
    if (strcmp(quad->op, "LABEL") == 0 && strlen(quad->arg1) > 0) {
        labelName = quad->arg1;
    } else if (quad->op[0] == 'L' && isdigit(quad->op[1])) {
        labelName = quad->op;
    } else if (strlen(quad->result) > 0) {
        labelName = quad->result;
    }
    
    if (labelName != NULL) {
        // CRITICAL FIX: Spill all registers before label to save any pending changes
        // UNLESS previous instruction was a return (code is unreachable)
        if (!skipSpill) {
            for (int r = REG_T0; r <= REG_T9; r++) {
                if (codegen->regDescriptors[r].varCount > 0) {
                    spillRegister(codegen, r);
                }
            }
        }
        
        // CRITICAL: Also invalidate ALL address descriptors after label
        // Variables may be loaded from multiple paths, so we can't trust register contents
        for (int i = 0; i < codegen->addrDescCount; i++) {
            codegen->addrDescriptors[i].inRegister = -1;
        }
        
        // CRITICAL FIX: Also clear ALL register descriptors after label
        // Since we can reach this label from multiple paths, register contents are unreliable
        // Force all variables to be reloaded from memory when needed
        for (int r = REG_T0; r <= REG_T9; r++) {
            clearRegisterDescriptor(codegen, r);
        }
        
        sanitizeLabelName(labelName, sanitized);
        sprintf(label, "%s:", sanitized);
        emitMIPS(codegen, label);
    }
}

/**
 * Translate unconditional jump
 * CRITICAL: Must spill all registers before jumping because control flow changes
 */
void translateGoto(MIPSCodeGenerator* codegen, Quadruple* quad) {
    char instr[128];
    char sanitized[128];
    
    // CRITICAL FIX: Spill only DIRTY registers before jumping
    // Clean registers are already saved and don't need re-spilling
    // This prevents exceeding frame size with unnecessary spills
    for (int r = REG_T0; r <= REG_T9; r++) {
        if (codegen->regDescriptors[r].varCount > 0 && codegen->regDescriptors[r].isDirty) {
            spillRegister(codegen, r);
        }
    }
    
    // CRITICAL: Also invalidate ALL address descriptors before goto
    // The target label may be reached from multiple paths, so register contents can't be trusted
    for (int i = 0; i < codegen->addrDescCount; i++) {
        codegen->addrDescriptors[i].inRegister = -1;
    }
    
    // The label can be in result OR arg1 depending on IR format
    // Labels can start with 'L' (L0, L1) or be named (skip_section, loop_end, etc.)
    const char* label = NULL;
    
    if (strlen(quad->result) > 0) {
        label = quad->result;
    } else if (strlen(quad->arg1) > 0) {
        label = quad->arg1;
    } else if (strlen(quad->arg2) > 0) {
        label = quad->arg2;
    }
    
    if (label != NULL && strlen(label) > 0) {
        sanitizeLabelName(label, sanitized);
        sprintf(instr, "    j %s", sanitized);
        emitMIPS(codegen, instr);
    } else {
        // This should rarely happen - debug info
        fprintf(stderr, "Warning: GOTO instruction with no label found. Op='%s', Arg1='%s', Arg2='%s', Result='%s'\n",
                quad->op, quad->arg1, quad->arg2, quad->result);
    }
}

/**
 * Translate conditional branch
 * CRITICAL: Must spill all registers before branching because control flow changes
 */
void translateConditionalBranch(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    char sanitized[128];
    
    // Get register for condition variable
    int regCond = getReg(codegen, quad->arg1, irIndex);
    
    // CRITICAL FIX: Spill only DIRTY registers before branching
    // Clean registers are already saved and don't need re-spilling
    // This prevents exceeding frame size with unnecessary spills
    for (int r = REG_T0; r <= REG_T9; r++) {
        if (codegen->regDescriptors[r].varCount > 0 && codegen->regDescriptors[r].isDirty && r != regCond) {
            spillRegister(codegen, r);
        }
    }
    
    // The target label is in arg2 for conditional branches
    const char* targetLabel = quad->arg2;
    
    // Sanitize label name (remove quotes, etc.)
    sanitizeLabelName(targetLabel, sanitized);
    
    // Check the opcode to determine branch type
    if (strcmp(quad->op, "IF_TRUE_GOTO") == 0) {
        // Branch if condition is true (not zero)
        sprintf(instr, "    bnez %s, %s", getRegisterName(regCond), sanitized);
    } 
    else if (strcmp(quad->op, "IF_FALSE_GOTO") == 0) {
        // Branch if condition is false (zero)
        sprintf(instr, "    beqz %s, %s", getRegisterName(regCond), sanitized);
    }
    else if (strcmp(quad->op, "IF_TRUE_GOTO_FLOAT") == 0) {
        // Branch if float condition is true (not zero)
        // For now, treat same as integer (proper float handling would use FPU)
        sprintf(instr, "    bnez %s, %s", getRegisterName(regCond), sanitized);
    }
    else if (strcmp(quad->op, "IF_FALSE_GOTO_FLOAT") == 0) {
        // Branch if float condition is false (zero)
        sprintf(instr, "    beqz %s, %s", getRegisterName(regCond), sanitized);
    }
    else {
        // Fallback - shouldn't happen but handle gracefully
        fprintf(stderr, "Warning: Unknown conditional branch op: %s\n", quad->op);
        sprintf(instr, "    beqz %s, %s", getRegisterName(regCond), sanitized);
    }
    
    emitMIPS(codegen, instr);
}

/**
 * Translate relational operation (LT, GT, EQ, etc.)
 */
void translateRelational(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    int regArg1 = getReg(codegen, quad->arg1, irIndex);
    int regArg2 = getReg(codegen, quad->arg2, irIndex);
    int regResult = getReg(codegen, quad->result, irIndex);
    
    if (strcmp(quad->op, "LT") == 0 || strcmp(quad->op, "<") == 0) {
        sprintf(instr, "    slt %s, %s, %s", 
               getRegisterName(regResult),
               getRegisterName(regArg1),
               getRegisterName(regArg2));
    } else if (strcmp(quad->op, "GT") == 0 || strcmp(quad->op, ">") == 0) {
        sprintf(instr, "    sgt %s, %s, %s", 
               getRegisterName(regResult),
               getRegisterName(regArg1),
               getRegisterName(regArg2));
    } else if (strcmp(quad->op, "LE") == 0 || strcmp(quad->op, "<=") == 0) {
        sprintf(instr, "    sle %s, %s, %s", 
               getRegisterName(regResult),
               getRegisterName(regArg1),
               getRegisterName(regArg2));
    } else if (strcmp(quad->op, "GE") == 0 || strcmp(quad->op, ">=") == 0) {
        sprintf(instr, "    sge %s, %s, %s", 
               getRegisterName(regResult),
               getRegisterName(regArg1),
               getRegisterName(regArg2));
    } else if (strcmp(quad->op, "EQ") == 0 || strcmp(quad->op, "==") == 0) {
        sprintf(instr, "    seq %s, %s, %s", 
               getRegisterName(regResult),
               getRegisterName(regArg1),
               getRegisterName(regArg2));
    } else if (strcmp(quad->op, "NE") == 0 || strcmp(quad->op, "!=") == 0) {
        sprintf(instr, "    sne %s, %s, %s", 
               getRegisterName(regResult),
               getRegisterName(regArg1),
               getRegisterName(regArg2));
    } else {
        return;  // Unknown relational op
    }
    
    emitMIPS(codegen, instr);
    updateDescriptors(codegen, regResult, quad->result);
    codegen->regDescriptors[regResult].isDirty = true;
}

/**
 * Translate logical operation (AND, OR, NOT)
 */
void translateLogical(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    if (strcmp(quad->op, "NOT") == 0 || strcmp(quad->op, "!") == 0) {
        // Unary NOT
        int regArg1 = getReg(codegen, quad->arg1, irIndex);
        int regResult = getReg(codegen, quad->result, irIndex);
        
        // Logical NOT: if arg1 == 0 then result = 1, else result = 0
        sprintf(instr, "    seq %s, %s, $zero", 
               getRegisterName(regResult),
               getRegisterName(regArg1));
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, regResult, quad->result);
    } else {
        // Binary logical operations
        int regArg1 = getReg(codegen, quad->arg1, irIndex);
        int regArg2 = getReg(codegen, quad->arg2, irIndex);
        int regResult = getReg(codegen, quad->result, irIndex);
        
        if (strcmp(quad->op, "AND") == 0 || strcmp(quad->op, "&&") == 0) {
            sprintf(instr, "    and %s, %s, %s", 
                   getRegisterName(regResult),
                   getRegisterName(regArg1),
                   getRegisterName(regArg2));
        } else if (strcmp(quad->op, "OR") == 0 || strcmp(quad->op, "||") == 0) {
            sprintf(instr, "    or %s, %s, %s", 
                   getRegisterName(regResult),
                   getRegisterName(regArg1),
                   getRegisterName(regArg2));
        } else {
            return;
        }
        
        emitMIPS(codegen, instr);
        updateDescriptors(codegen, regResult, quad->result);
    }
    codegen->regDescriptors[getReg(codegen, quad->result, irIndex)].isDirty = true;
}

/**
 * Translate bitwise operation (BITAND, BITOR, BITXOR, BITNOT, LSHIFT, RSHIFT)
 */
void translateBitwise(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    if (strcmp(quad->op, "BITNOT") == 0 || strcmp(quad->op, "~") == 0) {
        // Unary BITNOT: ~a = NOR a, $zero
        int regArg1 = getReg(codegen, quad->arg1, irIndex);
        int regResult = getReg(codegen, quad->result, irIndex);
        
        sprintf(instr, "    nor %s, %s, $zero", 
               getRegisterName(regResult),
               getRegisterName(regArg1));
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, regResult, quad->result);
    } else {
        // Binary bitwise operations
        int regArg1 = getReg(codegen, quad->arg1, irIndex);
        int regArg2 = getReg(codegen, quad->arg2, irIndex);
        int regResult = getReg(codegen, quad->result, irIndex);
        
        if (strcmp(quad->op, "BITAND") == 0 || strcmp(quad->op, "&") == 0) {
            sprintf(instr, "    and %s, %s, %s", 
                   getRegisterName(regResult),
                   getRegisterName(regArg1),
                   getRegisterName(regArg2));
        } else if (strcmp(quad->op, "BITOR") == 0 || strcmp(quad->op, "|") == 0) {
            sprintf(instr, "    or %s, %s, %s", 
                   getRegisterName(regResult),
                   getRegisterName(regArg1),
                   getRegisterName(regArg2));
        } else if (strcmp(quad->op, "BITXOR") == 0 || strcmp(quad->op, "^") == 0) {
            sprintf(instr, "    xor %s, %s, %s", 
                   getRegisterName(regResult),
                   getRegisterName(regArg1),
                   getRegisterName(regArg2));
        } else if (strcmp(quad->op, "LSHIFT") == 0 || strcmp(quad->op, "<<") == 0) {
            // Check if shift amount is constant
            if (isConstantValue(quad->arg2)) {
                int shiftAmount = atoi(quad->arg2);
                sprintf(instr, "    sll %s, %s, %d", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       shiftAmount);
            } else {
                sprintf(instr, "    sllv %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            }
        } else if (strcmp(quad->op, "RSHIFT") == 0 || strcmp(quad->op, ">>") == 0) {
            // Check if shift amount is constant
            if (isConstantValue(quad->arg2)) {
                int shiftAmount = atoi(quad->arg2);
                // Use sra (arithmetic shift) to preserve sign bit
                sprintf(instr, "    sra %s, %s, %d", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       shiftAmount);
            } else {
                // Use srav (arithmetic shift variable) to preserve sign bit
                sprintf(instr, "    srav %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            }
        } else {
            return;
        }
        
        emitMIPS(codegen, instr);
        updateDescriptors(codegen, regResult, quad->result);
    }
    codegen->regDescriptors[getReg(codegen, quad->result, irIndex)].isDirty = true;
}

// ============================================================================
// PHASE 3: Advanced Features - Function Calls, Arrays, Pointers, I/O
// ============================================================================

// ----------------------------------------------------------------------------
// Task 3.1: Function Call Support
// ----------------------------------------------------------------------------

/**
 * Translate PARAM instruction
 * First 4 params go to $a0-$a3, rest are pushed to stack
 */
void translateParam(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    // arg1 contains the parameter value
    const char* paramValue = quad->arg1;
    
    // Check if next instruction is a CALL to printf/scanf
    // If so, skip stack operations entirely - printf/scanf handler manages its own stack
    bool isIOCall = false;
    for (int i = irIndex + 1; i < codegen->irCount && i < irIndex + 20; i++) {
        if (strcmp(codegen->IR[i].op, "CALL") == 0 || strcmp(codegen->IR[i].op, "call") == 0) {
            const char* funcName = codegen->IR[i].arg1;
            if (strcmp(funcName, "printf") == 0 || strcmp(funcName, "scanf") == 0) {
                isIOCall = true;
            }
            break;
        }
    }
    
    // CRITICAL FIX: For the FIRST parameter (index 0), spill all caller-saved registers
    // This must happen BEFORE any parameters are set up to avoid overwriting memory
    // that parameters reference (e.g., &a where 'a' is at -12($fp))
    // HOWEVER: Don't spill registers that hold parameter VALUES that are about to be passed!
    if (codegen->currentParamCount == 0 && !isIOCall) {
        // Build a set of parameter variable names for this call
        // Look ahead to find all PARAM instructions before the next CALL
        char paramVars[10][128];  // Up to 10 parameters
        int paramVarCount = 0;
        
        for (int i = irIndex; i < codegen->irCount && i < irIndex + 20; i++) {
            if (strcmp(codegen->IR[i].op, "PARAM") == 0 || strcmp(codegen->IR[i].op, "param") == 0) {
                if (paramVarCount < 10) {
                    strncpy(paramVars[paramVarCount], codegen->IR[i].arg1, 127);
                    paramVars[paramVarCount][127] = '\0';
                    paramVarCount++;
                }
            } else if (strcmp(codegen->IR[i].op, "CALL") == 0 || strcmp(codegen->IR[i].op, "call") == 0) {
                break;  // Stop at CALL
            }
        }
        
        // Spill all dirty caller-saved registers EXCEPT those holding parameter values
        for (int r = REG_T0; r <= REG_T9; r++) {
            if (codegen->regDescriptors[r].varCount > 0 && codegen->regDescriptors[r].isDirty) {
                // Check if this register holds any parameter variable
                bool holdsParam = false;
                for (int v = 0; v < codegen->regDescriptors[r].varCount; v++) {
                    const char* varInReg = codegen->regDescriptors[r].varNames[v];
                    for (int p = 0; p < paramVarCount; p++) {
                        if (strcmp(varInReg, paramVars[p]) == 0) {
                            holdsParam = true;
                            break;
                        }
                    }
                    if (holdsParam) break;
                }
                
                // Only spill if this register doesn't hold a parameter value
                if (!holdsParam) {
                    spillRegister(codegen, r);
                }
            }
        }
    }
    
    // Increment parameter counter
    int paramIndex = codegen->currentParamCount++;
    
    if (paramIndex < 4) {
        // First 4 parameters go to $a0-$a3
        int argReg = REG_A0 + paramIndex;
        
        // Check if parameter is a string literal
        if (paramValue[0] == '"') {
            // String literal - find its label in data section
            int stringIndex = -1;
            for (int i = 0; i < codegen->stringCount; i++) {
                if (strcmp(codegen->stringLiterals[i], paramValue) == 0) {
                    stringIndex = i;
                    break;
                }
            }
            
            if (stringIndex >= 0) {
                // Generate load address with correct label
                sprintf(instr, "    la %s, _str%d", getRegisterName(argReg), stringIndex);
                emitMIPS(codegen, instr);
            } else {
                // Fallback - shouldn't happen if data section is generated first
                fprintf(stderr, "Warning: String literal not found in data section: %s\n", paramValue);
                sprintf(instr, "    li %s, 0", getRegisterName(argReg));
                emitMIPS(codegen, instr);
            }
        } else if (isConstantValue(paramValue)) {
            // Numeric constant - use loadVariable which handles floats properly
            loadVariable(codegen, paramValue, argReg);
        } else {
            // Variable - for I/O calls, load directly from memory to avoid register allocation issues
            if (isIOCall) {
                char location[128];
                getMemoryLocation(codegen, paramValue, location);
                sprintf(instr, "    lw %s, %s    # Load %s for I/O call", 
                       getRegisterName(argReg), location, paramValue);
                emitMIPS(codegen, instr);
            } else {
                // Get register for variable
                int srcReg = getReg(codegen, paramValue, irIndex);
                
                // Move to argument register
                sprintf(instr, "    move %s, %s", 
                       getRegisterName(argReg), 
                       getRegisterName(srcReg));
                emitMIPS(codegen, instr);
            }
        }
        
        // Track which register holds this param
        codegen->paramRegisterMap[paramIndex] = argReg;
    } else if (!isIOCall) {
        // Parameters 5+ go on the stack (but NOT for printf/scanf - they handle their own stack)
        int srcReg;
        
        if (isConstantValue(paramValue)) {
            // Load constant to temporary register
            srcReg = getReg(codegen, paramValue, irIndex);
        } else {
            srcReg = getReg(codegen, paramValue, irIndex);
        }
        
        // Push to stack
        sprintf(instr, "    sw %s, 0($sp)", getRegisterName(srcReg));
        emitMIPS(codegen, instr);
        sprintf(instr, "    addiu $sp, $sp, -4");
        emitMIPS(codegen, instr);
    }
    // If isIOCall and paramIndex >= 4, we skip pushing to stack - printf/scanf will handle it
}

/**
 * Translate CALL instruction
 * Format: result = call func_name, param_count
 */
void translateCall(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    // arg1 contains function name
    const char* funcName = quad->arg1;
    
    // arg2 contains parameter count
    int paramCount = codegen->currentParamCount;
    
    // Check if this is a standard library I/O function
    if (strcmp(funcName, "printf") == 0) {
        // FULL PRINTF IMPLEMENTATION WITH FORMAT STRING PARSING
        // Parse the format string and generate syscalls for each segment
        
        // Get the format string from the first parameter  
        // Look backward in IR for the PARAM instruction with string literal
        char formatStr[512] = "";
        bool foundFormat = false;
        
        // Search backward from current instruction for the first PARAM with a string literal
        for (int i = irIndex - 1; i >= 0 && i >= irIndex - 20; i--) {
            if ((strcmp(codegen->IR[i].op, "PARAM") == 0 || strcmp(codegen->IR[i].op, "param") == 0) &&
                codegen->IR[i].arg1[0] == '"') {
                // Found string literal parameter
                strncpy(formatStr, codegen->IR[i].arg1, sizeof(formatStr) - 1);
                foundFormat = true;
                break;
            }
        }
        
        if (!foundFormat || formatStr[0] != '"') {
            // Fallback to simple printf if no format string found
            sprintf(instr, "    li $v0, 4    # syscall 4: print_string");
            emitMIPS(codegen, instr);
            emitMIPS(codegen, "    syscall");
        } else {
            // Parse format string and generate a runtime string parser
            // Strategy: Load format string address, iterate through it character by character,
            // print characters until we hit %, then print the corresponding argument
            
            // Save argument registers to stack (we'll need $a0 for syscalls)
            if (paramCount > 1) {
                sprintf(instr, "    addiu $sp, $sp, -16");
                emitMIPS(codegen, instr);
                sprintf(instr, "    sw $a1, 0($sp)");
                emitMIPS(codegen, instr);
                if (paramCount > 2) {
                    sprintf(instr, "    sw $a2, 4($sp)");
                    emitMIPS(codegen, instr);
                }
                if (paramCount > 3) {
                    sprintf(instr, "    sw $a3, 8($sp)");
                    emitMIPS(codegen, instr);
                }
            }
            
            // Simpler approach: Parse format string at COMPILE time and generate inline code
            int argIndex = 1;  // Start with first argument after format string
            char segment[256];
            int segLen = 0;
            
            // Parse the format string (skip opening quote)
            for (int i = 1; formatStr[i] != '\0' && formatStr[i] != '"'; i++) {
                if (formatStr[i] == '%' && formatStr[i+1] != '%' && formatStr[i+1] != '\0' && formatStr[i+1] != '"') {
                    // Found format specifier - print accumulated segment first
                    if (segLen > 0) {
                        segment[segLen] = '\0';
                        // Print each character of the segment
                        for (int j = 0; j < segLen; j++) {
                            char ch = segment[j];
                            if (ch == '\n') {
                                // Print newline
                                sprintf(instr, "    li $a0, 10    # newline");
                                emitMIPS(codegen, instr);
                                sprintf(instr, "    li $v0, 11    # syscall 11: print_char");
                                emitMIPS(codegen, instr);
                                emitMIPS(codegen, "    syscall");
                            } else if (ch == '\t') {
                                sprintf(instr, "    li $a0, 9     # tab");
                                emitMIPS(codegen, instr);
                                sprintf(instr, "    li $v0, 11");
                                emitMIPS(codegen, instr);
                                emitMIPS(codegen, "    syscall");
                            } else if (ch >= 32 && ch <= 126) {
                                // Printable ASCII
                                sprintf(instr, "    li $a0, %d    # '%c'", (int)ch, ch);
                                emitMIPS(codegen, instr);
                                sprintf(instr, "    li $v0, 11    # syscall 11: print_char");
                                emitMIPS(codegen, instr);
                                emitMIPS(codegen, "    syscall");
                            }
                        }
                        segLen = 0;
                    }
                    
                    // Handle the format specifier
                    i++;  // Move past '%'
                    char spec = formatStr[i];
                    
                    // Skip precision specifiers like .1, .2
                    while (spec == '.' || (spec >= '0' && spec <= '9')) {
                        i++;
                        spec = formatStr[i];
                    }
                    
                    // Load the argument into $a0
                    if (argIndex == 1) {
                        sprintf(instr, "    lw $a0, 0($sp)    # Load arg %d", argIndex);
                    } else if (argIndex == 2) {
                        sprintf(instr, "    lw $a0, 4($sp)    # Load arg %d", argIndex);
                    } else if (argIndex == 3) {
                        sprintf(instr, "    lw $a0, 8($sp)    # Load arg %d", argIndex);
                    } else {
                        // Load from original stack position
                        sprintf(instr, "    lw $a0, %d($sp)    # Load arg %d", 16 + (argIndex - 4) * 4, argIndex);
                    }
                    emitMIPS(codegen, instr);
                    
                    // Emit appropriate syscall based on format specifier
                    if (spec == 'd' || spec == 'i') {
                        sprintf(instr, "    li $v0, 1    # syscall 1: print_int");
                    } else if (spec == 'f') {
                        // Float support - move from integer register to float register and print
                        sprintf(instr, "    mtc1 $a0, $f12    # Move to float register");
                        emitMIPS(codegen, instr);
                        sprintf(instr, "    li $v0, 2    # syscall 2: print_float");
                    } else if (spec == 'c') {
                        sprintf(instr, "    li $v0, 11   # syscall 11: print_char");
                    } else if (spec == 's') {
                        sprintf(instr, "    li $v0, 4    # syscall 4: print_string");
                    } else if (spec == 'p' || spec == 'x') {
                        sprintf(instr, "    li $v0, 1    # syscall 1: print_int (pointer as int)");
                    } else {
                        sprintf(instr, "    li $v0, 1    # syscall 1: print_int (default)");
                    }
                    emitMIPS(codegen, instr);
                    emitMIPS(codegen, "    syscall");
                    
                    argIndex++;
                } else if (formatStr[i] == '%' && formatStr[i+1] == '%') {
                    // Escaped percent sign
                    segment[segLen++] = '%';
                    i++;  // Skip next '%'
                } else {
                    // Regular character
                    char ch = formatStr[i];
                    
                    // Handle escape sequences
                    if (ch == '\\' && formatStr[i+1] != '\0' && formatStr[i+1] != '"') {
                        i++;
                        if (formatStr[i] == 'n') {
                            ch = '\n';
                        } else if (formatStr[i] == 't') {
                            ch = '\t';
                        } else if (formatStr[i] == 'r') {
                            ch = '\r';
                        } else if (formatStr[i] == '\\') {
                            ch = '\\';
                        } else if (formatStr[i] == '"') {
                            ch = '"';
                        } else {
                            ch = formatStr[i];
                        }
                    }
                    
                    segment[segLen++] = ch;
                }
            }
            
            // Print any remaining segment
            if (segLen > 0) {
                segment[segLen] = '\0';
                for (int j = 0; j < segLen; j++) {
                    char ch = segment[j];
                    if (ch == '\n') {
                        sprintf(instr, "    li $a0, 10    # newline");
                        emitMIPS(codegen, instr);
                        sprintf(instr, "    li $v0, 11    # syscall 11: print_char");
                        emitMIPS(codegen, instr);
                        emitMIPS(codegen, "    syscall");
                    } else if (ch == '\t') {
                        sprintf(instr, "    li $a0, 9     # tab");
                        emitMIPS(codegen, instr);
                        sprintf(instr, "    li $v0, 11");
                        emitMIPS(codegen, instr);
                        emitMIPS(codegen, "    syscall");
                    } else if (ch >= 32 && ch <= 126) {
                        sprintf(instr, "    li $a0, %d    # '%c'", (int)ch, ch);
                        emitMIPS(codegen, instr);
                        sprintf(instr, "    li $v0, 11    # syscall 11: print_char");
                        emitMIPS(codegen, instr);
                        emitMIPS(codegen, "    syscall");
                    }
                }
            }
            
            // Restore stack (restore the 16 bytes we allocated for saving a1-a3)
            if (paramCount > 1) {
                sprintf(instr, "    addiu $sp, $sp, 16");
                emitMIPS(codegen, instr);
            }
        }
        
        // NOTE: No parameter cleanup needed - printf params are NOT pushed to stack
        // (translateParam skips stack ops for printf/scanf)
        
        // No return value needed for printf
    }
    else if (strcmp(funcName, "scanf") == 0) {
        // FULL SCANF IMPLEMENTATION WITH FORMAT STRING PARSING
        // Get parameter count from quad->arg2 or currentParamCount
        int numParams = paramCount;
        if (quad->arg2 != NULL && strlen(quad->arg2) > 0 && isdigit(quad->arg2[0])) {
            numParams = atoi(quad->arg2);
        }
        
        // Get the format string - it's the FIRST parameter (last one pushed before call)
        // Search back exactly numParams PARAM instructions
        char formatStr[512] = "";
        bool foundFormat = false;
        int paramsFound = 0;
        
        for (int i = irIndex - 1; i >= 0 && i >= irIndex - 30 && paramsFound < numParams; i--) {
            if (strcmp(codegen->IR[i].op, "PARAM") == 0 || strcmp(codegen->IR[i].op, "param") == 0) {
                paramsFound++;
                if (paramsFound == numParams && codegen->IR[i].arg1[0] == '"') {
                    // This is the first param (format string)
                    strncpy(formatStr, codegen->IR[i].arg1, sizeof(formatStr) - 1);
                    foundFormat = true;
                    break;
                }
            }
        }
        
        if (!foundFormat || formatStr[0] != '"') {
            // Fallback to simple int scanf
            sprintf(instr, "    li $v0, 5    # syscall 5: read_int");
            emitMIPS(codegen, instr);
            emitMIPS(codegen, "    syscall");
            sprintf(instr, "    sw $v0, 0($a1)");
            emitMIPS(codegen, instr);
        } else {
            // Parse format string and generate appropriate reads
            // Parameters: first is format string, rest are addresses
            int argIndex = 1;  // Start with first address argument
            
            // Parse format string for specifiers
            for (int i = 1; formatStr[i] != '\0' && formatStr[i] != '"'; i++) {
                if (formatStr[i] == '%' && formatStr[i+1] != '%' && formatStr[i+1] != '\0' && formatStr[i+1] != '"') {
                    i++;  // Move past '%'
                    char spec = formatStr[i];
                    
                    // Skip spaces and width specifiers
                    while (spec == ' ') {
                        i++;
                        spec = formatStr[i];
                    }
                    while (spec >= '0' && spec <= '9') {
                        i++;
                        spec = formatStr[i];
                    }
                    
                    // Load the address of variable from argument
                    // Arguments are in $a1, $a2, $a3, or stack
                    if (argIndex == 1) {
                        sprintf(instr, "    move $t8, $a1    # Load address arg %d", argIndex);
                    } else if (argIndex == 2) {
                        sprintf(instr, "    move $t8, $a2    # Load address arg %d", argIndex);
                    } else if (argIndex == 3) {
                        sprintf(instr, "    move $t8, $a3    # Load address arg %d", argIndex);
                    } else {
                        // Load from stack (params beyond $a0-$a3)
                        // Stack has params in reverse order, adjust offset
                        int stackOffset = (paramCount - 1 - argIndex) * 4;
                        sprintf(instr, "    lw $t8, %d($sp)    # Load address arg %d", stackOffset, argIndex);
                    }
                    emitMIPS(codegen, instr);
                    
                    // Generate appropriate read syscall
                    if (spec == 'd' || spec == 'i') {
                        // Read integer
                        sprintf(instr, "    li $v0, 5    # syscall 5: read_int");
                        emitMIPS(codegen, instr);
                        emitMIPS(codegen, "    syscall");
                        sprintf(instr, "    sw $v0, 0($t8)   # Store int to address");
                        emitMIPS(codegen, instr);
                    } else if (spec == 'f') {
                        // Read float (SPIM syscall 6)
                        sprintf(instr, "    li $v0, 6    # syscall 6: read_float");
                        emitMIPS(codegen, instr);
                        emitMIPS(codegen, "    syscall");
                        sprintf(instr, "    swc1 $f0, 0($t8) # Store float to address");
                        emitMIPS(codegen, instr);
                    } else if (spec == 'c') {
                        // Read character (syscall 12)
                        sprintf(instr, "    li $v0, 12   # syscall 12: read_char");
                        emitMIPS(codegen, instr);
                        emitMIPS(codegen, "    syscall");
                        sprintf(instr, "    sb $v0, 0($t8)   # Store char to address");
                        emitMIPS(codegen, instr);
                    } else if (spec == 's') {
                        // Read string (syscall 8)
                        sprintf(instr, "    move $a0, $t8    # Buffer address");
                        emitMIPS(codegen, instr);
                        sprintf(instr, "    li $a1, 100      # Max length");
                        emitMIPS(codegen, instr);
                        sprintf(instr, "    li $v0, 8    # syscall 8: read_string");
                        emitMIPS(codegen, instr);
                        emitMIPS(codegen, "    syscall");
                    } else {
                        // Default to int
                        sprintf(instr, "    li $v0, 5    # syscall 5: read_int (default)");
                        emitMIPS(codegen, instr);
                        emitMIPS(codegen, "    syscall");
                        sprintf(instr, "    sw $v0, 0($t8)   # Store to address");
                        emitMIPS(codegen, instr);
                    }
                    
                    argIndex++;
                }
            }
            
            // scanf returns number of items successfully read
            sprintf(instr, "    li $v0, %d    # Return number of items read", argIndex - 1);
            emitMIPS(codegen, instr);
        }
        
        // If call has a result, store return value from $v0
        if (strlen(quad->result) > 0 && strcmp(quad->result, "") != 0) {
            int resultReg = getReg(codegen, quad->result, irIndex);
            sprintf(instr, "    move %s, $v0", getRegisterName(resultReg));
            emitMIPS(codegen, instr);
            
            // Update descriptors
            updateDescriptors(codegen, resultReg, quad->result);
            codegen->regDescriptors[resultReg].isDirty = false;
            
            // Store to memory
            storeVariable(codegen, quad->result, resultReg);
            int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
            if (addrIdx >= 0) {
                codegen->addrDescriptors[addrIdx].inMemory = true;
            }
        }
        
        // NOTE: No parameter cleanup needed - scanf params are NOT pushed to stack
        // (translateParam skips stack ops for printf/scanf)
        
        // CRITICAL: scanf writes to memory through pointers, so we must invalidate
        // ALL descriptors to force subsequent loads to read fresh values from memory
        // Clear all registers and invalidate address descriptor register pointers
        for (int r = REG_T0; r <= REG_T9; r++) {
            clearRegisterDescriptor(codegen, r);
        }
        // Ensure all address descriptors show variables are NOT in registers but ARE in memory
        for (int i = 0; i < codegen->addrDescCount; i++) {
            codegen->addrDescriptors[i].inRegister = -1;
            codegen->addrDescriptors[i].inMemory = true;  // Variables modified by scanf are in memory
        }
    }
    else {
        // Regular function call - generate jal
        
        // NOTE: Register spilling is now done in translateParam (before parameter setup)
        // This prevents the spilling from overwriting memory locations that parameters reference
        // (e.g., when passing &a, we don't want to overwrite 'a' after computing its address)
        
        // Add '_' prefix unless calling main (to avoid instruction name conflicts)
        char jalTarget[128];
        if (strcmp(funcName, "main") == 0) {
            sprintf(jalTarget, "%s", funcName);
        } else {
            sprintf(jalTarget, "_%s", funcName);
        }
        sprintf(instr, "    jal %s", jalTarget);
        emitMIPS(codegen, instr);
        
        // If more than 4 params, restore stack pointer
        if (paramCount > 4) {
            int stackBytes = (paramCount - 4) * 4;
            sprintf(instr, "    addiu $sp, $sp, %d", stackBytes);
            emitMIPS(codegen, instr);
        }
        
        // If call has a result, move return value from $v0 to destination
        int resultReg = -1;  // Track which register holds the return value
        if (strlen(quad->result) > 0 && strcmp(quad->result, "") != 0) {
            resultReg = getReg(codegen, quad->result, irIndex);
            sprintf(instr, "    move %s, $v0", getRegisterName(resultReg));
            emitMIPS(codegen, instr);
            
            // Update descriptors
            updateDescriptors(codegen, resultReg, quad->result);
            codegen->regDescriptors[resultReg].isDirty = false;  // Return value already stored
            
            // CRITICAL FIX: Store return value to memory so it can be loaded later
            // Without this, subsequent loads (e.g., for printf arguments) get garbage
            storeVariable(codegen, quad->result, resultReg);
            int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
            if (addrIdx >= 0) {
                codegen->addrDescriptors[addrIdx].inMemory = true;
            }
        }
    }
    
    // CRITICAL FIX: Function calls clobber caller-saved registers ($t0-$t9, $a0-$a3)
    // Invalidate all register descriptors for caller-saved registers so subsequent
    // operations will reload variables from memory instead of trusting stale register values
    // EXCEPTIONS:
    // 1. Don't clear the register that holds the return value
    // 2. Don't clear registers holding temporaries (tN) - they only exist in registers!
    int resultReg = -1;
    if (strlen(quad->result) > 0 && strcmp(quad->result, "") != 0) {
        // Find which register holds the result (check address descriptor)
        for (int i = 0; i < codegen->addrDescCount; i++) {
            if (strcmp(codegen->addrDescriptors[i].varName, quad->result) == 0) {
                resultReg = codegen->addrDescriptors[i].inRegister;
                break;
            }
        }
    }
    
    for (int r = REG_T0; r <= REG_T9; r++) {
        if (r == resultReg) {
            // Don't clear - holds the return value
            continue;
        }
        
        // Check if this register holds a temporary variable (tN)
        // Temporaries only exist in registers, so we must preserve them
        bool holdsTemporary = false;
        if (codegen->regDescriptors[r].varCount > 0) {
            for (int v = 0; v < codegen->regDescriptors[r].varCount; v++) {
                const char* varName = codegen->regDescriptors[r].varNames[v];
                if (varName[0] == 't' && isdigit(varName[1])) {
                    holdsTemporary = true;
                    break;
                }
            }
        }
        
        if (!holdsTemporary) {
            clearRegisterDescriptor(codegen, r);
        }
    }
    for (int r = REG_A0; r <= REG_A3; r++) {
        clearRegisterDescriptor(codegen, r);
    }
    
    // Reset parameter counter for next call
    codegen->currentParamCount = 0;
}

/**
 * Translate RETURN instruction
 * Format: return <value> or return (no value)
 */
void translateReturn(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    // If returning a value, move it to $v0
    if (strlen(quad->arg1) > 0 && strcmp(quad->arg1, "") != 0) {
        const char* retValue = quad->arg1;
        
        if (isConstantValue(retValue)) {
            // Return constant - use loadVariable which handles floats
            loadVariable(codegen, retValue, REG_V0);
        } else {
            // Return variable - get its register
            int srcReg = getReg(codegen, retValue, irIndex);
            sprintf(instr, "    move $v0, %s", getRegisterName(srcReg));
            emitMIPS(codegen, instr);
        }
    }
    
    // Generate function epilogue (restore $ra, $fp, adjust $sp, return)
    if (codegen->currentFunction != NULL) {
        int frameSize = codegen->currentFunction->frameSize;
        
        emitMIPS(codegen, "");
        emitMIPS(codegen, "    # Function epilogue");
        
        // If this is main function, print the return value before returning
        if (strcmp(codegen->currentFunction->funcName, "main") == 0) {
            emitMIPS(codegen, "    # Print return value (for testing)");
            emitMIPS(codegen, "    move $a0, $v0  # save return value");
            emitMIPS(codegen, "    li $v0, 1      # syscall 1: print integer");
            emitMIPS(codegen, "    syscall");
            emitMIPS(codegen, "    li $v0, 11     # syscall 11: print character");
            emitMIPS(codegen, "    li $a0, 10     # newline");
            emitMIPS(codegen, "    syscall");
        }
        
        sprintf(instr, "    lw $ra, %d($sp)", frameSize - 4);
        emitMIPS(codegen, instr);
        sprintf(instr, "    lw $fp, %d($sp)", frameSize - 8);
        emitMIPS(codegen, instr);
        sprintf(instr, "    addiu $sp, $sp, %d", frameSize);
        emitMIPS(codegen, instr);
        emitMIPS(codegen, "    jr $ra");
    }
}

// ----------------------------------------------------------------------------
// Task 3.2: Array Support
// ----------------------------------------------------------------------------

/**
 * Translate ARRAY_ACCESS operation (reading from array)
 * Format: result = ARRAY_ACCESS array, index
 * Calculate: result = *(array_base + index * element_size)
 */
void translateArrayAccess(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* arrayName = quad->arg1;
    const char* indexVar = quad->arg2;
    const char* resultVar = quad->result;
    
    // CRITICAL FIX: Check if this is a pointer or array in CURRENT SCOPE first
    // The global isPointerVariable() can return wrong results when there are
    // variables with the same name in different scopes (e.g., parameter vs local)
    bool isPointerAccess = false;
    bool foundInCurrentScope = false;
    int varOffset = 0;
    
    // First, check in current function's activation record
    if (codegen->currentFunction != NULL) {
        for (int v = 0; v < codegen->currentFunction->varCount; v++) {
            if (strcmp(codegen->currentFunction->variables[v].varName, arrayName) == 0) {
                // Found in current scope - get the offset
                foundInCurrentScope = true;
                varOffset = codegen->currentFunction->variables[v].offset;
                
                // Now find the symbol table entry with matching name AND scope
                // Use the offset to disambiguate between parameter and local
                for (int i = 0; i < symCount; i++) {
                    if (strcmp(symtab[i].name, arrayName) == 0) {
                        // Check if this symbol is in the current function's scope
                        // Parameters have positive/small negative offsets, locals have larger negative
                        // The activation record stores the correct offset for THIS variable
                        // So we use the is_array flag from the first matching symbol in THIS function
                        if (strcmp(symtab[i].function_scope, codegen->currentFunction->funcName) == 0) {
                            isPointerAccess = (symtab[i].ptr_level > 0 && !symtab[i].is_array);
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    
    // If not found in current scope, use global lookup
    if (!foundInCurrentScope) {
        isPointerAccess = isPointerVariable(arrayName);
    }
    
    // Get actual element size from symbol table
    bool isCharArray = false;
    int elementSize = getArrayElementInfo(arrayName, &isCharArray);
    
    // Handle pointer subscripting: ptr[i] where ptr is a pointer variable
    if (isPointerAccess) {
        // Load pointer value from memory
        int ptrReg = getReg(codegen, arrayName, irIndex);
        
        // Get index
        int indexReg;
        if (isConstantValue(indexVar)) {
            indexReg = REG_T8;
            sprintf(instr, "    li %s, %d", getRegisterName(indexReg), atoi(indexVar));
            emitMIPS(codegen, instr);
        } else {
            indexReg = getReg(codegen, indexVar, irIndex);
        }
        
        // Calculate offset = index * element_size
        if (elementSize == 4) {
            sprintf(instr, "    sll $t9, %s, 2", getRegisterName(indexReg));
        } else if (elementSize == 1) {
            sprintf(instr, "    move $t9, %s", getRegisterName(indexReg));
        } else {
            sprintf(instr, "    li $t9, %d", elementSize);
            emitMIPS(codegen, instr);
            sprintf(instr, "    mul $t9, %s, $t9", getRegisterName(indexReg));
        }
        emitMIPS(codegen, instr);
        
        // Add offset to pointer: address = ptr + (index * elementSize)
        sprintf(instr, "    add $t8, %s, $t9", getRegisterName(ptrReg));
        emitMIPS(codegen, instr);
        
        // Load value at address - use lb for char, lw for others
        int resultReg = getReg(codegen, resultVar, irIndex);
        if (isCharArray) {
            sprintf(instr, "    lb %s, 0($t8)", getRegisterName(resultReg));
        } else {
            sprintf(instr, "    lw %s, 0($t8)", getRegisterName(resultReg));
        }
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
        codegen->regDescriptors[resultReg].isDirty = false;
        
        // Store to memory for reloadability
        storeVariable(codegen, resultVar, resultReg);
        int addrIdx = findOrCreateAddrDesc(codegen, resultVar);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
        }
        return;
    }
    
    // Regular array access: arr[i] where arr is an array
    // Get base address of array (from $fp offset)
    char arrayLocation[128];
    getMemoryLocation(codegen, arrayName, arrayLocation);
    
    // Parse offset value (e.g., "-12" from "-12($fp)")
    int baseOffset = 0;
    if (strstr(arrayLocation, "($fp)") != NULL) {
        sscanf(arrayLocation, "%d($fp)", &baseOffset);
    }
    
    if (isConstantValue(indexVar)) {
        // Constant index - calculate offset at compile time
        int index = atoi(indexVar);
        int totalOffset = baseOffset + (index * elementSize);
        
        // Load from calculated address - use lb for char, lw for others
        int resultReg = getReg(codegen, resultVar, irIndex);
        if (isCharArray) {
            sprintf(instr, "    lb %s, %d($fp)", getRegisterName(resultReg), totalOffset);
        } else {
            sprintf(instr, "    lw %s, %d($fp)", getRegisterName(resultReg), totalOffset);
        }
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
        
        // CRITICAL FIX: Store result to memory immediately after loading from array
        // This ensures that if the register is spilled later, the memory location has the correct value
        // Without this, if t43 = intArr[3] loads into $t1, but later $t1 is reused and t43 is reloaded,
        // it would reload from t43's stack slot which was never written to
        storeVariable(codegen, resultVar, resultReg);
        int addrIdx = findAddressDescriptor(codegen, resultVar);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
        }
    } else {
        // Variable index - calculate at runtime
        int indexReg = getReg(codegen, indexVar, irIndex);
        
        // Calculate offset = index * element_size (use $t8 for offset calculation)
        if (elementSize == 4) {
            // Multiply by 4: shift left by 2
            sprintf(instr, "    sll $t8, %s, 2", getRegisterName(indexReg));
        } else if (elementSize == 1) {
            // Multiply by 1: offset = index
            sprintf(instr, "    move $t8, %s", getRegisterName(indexReg));
        } else if (elementSize == 8) {
            // Multiply by 8: shift left by 3
            sprintf(instr, "    sll $t8, %s, 3", getRegisterName(indexReg));
        } else {
            // General case: multiply
            sprintf(instr, "    li $t9, %d", elementSize);
            emitMIPS(codegen, instr);
            sprintf(instr, "    mul $t8, %s, $t9", getRegisterName(indexReg));
        }
        emitMIPS(codegen, instr);
        
        // Add base offset to $t8
        sprintf(instr, "    addi $t8, $t8, %d", baseOffset);
        emitMIPS(codegen, instr);
        
        // Add $fp to get final address
        sprintf(instr, "    add $t8, $t8, $fp");
        emitMIPS(codegen, instr);
        
        // Load value at address - use lb for char, lw for others
        int resultReg = getReg(codegen, resultVar, irIndex);
        if (isCharArray) {
            sprintf(instr, "    lb %s, 0($t8)", getRegisterName(resultReg));
        } else {
            sprintf(instr, "    lw %s, 0($t8)", getRegisterName(resultReg));
        }
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
        
        // CRITICAL FIX: Store result to memory immediately after loading from array
        // This ensures that if the register is spilled later, the memory location has the correct value
        storeVariable(codegen, resultVar, resultReg);
        int addrIdx = findAddressDescriptor(codegen, resultVar);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
        }
    }
}

/**
 * Translate ASSIGN_ARRAY operation (writing to array)
 * Format: ASSIGN_ARRAY index, array, value
 * Calculate: *(array_base + index * element_size) = value
 */
void translateAssignArray(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* indexVar = quad->arg1;
    const char* arrayName = quad->arg2;
    const char* valueVar = quad->result;
    
    // Check if this is a pointer dereference (ptr[i]) or array access (arr[i])
    Symbol* arraySym = lookupSymbol(arrayName);
    bool isPointerAccess = (arraySym != NULL && arraySym->ptr_level > 0 && !arraySym->is_array);
    
    // Get actual element size from symbol table
    bool isCharArray = false;
    int elementSize = getArrayElementInfo(arrayName, &isCharArray);
    
    // Handle pointer subscripting: ptr[i] where ptr is a pointer variable
    if (isPointerAccess) {
        // Load pointer value from memory
        int ptrReg = getReg(codegen, arrayName, irIndex);
        
        // Get index
        int indexReg;
        if (isConstantValue(indexVar)) {
            indexReg = REG_T8;
            sprintf(instr, "    li %s, %d", getRegisterName(indexReg), atoi(indexVar));
            emitMIPS(codegen, instr);
        } else {
            indexReg = getReg(codegen, indexVar, irIndex);
        }
        
        // Calculate offset = index * element_size
        if (elementSize == 4) {
            sprintf(instr, "    sll $t9, %s, 2", getRegisterName(indexReg));
        } else if (elementSize == 1) {
            sprintf(instr, "    move $t9, %s", getRegisterName(indexReg));
        } else {
            sprintf(instr, "    li $t9, %d", elementSize);
            emitMIPS(codegen, instr);
            sprintf(instr, "    mul $t9, %s, $t9", getRegisterName(indexReg));
        }
        emitMIPS(codegen, instr);
        
        // Add offset to pointer: address = ptr + (index * elementSize)
        sprintf(instr, "    add $t9, %s, $t9", getRegisterName(ptrReg));
        emitMIPS(codegen, instr);
        
        // Get value to store
        int valueReg;
        if (isConstantValue(valueVar)) {
            loadVariable(codegen, valueVar, REG_T8);
            valueReg = REG_T8;
        } else {
            valueReg = getReg(codegen, valueVar, irIndex);
        }
        
        // Store value at computed address - use sb for char, sw for others
        if (isCharArray) {
            sprintf(instr, "    sb %s, 0($t9)", getRegisterName(valueReg));
        } else {
            sprintf(instr, "    sw %s, 0($t9)", getRegisterName(valueReg));
        }
        emitMIPS(codegen, instr);
        return;
    }
    
    // Regular array access: arr[i] where arr is an array
    // Get base address of array (from $fp offset)
    char arrayLocation[128];
    getMemoryLocation(codegen, arrayName, arrayLocation);
    
    // Parse offset value
    int baseOffset = 0;
    if (strstr(arrayLocation, "($fp)") != NULL) {
        sscanf(arrayLocation, "%d($fp)", &baseOffset);
    }
    
    if (isConstantValue(indexVar)) {
        // Constant index - calculate offset at compile time
        int index = atoi(indexVar);
        int totalOffset = baseOffset + (index * elementSize);
        
        // Get value to store
        if (isConstantValue(valueVar)) {
            // Constant value - use loadVariable which handles floats
            loadVariable(codegen, valueVar, REG_T9);
            // Use sb for char arrays, sw for others
            if (isCharArray) {
                sprintf(instr, "    sb $t9, %d($fp)", totalOffset);
            } else {
                sprintf(instr, "    sw $t9, %d($fp)", totalOffset);
            }
            emitMIPS(codegen, instr);
        } else {
            // Variable value
            int valueReg = getReg(codegen, valueVar, irIndex);
            // Use sb for char arrays, sw for others
            if (isCharArray) {
                sprintf(instr, "    sb %s, %d($fp)", getRegisterName(valueReg), totalOffset);
            } else {
                sprintf(instr, "    sw %s, %d($fp)", getRegisterName(valueReg), totalOffset);
            }
            emitMIPS(codegen, instr);
        }
    } else {
        // Variable index - calculate at runtime
        int indexReg = getReg(codegen, indexVar, irIndex);
        
        // Calculate offset = index * element_size (use $t8)
        if (elementSize == 4) {
            sprintf(instr, "    sll $t8, %s, 2", getRegisterName(indexReg));
        } else if (elementSize == 1) {
            sprintf(instr, "    move $t8, %s", getRegisterName(indexReg));
        } else if (elementSize == 8) {
            sprintf(instr, "    sll $t8, %s, 3", getRegisterName(indexReg));
        } else {
            sprintf(instr, "    li $t9, %d", elementSize);
            emitMIPS(codegen, instr);
            sprintf(instr, "    mul $t8, %s, $t9", getRegisterName(indexReg));
        }
        emitMIPS(codegen, instr);
        
        // Add base offset to $t8
        sprintf(instr, "    addi $t8, $t8, %d", baseOffset);
        emitMIPS(codegen, instr);
        
        // Add $fp to get final address
        sprintf(instr, "    add $t8, $t8, $fp");
        emitMIPS(codegen, instr);
        
        // Get value to store
        int valueReg;
        if (isConstantValue(valueVar)) {
            // Use loadVariable which handles floats
            loadVariable(codegen, valueVar, REG_T9);
            // Use sb for char arrays, sw for others
            if (isCharArray) {
                sprintf(instr, "    sb $t9, 0($t8)");
            } else {
                sprintf(instr, "    sw $t9, 0($t8)");
            }
            emitMIPS(codegen, instr);
        } else {
            valueReg = getReg(codegen, valueVar, irIndex);
            // Use sb for char arrays, sw for others
            if (isCharArray) {
                sprintf(instr, "    sb %s, 0($t8)", getRegisterName(valueReg));
            } else {
                sprintf(instr, "    sw %s, 0($t8)", getRegisterName(valueReg));
            }
            emitMIPS(codegen, instr);
        }
    }
}

// ----------------------------------------------------------------------------
// Task 3.3: Pointer Operations
// ----------------------------------------------------------------------------

/**
 * Translate ADDR operation (address-of &)
 * Format: result = &variable
 */
void translateAddr(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* varName = quad->arg1;
    const char* resultVar = quad->result;
    
    // Get variable's memory location
    char location[128];
    getMemoryLocation(codegen, varName, location);
    
    // Parse offset value (e.g., "-12" from "-12($fp)")
    int offset = 0;
    if (strstr(location, "($fp)") != NULL) {
        sscanf(location, "%d($fp)", &offset);
        
        // Calculate address (add offset to $fp)
        int resultReg = getReg(codegen, resultVar, irIndex);
        sprintf(instr, "    addiu %s, $fp, %d", getRegisterName(resultReg), offset);
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
        codegen->regDescriptors[resultReg].isDirty = false;
        
        // Store to memory for reloadability
        storeVariable(codegen, resultVar, resultReg);
        int addrIdx = findOrCreateAddrDesc(codegen, resultVar);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
        }
    } else {
        // Global variable - load address
        int resultReg = getReg(codegen, resultVar, irIndex);
        sprintf(instr, "    la %s, %s", getRegisterName(resultReg), varName);
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
        codegen->regDescriptors[resultReg].isDirty = false;
        
        // Store to memory for reloadability
        storeVariable(codegen, resultVar, resultReg);
        int addrIdx = findOrCreateAddrDesc(codegen, resultVar);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
        }
    }
}

/**
 * Translate ARRAY_ADDR operation: result = &array[index]
 * Computes the address of an array element
 * This is critical for pointer arithmetic like: ptr = arr + 3
 */
void translateArrayAddr(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* arrayName = quad->arg1;
    const char* indexVar = quad->arg2;
    const char* resultVar = quad->result;
    
    // Check if this is a pointer dereference (ptr[i]) or array access (arr[i])
    // CRITICAL FIX: Use scope-independent lookup, not lookupSymbol()
    bool isPointerAccess = false;
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, arrayName) == 0) {
            // It's pointer access if it has pointer level > 0 and is NOT a local array
            // (A parameter like int* arr has ptr_level=1)
            isPointerAccess = (symtab[i].ptr_level > 0 && !symtab[i].is_array);
            break;
        }
    }
    
    // Get element size from symbol table
    bool isCharArray = false;
    int elementSize = getArrayElementInfo(arrayName, &isCharArray);
    
    if (isPointerAccess) {
        // ptr[i] - load pointer value from memory, add index offset
        int ptrReg = getReg(codegen, arrayName, irIndex);
        
        // Get index
        int indexReg;
        if (isConstantValue(indexVar)) {
            indexReg = REG_T8;
            sprintf(instr, "    li %s, %d", getRegisterName(indexReg), atoi(indexVar));
            emitMIPS(codegen, instr);
        } else {
            indexReg = getReg(codegen, indexVar, irIndex);
        }
        
        // Calculate offset = index * element_size
        if (elementSize == 4) {
            sprintf(instr, "    sll $t9, %s, 2", getRegisterName(indexReg));
        } else if (elementSize == 1) {
            sprintf(instr, "    move $t9, %s", getRegisterName(indexReg));
        } else {
            sprintf(instr, "    li $t9, %d", elementSize);
            emitMIPS(codegen, instr);
            sprintf(instr, "    mul $t9, %s, $t9", getRegisterName(indexReg));
        }
        emitMIPS(codegen, instr);
        
        // Add offset to pointer: address = ptr + (index * elementSize)
        int resultReg = getReg(codegen, resultVar, irIndex);
        sprintf(instr, "    add %s, %s, $t9", 
                getRegisterName(resultReg), getRegisterName(ptrReg));
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
        codegen->regDescriptors[resultReg].isDirty = true;
    } else {
        // arr[i] - compute address from $fp offset
        char arrayLocation[128];
        getMemoryLocation(codegen, arrayName, arrayLocation);
        
        int baseOffset = 0;
        if (strstr(arrayLocation, "($fp)") != NULL) {
            sscanf(arrayLocation, "%d($fp)", &baseOffset);
        }
        
        // Handle constant index vs variable index
        if (isConstantValue(indexVar)) {
            // Constant index - can compute address at compile time
            int constIndex = atoi(indexVar);
            int totalOffset = baseOffset + (constIndex * elementSize);
            
            int resultReg = getReg(codegen, resultVar, irIndex);
            sprintf(instr, "    addiu %s, $fp, %d", 
                    getRegisterName(resultReg), totalOffset);
            emitMIPS(codegen, instr);
            
            updateDescriptors(codegen, resultReg, resultVar);
            codegen->regDescriptors[resultReg].isDirty = true;
        } else {
            // Variable index - must compute at runtime
            int indexReg = getReg(codegen, indexVar, irIndex);
            
            // Calculate offset = index * element_size
            if (elementSize == 4) {
                sprintf(instr, "    sll $t9, %s, 2", getRegisterName(indexReg));
            } else if (elementSize == 1) {
                sprintf(instr, "    move $t9, %s", getRegisterName(indexReg));
            } else {
                sprintf(instr, "    li $t9, %d", elementSize);
                emitMIPS(codegen, instr);
                sprintf(instr, "    mul $t9, %s, $t9", getRegisterName(indexReg));
            }
            emitMIPS(codegen, instr);
            
            // Add base offset to scaled index
            sprintf(instr, "    addiu $t9, $t9, %d", baseOffset);
            emitMIPS(codegen, instr);
            
            // Add to $fp to get final address
            int resultReg = getReg(codegen, resultVar, irIndex);
            sprintf(instr, "    add %s, $fp, $t9", getRegisterName(resultReg));
            emitMIPS(codegen, instr);
            
            updateDescriptors(codegen, resultReg, resultVar);
            codegen->regDescriptors[resultReg].isDirty = true;
        }
    }
}

/**
 * Translate DEREF operation (dereference *)
 * Format: result = *pointer
 */
void translateDeref(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* ptrVar = quad->arg1;
    const char* resultVar = quad->result;
    
    // Get register containing pointer address
    int ptrReg = getReg(codegen, ptrVar, irIndex);
    
    // Load value at pointer address
    int resultReg = getReg(codegen, resultVar, irIndex);
    sprintf(instr, "    lw %s, 0(%s)",
           getRegisterName(resultReg),
           getRegisterName(ptrReg));
    emitMIPS(codegen, instr);
    
    updateDescriptors(codegen, resultReg, resultVar);
    codegen->regDescriptors[resultReg].isDirty = true;
    
    // Store result to memory for reloadability
    storeVariable(codegen, resultVar, resultReg);
    int addrIdx = findOrCreateAddrDesc(codegen, resultVar);
    if (addrIdx >= 0) {
        codegen->addrDescriptors[addrIdx].inMemory = true;
    }
}

/**
 * Translate ASSIGN_DEREF operation (*ptr = value)
 * Format: ASSIGN_DEREF value, pointer, ""
 */
void translateAssignDeref(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* valueVar = quad->arg1;
    const char* ptrVar = quad->arg2;
    
    // Get register containing pointer address
    int ptrReg = getReg(codegen, ptrVar, irIndex);
    
    // Get value to store
    int valueReg;
    if (isConstantValue(valueVar)) {
        // Use loadVariable which handles floats
        loadVariable(codegen, valueVar, REG_T9);
        sprintf(instr, "    sw $t9, 0(%s)", getRegisterName(ptrReg));
        emitMIPS(codegen, instr);
    } else {
        valueReg = getReg(codegen, valueVar, irIndex);
        sprintf(instr, "    sw %s, 0(%s)", getRegisterName(valueReg), getRegisterName(ptrReg));
        emitMIPS(codegen, instr);
    }
    
    // CRITICAL FIX: After storing through a pointer, we don't know which variable was modified
    // due to pointer aliasing (the pointer could point to any variable)
    // Therefore, we must invalidate ALL register descriptors to force subsequent loads
    // to read fresh values from memory. This is conservative but correct.
    // Similar to what we do after scanf (which also modifies memory through pointers)
    for (int r = REG_T0; r <= REG_T9; r++) {
        clearRegisterDescriptor(codegen, r);
    }
    // Also invalidate address descriptors - variables are no longer in registers
    for (int i = 0; i < codegen->addrDescCount; i++) {
        codegen->addrDescriptors[i].inRegister = -1;
    }
}

/**
 * Translate LOAD_OFFSET operation (load from pointer with offset)
 * Format: result = *(pointer + offset)
 * IR: op="LOAD_OFFSET", arg1=pointer, arg2=offset, result=dest
 */
void translateLoadOffset(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* ptrVar = quad->arg1;
    const char* offsetStr = quad->arg2;
    const char* resultVar = quad->result;
    
    // Parse offset (can be a number or variable)
    int offset = 0;
    bool isConstOffset = isConstantValue(offsetStr);
    
    if (isConstOffset) {
        offset = atoi(offsetStr);
    }
    
    // Get register containing pointer address
    int ptrReg = getReg(codegen, ptrVar, irIndex);
    
    // Load value at pointer address + offset
    int resultReg = getReg(codegen, resultVar, irIndex);
    
    if (isConstOffset) {
        sprintf(instr, "    lw %s, %d(%s)    # %s = *(%s + %d)",
               getRegisterName(resultReg), offset,
               getRegisterName(ptrReg), resultVar, ptrVar, offset);
    } else {
        // Variable offset - must compute address
        int offsetReg = getReg(codegen, offsetStr, irIndex);
        sprintf(instr, "    add $t9, %s, %s", 
               getRegisterName(ptrReg), getRegisterName(offsetReg));
        emitMIPS(codegen, instr);
        sprintf(instr, "    lw %s, 0($t9)    # %s = *(%s + %s)",
               getRegisterName(resultReg), resultVar, ptrVar, offsetStr);
    }
    emitMIPS(codegen, instr);
    
    updateDescriptors(codegen, resultReg, resultVar);
    codegen->regDescriptors[resultReg].isDirty = false;
    
    // Store result to memory for reloadability
    storeVariable(codegen, resultVar, resultReg);
    int addrIdx = findOrCreateAddrDesc(codegen, resultVar);
    if (addrIdx >= 0) {
        codegen->addrDescriptors[addrIdx].inMemory = true;
    }
}

/**
 * Translate STORE_OFFSET operation (store to pointer with offset)
 * Format: *(pointer + offset) = value
 * IR: op="STORE_OFFSET", arg1=pointer, arg2=offset, result=value
 */
void translateStoreOffset(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* ptrVar = quad->arg1;
    const char* offsetStr = quad->arg2;
    const char* valueVar = quad->result;
    
    // Parse offset (can be a number or variable)
    int offset = 0;
    bool isConstOffset = isConstantValue(offsetStr);
    
    if (isConstOffset) {
        offset = atoi(offsetStr);
    }
    
    // Get register containing pointer address
    int ptrReg = getReg(codegen, ptrVar, irIndex);
    
    // Get value to store
    int valueReg;
    if (isConstantValue(valueVar)) {
        // Use loadVariable which handles floats
        loadVariable(codegen, valueVar, REG_T9);
        
        if (isConstOffset) {
            sprintf(instr, "    sw $t9, %d(%s)    # *(%s + %d) = %s", 
                   offset, getRegisterName(ptrReg), ptrVar, offset, valueVar);
        } else {
            int offsetReg = getReg(codegen, offsetStr, irIndex);
            sprintf(instr, "    add $t8, %s, %s", 
                   getRegisterName(ptrReg), getRegisterName(offsetReg));
            emitMIPS(codegen, instr);
            sprintf(instr, "    sw $t9, 0($t8)    # *(%s + %s) = %s", 
                   ptrVar, offsetStr, valueVar);
        }
        emitMIPS(codegen, instr);
    } else {
        valueReg = getReg(codegen, valueVar, irIndex);
        
        if (isConstOffset) {
            sprintf(instr, "    sw %s, %d(%s)    # *(%s + %d) = %s", 
                   getRegisterName(valueReg), offset, 
                   getRegisterName(ptrReg), ptrVar, offset, valueVar);
        } else {
            int offsetReg = getReg(codegen, offsetStr, irIndex);
            sprintf(instr, "    add $t9, %s, %s", 
                   getRegisterName(ptrReg), getRegisterName(offsetReg));
            emitMIPS(codegen, instr);
            sprintf(instr, "    sw %s, 0($t9)    # *(%s + %s) = %s", 
                   getRegisterName(valueReg), ptrVar, offsetStr, valueVar);
        }
        emitMIPS(codegen, instr);
    }
}

// ----------------------------------------------------------------------------
// Task 3.4: I/O Operations
// ----------------------------------------------------------------------------

/**
 * Generate MIPS syscall
 * Common syscalls:
 *  1 = print_int
 *  4 = print_string
 *  5 = read_int
 *  8 = read_string
 * 10 = exit
 */
void generateSyscall(MIPSCodeGenerator* codegen, int syscall_num, int reg) {
    char instr[256];
    
    // Load syscall number
    sprintf(instr, "    li $v0, %d", syscall_num);
    emitMIPS(codegen, instr);
    
    // Argument is already in the register (usually $a0)
    // Just call syscall
    emitMIPS(codegen, "    syscall");
}

// ============================================================================
// Task 2.5: Data Section Generation
// ============================================================================

/**
 * Generate .data section (static allocation - Lecture 32)
 */
void generateDataSection(MIPSCodeGenerator* codegen) {
    extern StaticVarInfo staticVars[];
    extern int staticVarCount;
    
    emitMIPS(codegen, ".data");
    emitMIPS(codegen, "");
    
    // Create a combined list of all variables that need to be in .data section
    // We'll use staticVars as the source of truth for initial values
    
    // First, emit all variables from staticVars array (these have correct init values)
    for (int i = 0; i < staticVarCount; i++) {
        char directive[256];
        sprintf(directive, "%s: .word %s", staticVars[i].name, staticVars[i].init_value);
        emitMIPS(codegen, directive);
    }
    
    // Second, check for global variables in symbol table that might not be in staticVars
    // (e.g., uninitialized globals or arrays)
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level == 0 && !symtab[i].is_function) {
            // Check if already added from staticVars
            bool alreadyAdded = false;
            for (int j = 0; j < staticVarCount; j++) {
                if (strcmp(staticVars[j].name, symtab[i].name) == 0) {
                    alreadyAdded = true;
                    break;
                }
            }
            
            if (!alreadyAdded) {
                char directive[256];
                if (symtab[i].is_array) {
                    // Array allocation
                    int totalSize = symtab[i].size;
                    sprintf(directive, "%s: .space %d    # Array", symtab[i].name, totalSize);
                } else {
                    // Simple variable with no initializer
                    sprintf(directive, "%s: .word 0", symtab[i].name);
                }
                emitMIPS(codegen, directive);
            }
        }
    }
    
    // 2. Float constants (scan IR for float literals)
    codegen->floatConstCount = 0;
    for (int i = 0; i < codegen->irCount; i++) {
        const char* args[] = {codegen->IR[i].arg1, codegen->IR[i].arg2, codegen->IR[i].result};
        for (int a = 0; a < 3; a++) {
            if (args[a] && isFloatConstant(args[a])) {
                bool alreadyAdded = false;
                for (int j = 0; j < codegen->floatConstCount; j++) {
                    if (strcmp(codegen->floatConstants[j], args[a]) == 0) {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded && codegen->floatConstCount < 100) {
                    strcpy(codegen->floatConstants[codegen->floatConstCount], args[a]);
                    codegen->floatConstCount++;
                }
            }
        }
    }
    
    // Emit float constants
    for (int i = 0; i < codegen->floatConstCount; i++) {
        char directive[256];
        char cleanValue[64];
        strcpy(cleanValue, codegen->floatConstants[i]);
        // Remove 'f' suffix if present
        int len = strlen(cleanValue);
        if (len > 0 && (cleanValue[len-1] == 'f' || cleanValue[len-1] == 'F')) {
            cleanValue[len-1] = '\0';
        }
        sprintf(directive, "_float%d: .float %s", i, cleanValue);
        emitMIPS(codegen, directive);
    }
    
    // 3. String literals (scan IR for PARAM and ASSIGN instructions with strings)
    codegen->stringCount = 0;
    for (int i = 0; i < codegen->irCount; i++) {
        // Check PARAM instructions (for printf/scanf format strings and arguments)
        if (strcmp(codegen->IR[i].op, "PARAM") == 0 || strcmp(codegen->IR[i].op, "param") == 0) {
            if (codegen->IR[i].arg1[0] == '"') {
                // Check if already added (avoid duplicates)
                bool alreadyAdded = false;
                for (int j = 0; j < codegen->stringCount; j++) {
                    if (strcmp(codegen->stringLiterals[j], codegen->IR[i].arg1) == 0) {
                        alreadyAdded = true;
                        break;
                    }
                }
                
                if (!alreadyAdded) {
                    // Save string literal for later lookup
                    strcpy(codegen->stringLiterals[codegen->stringCount], codegen->IR[i].arg1);
                    
                    char directive[512];
                    sprintf(directive, "_str%d: .asciiz %s", codegen->stringCount, codegen->IR[i].arg1);
                    emitMIPS(codegen, directive);
                    codegen->stringCount++;
                }
            }
        }
        // Check ASSIGN instructions (for string variable initialization like msg = "Hello")
        else if (strcmp(codegen->IR[i].op, "ASSIGN") == 0 || strcmp(codegen->IR[i].op, "=") == 0) {
            if (codegen->IR[i].arg1[0] == '"') {
                // Check if already added (avoid duplicates)
                bool alreadyAdded = false;
                for (int j = 0; j < codegen->stringCount; j++) {
                    if (strcmp(codegen->stringLiterals[j], codegen->IR[i].arg1) == 0) {
                        alreadyAdded = true;
                        break;
                    }
                }
                
                if (!alreadyAdded) {
                    // Save string literal for later lookup
                    strcpy(codegen->stringLiterals[codegen->stringCount], codegen->IR[i].arg1);
                    
                    char directive[512];
                    sprintf(directive, "_str%d: .asciiz %s", codegen->stringCount, codegen->IR[i].arg1);
                    emitMIPS(codegen, directive);
                    codegen->stringCount++;
                }
            }
        }
    }
    
    // Add newline string for printf
    char newline[128];
    sprintf(newline, "_newline: .asciiz \"\\n\"");
    emitMIPS(codegen, newline);
    
    emitMIPS(codegen, "");
}

// ============================================================================
// Task 2.6: Function Prologue/Epilogue
// ============================================================================

/**
 * Generate function prologue (stack frame setup)
 */
void generatePrologue(MIPSCodeGenerator* codegen, const char* funcName) {
    ActivationRecord* record = getActivationRecord(funcName);
    if (!record) {
        fprintf(stderr, "Error: No activation record for function %s\n", funcName);
        return;
    }
    
    char instr[256];
    char comment[512];
    
    // Allocate stack frame
    sprintf(comment, "    # Function: %s (Frame size: %d bytes)", funcName, record->frameSize);
    emitMIPS(codegen, comment);
    
    sprintf(instr, "    addiu $sp, $sp, -%d", record->frameSize);
    emitMIPS(codegen, instr);    // Save $ra
    sprintf(instr, "    sw $ra, %d($sp)", record->frameSize - 4);
    emitMIPS(codegen, instr);
    
    // Save $fp
    sprintf(instr, "    sw $fp, %d($sp)", record->frameSize - 8);
    emitMIPS(codegen, instr);
    
    // Set new $fp
    sprintf(instr, "    addiu $fp, $sp, %d", record->frameSize - 4);
    emitMIPS(codegen, instr);
    
    // CRITICAL FIX: Save parameters from $a0-$a3 to their stack locations
    // This ensures parameters can be restored after function calls (especially recursive)
    char params[16][128];
    int paramCount = 0;
    getParameterNames(funcName, params, &paramCount);
    
    for (int p = 0; p < paramCount && p < 4; p++) {
        // Find parameter's stack offset
        for (int v = 0; v < record->varCount; v++) {
            if (strcmp(record->variables[v].varName, params[p]) == 0) {
                sprintf(instr, "    sw $a%d, %d($fp)    # Save parameter %s", 
                       p, record->variables[v].offset, params[p]);
                emitMIPS(codegen, instr);
                break;
            }
        }
    }
    
    emitMIPS(codegen, "");
}

/**
 * Generate function epilogue (cleanup and return)
 */
void generateEpilogue(MIPSCodeGenerator* codegen, const char* funcName) {
    ActivationRecord* record = getActivationRecord(funcName);
    if (!record) return;
    
    char instr[256];
    
    emitMIPS(codegen, "");
    emitMIPS(codegen, "    # Function epilogue");
    
    // Restore $ra
    sprintf(instr, "    lw $ra, %d($sp)", record->frameSize - 4);
    emitMIPS(codegen, instr);
    
    // Restore $fp
    sprintf(instr, "    lw $fp, %d($sp)", record->frameSize - 8);
    emitMIPS(codegen, instr);
    
    // Deallocate stack frame
    sprintf(instr, "    addiu $sp, $sp, %d", record->frameSize);
    emitMIPS(codegen, instr);
    
    // Return
    emitMIPS(codegen, "    jr $ra");
}

// ============================================================================
// Task 2.7: Main Code Generator
// ============================================================================

/**
 * Translate a single IR instruction
 */
void translateInstruction(MIPSCodeGenerator* codegen, int irIndex) {
    Quadruple* quad = &codegen->IR[irIndex];
    
    // Skip function begin/end
    if (isFunctionBegin(quad) || isFunctionEnd(quad)) {
        return;
    }
    
    // Check if this is a label (op field is "LABEL")
    if (strcmp(quad->op, "LABEL") == 0) {
        translateLabel(codegen, quad, false);
        return;
    }
    
    // Check if op field contains a label directly (like "L0", "L1", etc.)
    if (quad->op[0] == 'L' && isdigit(quad->op[1])) {
        translateLabel(codegen, quad, false);
        return;
    }
    
    // Arithmetic operations
    if (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "SUB") == 0 ||
        strcmp(quad->op, "MUL") == 0 || strcmp(quad->op, "DIV") == 0 ||
        strcmp(quad->op, "MOD") == 0 ||
        strcmp(quad->op, "+") == 0 || strcmp(quad->op, "-") == 0 ||
        strcmp(quad->op, "*") == 0 || strcmp(quad->op, "/") == 0 ||
        strcmp(quad->op, "%") == 0 ||
        strcmp(quad->op, "PTR_ADD") == 0 || strcmp(quad->op, "PTR_SUB") == 0) {
        translateArithmetic(codegen, quad, irIndex);
    }
    // Relational operations
    else if (strcmp(quad->op, "LT") == 0 || strcmp(quad->op, "GT") == 0 ||
             strcmp(quad->op, "LE") == 0 || strcmp(quad->op, "GE") == 0 ||
             strcmp(quad->op, "EQ") == 0 || strcmp(quad->op, "NE") == 0 ||
             strcmp(quad->op, "<") == 0 || strcmp(quad->op, ">") == 0 ||
             strcmp(quad->op, "<=") == 0 || strcmp(quad->op, ">=") == 0 ||
             strcmp(quad->op, "==") == 0 || strcmp(quad->op, "!=") == 0) {
        translateRelational(codegen, quad, irIndex);
    }
    // Logical operations
    else if (strcmp(quad->op, "AND") == 0 || strcmp(quad->op, "OR") == 0 ||
             strcmp(quad->op, "NOT") == 0 || strcmp(quad->op, "!") == 0 ||
             strcmp(quad->op, "&&") == 0 || strcmp(quad->op, "||") == 0) {
        translateLogical(codegen, quad, irIndex);
    }
    // Bitwise operations
    else if (strcmp(quad->op, "BITAND") == 0 || strcmp(quad->op, "BITOR") == 0 ||
             strcmp(quad->op, "BITXOR") == 0 || strcmp(quad->op, "BITNOT") == 0 ||
             strcmp(quad->op, "LSHIFT") == 0 || strcmp(quad->op, "RSHIFT") == 0 ||
             strcmp(quad->op, "<<") == 0 || strcmp(quad->op, ">>") == 0 ||
             strcmp(quad->op, "~") == 0) {
        translateBitwise(codegen, quad, irIndex);
    }
    // Assignment
    else if (strcmp(quad->op, "ASSIGN") == 0 || strcmp(quad->op, "=") == 0) {
        translateAssignment(codegen, quad, irIndex);
    }
    // Conditional branches (Phase 2 - Fixed)
    else if (strcmp(quad->op, "IF_TRUE_GOTO") == 0 || 
             strcmp(quad->op, "IF_FALSE_GOTO") == 0 ||
             strcmp(quad->op, "IF_TRUE_GOTO_FLOAT") == 0 ||
             strcmp(quad->op, "IF_FALSE_GOTO_FLOAT") == 0) {
        translateConditionalBranch(codegen, quad, irIndex);
    }
    // Unconditional jump
    else if (strcmp(quad->op, "GOTO") == 0 || strcmp(quad->op, "goto") == 0) {
        translateGoto(codegen, quad);
    }
    // PARAM (Phase 3)
    else if (strcmp(quad->op, "PARAM") == 0 || strcmp(quad->op, "param") == 0) {
        translateParam(codegen, quad, irIndex);
    }
    // CALL (Phase 3)
    else if (strcmp(quad->op, "CALL") == 0 || strcmp(quad->op, "call") == 0) {
        translateCall(codegen, quad, irIndex);
    }
    // INDIRECT_CALL - call through a function pointer
    else if (strcmp(quad->op, "INDIRECT_CALL") == 0) {
        // Format: INDIRECT_CALL func_var <param_count> [result]
        char instr[256];

        // The function variable name is in arg1
        const char* funcVar = quad->arg1;

        // Number of params is in arg2 (but translateParam already pushed them)
        int paramCount = atoi(quad->arg2);

        // Load function pointer value into a register
        int regFunc = getReg(codegen, funcVar, irIndex);

        // Jump-and-link-register to the function pointer
        sprintf(instr, "    jalr %s    # indirect call via %s", getRegisterName(regFunc), funcVar);
        emitMIPS(codegen, instr);

        // If the indirect call has a result (quad->result non-empty), move $v0 into result
        if (quad->result && quad->result[0] != '\0') {
            int resultReg = getReg(codegen, quad->result, irIndex);
            sprintf(instr, "    move %s, $v0    # move return value", getRegisterName(resultReg));
            emitMIPS(codegen, instr);

            // Store and update descriptors
            storeVariable(codegen, quad->result, resultReg);
            updateDescriptors(codegen, resultReg, quad->result);
            codegen->regDescriptors[resultReg].isDirty = false;
            int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
            if (addrIdx >= 0) {
                codegen->addrDescriptors[addrIdx].inMemory = true;
            }
        }
    }
    // RETURN (Phase 3 - improved version)
    else if (strcmp(quad->op, "RETURN") == 0 || strcmp(quad->op, "return") == 0) {
        translateReturn(codegen, quad, irIndex);
    }
    // ARRAY_ACCESS (Phase 3)
    else if (strcmp(quad->op, "ARRAY_ACCESS") == 0) {
        translateArrayAccess(codegen, quad, irIndex);
    }
    // ASSIGN_ARRAY (Phase 3)
    else if (strcmp(quad->op, "ASSIGN_ARRAY") == 0) {
        translateAssignArray(codegen, quad, irIndex);
    }
    // ADDR - address-of operator (Phase 3)
    else if (strcmp(quad->op, "ADDR") == 0 || strcmp(quad->op, "&") == 0) {
        translateAddr(codegen, quad, irIndex);
    }
    // ARRAY_ADDR - address of array element: &arr[i]
    else if (strcmp(quad->op, "ARRAY_ADDR") == 0) {
        translateArrayAddr(codegen, quad, irIndex);
    }
    // LOAD - load from reference: LOAD [ptr] result
    else if (strncmp(quad->op, "LOAD", 4) == 0) {
        // LOAD [x] t0 means t0 = *x (load value at address in x)
        // arg1 contains "[x]" - extract the variable name
        char ptrVar[128];
        if (quad->arg1 && quad->arg1[0] == '[') {
            // Extract variable name from "[x]"
            int len = strlen(quad->arg1);
            strncpy(ptrVar, quad->arg1 + 1, len - 2);
            ptrVar[len - 2] = '\0';
        } else {
            strcpy(ptrVar, quad->arg1 ? quad->arg1 : "");
        }
        
        // Get register holding the pointer address
        int regPtr = getReg(codegen, ptrVar, irIndex);
        
        // Get register for result
        int regResult = getReg(codegen, quad->result, irIndex);
        
        // Load from address
        char instr[256];
        sprintf(instr, "    lw %s, 0(%s)    # %s = *%s", 
                getRegisterName(regResult), getRegisterName(regPtr), 
                quad->result, ptrVar);
        emitMIPS(codegen, instr);
        
        // Store and update descriptors
        storeVariable(codegen, quad->result, regResult);
        updateDescriptors(codegen, regResult, quad->result);
        int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
        if (addrIdx >= 0) {
            codegen->addrDescriptors[addrIdx].inMemory = true;
            codegen->addrDescriptors[addrIdx].inRegister = regResult;
        }
    }
    // STORE - store to reference: STORE value [ptr]
    else if (strncmp(quad->op, "STORE", 5) == 0) {
        // STORE t1 [x] means *x = t1 (store value to address in x)
        // arg2 contains "[x]" - extract the variable name
        char ptrVar[128];
        if (quad->arg2 && quad->arg2[0] == '[') {
            // Extract variable name from "[x]"
            int len = strlen(quad->arg2);
            strncpy(ptrVar, quad->arg2 + 1, len - 2);
            ptrVar[len - 2] = '\0';
        } else {
            strcpy(ptrVar, quad->arg2 ? quad->arg2 : "");
        }
        
        // Get register holding the value to store
        int regValue = getReg(codegen, quad->arg1, irIndex);
        
        // Get register holding the pointer address
        int regPtr = getReg(codegen, ptrVar, irIndex);
        
        // Store to address
        char instr[256];
        sprintf(instr, "    sw %s, 0(%s)    # *%s = %s", 
                getRegisterName(regValue), getRegisterName(regPtr), 
                ptrVar, quad->arg1);
        emitMIPS(codegen, instr);
    }
    // DEREF - dereference operator (Phase 3)
    else if (strcmp(quad->op, "DEREF") == 0 || strcmp(quad->op, "*") == 0) {
        translateDeref(codegen, quad, irIndex);
    }
    // ASSIGN_DEREF - assign to dereferenced pointer (Phase 3)
    else if (strcmp(quad->op, "ASSIGN_DEREF") == 0) {
        translateAssignDeref(codegen, quad, irIndex);
    }
    // LOAD_OFFSET - load from pointer with offset: result = *(ptr + offset)
    else if (strcmp(quad->op, "LOAD_OFFSET") == 0) {
        translateLoadOffset(codegen, quad, irIndex);
    }
    // STORE_OFFSET - store to pointer with offset: *(ptr + offset) = value
    else if (strcmp(quad->op, "STORE_OFFSET") == 0) {
        translateStoreOffset(codegen, quad, irIndex);
    }
    // CAST operations - proper type conversion
    // CAST_int_to_float, CAST_float_to_int, etc. require actual conversion instructions
    else if (strstr(quad->op, "CAST") != NULL || 
             strcmp(quad->op, "FLOAT_TO_DOUBLE") == 0 ||
             strcmp(quad->op, "DOUBLE_TO_FLOAT") == 0 ||
             strcmp(quad->op, "INT_TO_FLOAT") == 0 ||
             strcmp(quad->op, "FLOAT_TO_INT") == 0) {
        
        char instr[256];
        
        // Handle int to float conversion
        if (strstr(quad->op, "int_to_float") != NULL || strcmp(quad->op, "INT_TO_FLOAT") == 0) {
            // Load integer value
            int regSrc = getReg(codegen, quad->arg1, irIndex);
            
            // Convert: move to FPU, convert, move back
            sprintf(instr, "    mtc1 %s, $f0    # Move int to FPU", getRegisterName(regSrc));
            emitMIPS(codegen, instr);
            sprintf(instr, "    cvt.s.w $f0, $f0    # Convert int to float");
            emitMIPS(codegen, instr);
            
            // Get result register
            int regDest = getReg(codegen, quad->result, irIndex);
            sprintf(instr, "    mfc1 %s, $f0    # Move float result back", getRegisterName(regDest));
            emitMIPS(codegen, instr);
            
            // Store and update
            storeVariable(codegen, quad->result, regDest);
            updateDescriptors(codegen, regDest, quad->result);
            codegen->regDescriptors[regDest].isDirty = false;
            int addrIdx = findOrCreateAddrDesc(codegen, quad->result);
            if (addrIdx >= 0) {
                codegen->addrDescriptors[addrIdx].inMemory = true;
            }
            registerVariableType(codegen, quad->result, "float");
        }
        // Handle float to int conversion
        else if (strstr(quad->op, "float_to_int") != NULL || strcmp(quad->op, "FLOAT_TO_INT") == 0) {
            // Load float value
            int regSrc = getReg(codegen, quad->arg1, irIndex);
            
            // Convert: move to FPU, convert with truncation, move back
            sprintf(instr, "    mtc1 %s, $f0    # Move float to FPU", getRegisterName(regSrc));
            emitMIPS(codegen, instr);
            sprintf(instr, "    cvt.w.s $f0, $f0    # Convert float to int (truncate)");
            emitMIPS(codegen, instr);
            
            // Get result register
            int regDest = getReg(codegen, quad->result, irIndex);
            sprintf(instr, "    mfc1 %s, $f0    # Move int result back", getRegisterName(regDest));
            emitMIPS(codegen, instr);
            
            // Store and update
            storeVariable(codegen, quad->result, regDest);
            updateDescriptors(codegen, regDest, quad->result);
            codegen->regDescriptors[regDest].isDirty = false;
            int addrIdx2 = findOrCreateAddrDesc(codegen, quad->result);
            if (addrIdx2 >= 0) {
                codegen->addrDescriptors[addrIdx2].inMemory = true;
            }
            registerVariableType(codegen, quad->result, "int");
        }
        // Handle int to bool conversion
        else if (strstr(quad->op, "int_to_bool") != NULL) {
            int regSrc = getReg(codegen, quad->arg1, irIndex);
            int regDest = getReg(codegen, quad->result, irIndex);
            
            // Convert to boolean (0 or 1)
            sprintf(instr, "    sltu %s, $zero, %s    # Convert to bool", 
                   getRegisterName(regDest), getRegisterName(regSrc));
            emitMIPS(codegen, instr);
            
            storeVariable(codegen, quad->result, regDest);
            updateDescriptors(codegen, regDest, quad->result);
            codegen->regDescriptors[regDest].isDirty = false;
            int addrIdx3 = findOrCreateAddrDesc(codegen, quad->result);
            if (addrIdx3 >= 0) {
                codegen->addrDescriptors[addrIdx3].inMemory = true;
            }
        }
        // For other casts (char/int, float/double, etc.), just do a simple move
        else {
            translateAssignment(codegen, quad, irIndex);
        }
    }
}

/**
 * Generate code for a single function
 */
void generateFunction(MIPSCodeGenerator* codegen, int funcStart, int funcEnd) {
    const char* funcName = codegen->IR[funcStart].arg1;
    strcpy(codegen->currentFuncName, funcName);
    
    // Set current function pointer
    codegen->currentFunction = getActivationRecord(funcName);
    
    // Reset function call state
    codegen->currentParamCount = 0;
    
    // Function label - prefix with '_' to avoid conflicts with MIPS instructions
    // (e.g., function 'add' would conflict with 'add' instruction)
    char label[128];
    if (strcmp(funcName, "main") == 0) {
        // main should remain as 'main' for SPIM entry point
        sprintf(label, "%s:", funcName);
    } else {
        // Other functions get '_' prefix to avoid instruction name conflicts
        sprintf(label, "_%s:", funcName);
    }
    emitMIPS(codegen, label);
    
    // Prologue
    generatePrologue(codegen, funcName);
    
    // Initialize descriptors for this function
    initDescriptors(codegen);
    
    // Track if previous instruction was a return (to avoid generating unreachable code)
    bool prevWasReturn = false;
    
    // Translate each instruction
    for (int i = funcStart + 1; i < funcEnd; i++) {
        Quadruple* quad = &codegen->IR[i];
        
        // Check if this is a label following a return
        bool isLabel = (strcmp(quad->op, "LABEL") == 0 || 
                       (quad->op[0] == 'L' && isdigit(quad->op[1])));
        
        if (prevWasReturn && isLabel) {
            // Label after return - don't spill registers (unreachable code between return and label)
            translateLabel(codegen, quad, true);
            prevWasReturn = false;
        } else {
            translateInstruction(codegen, i);
            
            // Check if this was a return statement
            prevWasReturn = (strcmp(quad->op, "RETURN") == 0 || strcmp(quad->op, "return") == 0);
        }
    }
    
    // NOTE: Epilogue is generated by RETURN instruction, not here
    // generateEpilogue(codegen, funcName);
    emitMIPS(codegen, "");
}

/**
 * Generate .text section
 */
void generateTextSection(MIPSCodeGenerator* codegen) {
    emitMIPS(codegen, ".text");
    emitMIPS(codegen, ".globl main");
    emitMIPS(codegen, "");
    
    // Process each function in IR
    int i = 0;
    while (i < codegen->irCount) {
        if (isFunctionBegin(&codegen->IR[i])) {
            const char* funcName = codegen->IR[i].arg1;
            
            // Find function end
            int funcEnd = i + 1;
            while (funcEnd < codegen->irCount && !isFunctionEnd(&codegen->IR[funcEnd])) {
                funcEnd++;
            }
            
            // Generate function
            generateFunction(codegen, i, funcEnd);
            
            i = funcEnd + 1;
        } else {
            i++;
        }
    }
    
    // Generate C standard library function implementations
    emitMIPS(codegen, "");
    emitMIPS(codegen, "# ============================================");
    emitMIPS(codegen, "# C Standard Library Function Implementations");
    emitMIPS(codegen, "# ============================================");
    emitMIPS(codegen, "");
    
    // atoi - ASCII to Integer
    // Input: $a0 = string address
    // Output: $v0 = integer value
    emitMIPS(codegen, "_atoi:");
    emitMIPS(codegen, "    li $v0, 0          # result = 0");
    emitMIPS(codegen, "    li $t1, 1          # sign = 1 (positive)");
    emitMIPS(codegen, "    move $t0, $a0      # ptr = str");
    emitMIPS(codegen, "");
    emitMIPS(codegen, "    # Skip leading whitespace");
    emitMIPS(codegen, "_atoi_skip_space:");
    emitMIPS(codegen, "    lb $t2, 0($t0)     # load char");
    emitMIPS(codegen, "    beq $t2, 32, _atoi_next_char   # if space, skip");
    emitMIPS(codegen, "    beq $t2, 9, _atoi_next_char    # if tab, skip");
    emitMIPS(codegen, "    j _atoi_check_sign");
    emitMIPS(codegen, "_atoi_next_char:");
    emitMIPS(codegen, "    addi $t0, $t0, 1");
    emitMIPS(codegen, "    j _atoi_skip_space");
    emitMIPS(codegen, "");
    emitMIPS(codegen, "    # Check for sign");
    emitMIPS(codegen, "_atoi_check_sign:");
    emitMIPS(codegen, "    lb $t2, 0($t0)     # load char");
    emitMIPS(codegen, "    bne $t2, 45, _atoi_check_plus  # if not '-', check '+'");
    emitMIPS(codegen, "    li $t1, -1         # sign = -1");
    emitMIPS(codegen, "    addi $t0, $t0, 1   # skip '-'");
    emitMIPS(codegen, "    j _atoi_loop");
    emitMIPS(codegen, "_atoi_check_plus:");
    emitMIPS(codegen, "    bne $t2, 43, _atoi_loop  # if not '+', start conversion");
    emitMIPS(codegen, "    addi $t0, $t0, 1   # skip '+'");
    emitMIPS(codegen, "");
    emitMIPS(codegen, "    # Convert digits");
    emitMIPS(codegen, "_atoi_loop:");
    emitMIPS(codegen, "    lb $t2, 0($t0)     # load char");
    emitMIPS(codegen, "    blt $t2, 48, _atoi_done   # if < '0', done");
    emitMIPS(codegen, "    bgt $t2, 57, _atoi_done   # if > '9', done");
    emitMIPS(codegen, "    addi $t2, $t2, -48 # convert ASCII to digit");
    emitMIPS(codegen, "    mul $v0, $v0, 10   # result *= 10");
    emitMIPS(codegen, "    add $v0, $v0, $t2  # result += digit");
    emitMIPS(codegen, "    addi $t0, $t0, 1   # ptr++");
    emitMIPS(codegen, "    j _atoi_loop");
    emitMIPS(codegen, "_atoi_done:");
    emitMIPS(codegen, "    mul $v0, $v0, $t1  # apply sign");
    emitMIPS(codegen, "    jr $ra");
    emitMIPS(codegen, "");
    
    // atof - ASCII to Float (simplified - returns integer part)
    // Input: $a0 = string address
    // Output: $v0 = integer value (float truncated to int)
    emitMIPS(codegen, "_atof:");
    emitMIPS(codegen, "    # Simplified atof - just calls atoi for integer part");
    emitMIPS(codegen, "    addiu $sp, $sp, -4");
    emitMIPS(codegen, "    sw $ra, 0($sp)");
    emitMIPS(codegen, "    jal _atoi          # reuse atoi implementation");
    emitMIPS(codegen, "    lw $ra, 0($sp)");
    emitMIPS(codegen, "    addiu $sp, $sp, 4");
    emitMIPS(codegen, "    jr $ra");
    emitMIPS(codegen, "");
    
    // atol - ASCII to Long (same as atoi on 32-bit MIPS)
    // Input: $a0 = string address
    // Output: $v0 = integer value
    emitMIPS(codegen, "_atol:");
    emitMIPS(codegen, "    # atol same as atoi on 32-bit MIPS");
    emitMIPS(codegen, "    addiu $sp, $sp, -4");
    emitMIPS(codegen, "    sw $ra, 0($sp)");
    emitMIPS(codegen, "    jal _atoi");
    emitMIPS(codegen, "    lw $ra, 0($sp)");
    emitMIPS(codegen, "    addiu $sp, $sp, 4");
    emitMIPS(codegen, "    jr $ra");
    emitMIPS(codegen, "");
    
    // abs - Absolute Value
    // Input: $a0 = integer
    // Output: $v0 = |integer|
    emitMIPS(codegen, "_abs:");
    emitMIPS(codegen, "    move $v0, $a0      # result = input");
    emitMIPS(codegen, "    bgez $a0, _abs_done  # if >= 0, done");
    emitMIPS(codegen, "    neg $v0, $a0       # else result = -input");
    emitMIPS(codegen, "_abs_done:");
    emitMIPS(codegen, "    jr $ra");
    emitMIPS(codegen, "");
}

/**
 * Main entry point for MIPS code generation
 */
void generateMIPSCode(MIPSCodeGenerator* codegen) {
    // Open output file
    char filename[256];
    sprintf(filename, "output.s");
    codegen->outputFile = fopen(filename, "w");
    
    if (!codegen->outputFile) {
        fprintf(stderr, "Error: Cannot create output file %s\n", filename);
        return;
    }
    
    // Generate .data section
    generateDataSection(codegen);
    
    // Generate .text section
    generateTextSection(codegen);
    
    // Close output file
    fclose(codegen->outputFile);
    codegen->outputFile = NULL;
}

/**
 * Main entry point for testing Phase 2
 */
void testMIPSCodeGeneration() {
    // First, run basic block analysis (needed for next-use info)
    analyzeIR();
    
    // Initialize code generator - use malloc to avoid stack overflow
    MIPSCodeGenerator* codegen = (MIPSCodeGenerator*)malloc(sizeof(MIPSCodeGenerator));
    if (!codegen) {
        fprintf(stderr, "Error: Cannot allocate memory for code generator\n");
        return;
    }
    initMIPSCodeGen(codegen);
    
    // Compute activation records (from Phase 1)
    computeActivationRecords();
    
    // DEBUG: Print activation records
    printActivationRecords();
    
    // Copy activation records to codegen
    for (int i = 0; i < activationRecordCount; i++) {
        codegen->activationRecords[i] = activationRecords[i];
    }
    codegen->funcCount = activationRecordCount;
    
    // Generate MIPS code
    generateMIPSCode(codegen);
    
    printf("MIPS assembly generated: output.s\n");
    
    // Free memory
    free(codegen);
}

