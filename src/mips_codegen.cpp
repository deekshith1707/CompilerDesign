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
    
    // Check for float constants (contains '.')
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '.' && (i > 0 || isdigit(str[i+1]))) {
            return true;
        }
    }
    
    return false;
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
    
    // Count parameters (first 4 go in registers, rest on stack)
    int paramCount = getParameterCount(funcName);
    if (paramCount > 4) {
        frameSize += (paramCount - 4) * 4;
    }
    
    // Count local variables
    int localCount = countLocalsInFunction(funcName);
    frameSize += localCount * 4;
    
    // Count temporaries
    int tempCount = countTemporariesInFunction(funcStart, funcEnd);
    frameSize += tempCount * 4;
    
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
    
    // First 4 parameters go in $a0-$a3 (no stack space needed for them)
    // Parameters 5+ need stack space
    for (int p = 4; p < paramCount; p++) {
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
                record->variables[record->varCount].offset = offset;
                record->variables[record->varCount].size = symtab[i].size > 0 ? symtab[i].size : 4;
                record->varCount++;
                offset -= symtab[i].size > 0 ? symtab[i].size : 4;
            }
        }
    }
    record->numLocals = record->varCount - (paramCount > 4 ? paramCount - 4 : 0);
    
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

// ============================================================================
// Task 1.4: Build Activation Records for All Functions
// ============================================================================

/**
 * Scan IR and build activation records for all functions
 */
void computeActivationRecords() {
    activationRecordCount = 0;
    
    printf("\n=== Computing Activation Records ===\n");
    
    // Find all functions in IR
    int i = 0;
    while (i < irCount) {
        if (isFunctionBegin(&IR[i])) {
            const char* funcName = IR[i].arg1;
            int funcStart = i;
            int funcEnd = findFunctionEnd(funcStart);
            
            printf("Processing function: %s (IR %d to %d)\n", funcName, funcStart, funcEnd);
            
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
    
    printf("Total functions processed: %d\n", activationRecordCount);
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
    
    printf("MIPS Code Generator initialized.\n");
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
