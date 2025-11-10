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
 * Check if a constant is a float (contains a decimal point)
 */
bool isFloatConstant(const char* str) {
    if (str == NULL || strlen(str) == 0) {
        return false;
    }
    
    // Check for decimal point
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '.') {
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
    
    // Initialize function call state (Phase 3)
    codegen->currentParamCount = 0;
    codegen->stringCount = 0;
    for (int i = 0; i < 10; i++) {
        codegen->paramRegisterMap[i] = -1;
    }
    for (int i = 0; i < 100; i++) {
        codegen->stringLiterals[i][0] = '\0';
    }
    
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
    codegen->addrDescriptors[idx].inMemory = true;   // Assume in memory initially
    codegen->addrDescriptors[idx].inRegister = -1;    // Not in register yet
    codegen->addrDescriptors[idx].memoryOffset = 0;
    codegen->addrDescriptors[idx].isGlobal = false;
    
    // Get offset from current function's activation record
    if (codegen->currentFunction) {
        for (int v = 0; v < codegen->currentFunction->varCount; v++) {
            if (strcmp(codegen->currentFunction->variables[v].varName, varName) == 0) {
                codegen->addrDescriptors[idx].memoryOffset = 
                    codegen->currentFunction->variables[v].offset;
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
    
    // Update address descriptor - variable is now in this register
    int addrIdx = findOrCreateAddrDesc(codegen, varName);
    if (addrIdx >= 0) {
        codegen->addrDescriptors[addrIdx].inRegister = regNum;
    }
}

/**
 * Clear register descriptor
 */
void clearRegisterDescriptor(MIPSCodeGenerator* codegen, int regNum) {
    codegen->regDescriptors[regNum].varCount = 0;
    codegen->regDescriptors[regNum].isDirty = false;
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
 * Check if variable is global
 */
bool isGlobalVariable(MIPSCodeGenerator* codegen, const char* varName) {
    for (int i = 0; i < symCount; i++) {
        if (strcmp(symtab[i].name, varName) == 0) {
            return (symtab[i].scope_level == 0 && !symtab[i].is_function);
        }
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
    
    // Otherwise, it's a local variable - get offset from activation record
    int addrIdx = findAddressDescriptor(codegen, varName);
    if (addrIdx >= 0 && codegen->addrDescriptors[addrIdx].memoryOffset != 0) {
        sprintf(location, "%d($fp)", codegen->addrDescriptors[addrIdx].memoryOffset);
        return;
    }
    
    // FIX: Look up offset directly from activation record
    if (codegen->currentFunction != NULL) {
        for (int i = 0; i < codegen->currentFunction->varCount; i++) {
            if (strcmp(codegen->currentFunction->variables[i].varName, varName) == 0) {
                sprintf(location, "%d($fp)", codegen->currentFunction->variables[i].offset);
                return;
            }
        }
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
        // Handle float constants specially (MIPS li doesn't support floats)
        if (isFloatConstant(varName)) {
            // For now, convert float to integer (truncate)
            // TODO: Proper float support would use FPU registers and .data section
            int intValue = (int)atof(varName);
            sprintf(instr, "    li %s, %d    # Float %s truncated to int", 
                    getRegisterName(regNum), intValue, varName);
            emitMIPS(codegen, instr);
            return;
        }
        
        // Handle character literals
        if (varName[0] == '\'') {
            // Extract ASCII value from 'X'
            if (strlen(varName) >= 3) {
                int asciiValue = (int)varName[1];
                sprintf(instr, "    li %s, %d    # '%c'", 
                        getRegisterName(regNum), asciiValue, varName[1]);
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
    // Skip if varName is empty or a constant
    if (varName == NULL || strlen(varName) == 0) {
        return REG_T0;  // Default register
    }
    
    if (isConstantValue(varName)) {
        // For constants, find an empty register and load the constant
        for (int r = REG_T0; r <= REG_T9; r++) {
            if (codegen->regDescriptors[r].varCount == 0) {
                loadVariable(codegen, varName, r);
                return r;
            }
        }
        // If no empty register, use REG_T0 and load the constant
        loadVariable(codegen, varName, REG_T0);
        return REG_T0;
    }
    
    // CASE 1: Variable already in a register? (BEST CASE)
    int addrIdx = findAddressDescriptor(codegen, varName);
    if (addrIdx >= 0) {
        int reg = codegen->addrDescriptors[addrIdx].inRegister;
        if (reg >= REG_T0 && reg <= REG_T9) {
            return reg;  // Already loaded!
        }
    }
    
    // CASE 2: Find an empty temporary register
    for (int r = REG_T0; r <= REG_T9; r++) {
        if (codegen->regDescriptors[r].varCount == 0) {
            // Empty register found!
            // FIX: Only load from memory if variable has been initialized (inMemory == true)
            if (addrIdx >= 0 && codegen->addrDescriptors[addrIdx].inMemory) {
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
    
    // Get registers for operands
    int regArg1 = getReg(codegen, quad->arg1, irIndex);
    int regResult;
    
    // Handle binary operations
    if (strlen(quad->arg2) > 0 && strcmp(quad->arg2, "") != 0) {
        // Binary operation: result = arg1 op arg2
        
        // Check if arg2 is a constant (can use immediate instruction)
        if (isConstantValue(quad->arg2) && strcmp(quad->op, "ADD") == 0) {
            // Use immediate instruction for ADD
            regResult = getReg(codegen, quad->result, irIndex);
            sprintf(instr, "    addi %s, %s, %s", 
                   getRegisterName(regResult), 
                   getRegisterName(regArg1), 
                   quad->arg2);
            emitMIPS(codegen, instr);
        } else {
            // Load both operands into registers
            int regArg2 = getReg(codegen, quad->arg2, irIndex);
            regResult = getReg(codegen, quad->result, irIndex);
            
            if (strcmp(quad->op, "ADD") == 0) {
                sprintf(instr, "    add %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            } else if (strcmp(quad->op, "SUB") == 0) {
                sprintf(instr, "    sub %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            } else if (strcmp(quad->op, "MUL") == 0) {
                sprintf(instr, "    mul %s, %s, %s", 
                       getRegisterName(regResult),
                       getRegisterName(regArg1),
                       getRegisterName(regArg2));
            } else if (strcmp(quad->op, "DIV") == 0) {
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
}

/**
 * Translate simple assignment: result = arg1
 */
void translateAssignment(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    // FIX: Handle constant assignments directly (a = 10)
    if (isConstantValue(quad->arg1)) {
        // Load constant into a register
        int regSrc = getReg(codegen, quad->arg1, irIndex);
        
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
    
    // Update descriptors
    updateDescriptors(codegen, regDest, quad->result);
    codegen->regDescriptors[regDest].isDirty = true;
}

/**
 * Translate label
 */
void translateLabel(MIPSCodeGenerator* codegen, Quadruple* quad) {
    char label[128];
    
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
        sprintf(label, "%s:", labelName);
        emitMIPS(codegen, label);
    }
}

/**
 * Translate unconditional jump
 */
void translateGoto(MIPSCodeGenerator* codegen, Quadruple* quad) {
    char instr[128];
    
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
        sprintf(instr, "    j %s", label);
        emitMIPS(codegen, instr);
    } else {
        // This should rarely happen - debug info
        fprintf(stderr, "Warning: GOTO instruction with no label found. Op='%s', Arg1='%s', Arg2='%s', Result='%s'\n",
                quad->op, quad->arg1, quad->arg2, quad->result);
    }
}

/**
 * Translate conditional branch
 */
void translateConditionalBranch(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    // Get register for condition variable
    int regCond = getReg(codegen, quad->arg1, irIndex);
    
    // The target label is in arg2 for conditional branches
    const char* targetLabel = quad->arg2;
    
    // Check the opcode to determine branch type
    if (strcmp(quad->op, "IF_TRUE_GOTO") == 0) {
        // Branch if condition is true (not zero)
        sprintf(instr, "    bnez %s, %s", getRegisterName(regCond), targetLabel);
    } 
    else if (strcmp(quad->op, "IF_FALSE_GOTO") == 0) {
        // Branch if condition is false (zero)
        sprintf(instr, "    beqz %s, %s", getRegisterName(regCond), targetLabel);
    }
    else if (strcmp(quad->op, "IF_TRUE_GOTO_FLOAT") == 0) {
        // Branch if float condition is true (not zero)
        // For now, treat same as integer (proper float handling would use FPU)
        sprintf(instr, "    bnez %s, %s", getRegisterName(regCond), targetLabel);
    }
    else if (strcmp(quad->op, "IF_FALSE_GOTO_FLOAT") == 0) {
        // Branch if float condition is false (zero)
        sprintf(instr, "    beqz %s, %s", getRegisterName(regCond), targetLabel);
    }
    else {
        // Fallback - shouldn't happen but handle gracefully
        fprintf(stderr, "Warning: Unknown conditional branch op: %s\n", quad->op);
        sprintf(instr, "    beqz %s, %s", getRegisterName(regCond), targetLabel);
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
            // Get register for variable
            int srcReg = getReg(codegen, paramValue, irIndex);
            
            // Move to argument register
            sprintf(instr, "    move %s, %s", 
                   getRegisterName(argReg), 
                   getRegisterName(srcReg));
            emitMIPS(codegen, instr);
        }
        
        // Track which register holds this param
        codegen->paramRegisterMap[paramIndex] = argReg;
    } else {
        // Parameters 5+ go on the stack
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
        sprintf(instr, "    addi $sp, $sp, -4");
        emitMIPS(codegen, instr);
    }
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
        // Handle printf with syscall
        // Determine type based on first parameter
        // If first param is string, use syscall 4 (print_string)
        // Parameters are already in $a0, $a1, etc. from PARAM instructions
        
        // For simplicity, printf with format string: use syscall 4 for string, syscall 1 for int
        // The parameters are already loaded into $a0, $a1 by PARAM instructions
        
        // Print the format string (already in $a0)
        sprintf(instr, "    li $v0, 4    # syscall 4: print_string");
        emitMIPS(codegen, instr);
        emitMIPS(codegen, "    syscall");
        
        // If there's a second parameter (integer to print), print it too
        // Note: This is a simplified printf - doesn't parse format string
        if (paramCount > 1) {
            // Print the integer in $a1
            sprintf(instr, "    move $a0, $a1");
            emitMIPS(codegen, instr);
            sprintf(instr, "    li $v0, 1    # syscall 1: print_int");
            emitMIPS(codegen, instr);
            emitMIPS(codegen, "    syscall");
        }
        
        // No return value needed for printf in this simple implementation
    }
    else if (strcmp(funcName, "scanf") == 0) {
        // Handle scanf with syscall 5 (read_int)
        sprintf(instr, "    li $v0, 5    # syscall 5: read_int");
        emitMIPS(codegen, instr);
        emitMIPS(codegen, "    syscall");
        
        // Result is in $v0, store to the address in $a0
        // Note: scanf needs address, so PARAM should have passed &variable
        sprintf(instr, "    sw $v0, 0($a0)");
        emitMIPS(codegen, instr);
    }
    else {
        // Regular function call - generate jal
        sprintf(instr, "    jal %s", funcName);
        emitMIPS(codegen, instr);
        
        // If more than 4 params, restore stack pointer
        if (paramCount > 4) {
            int stackBytes = (paramCount - 4) * 4;
            sprintf(instr, "    addi $sp, $sp, %d", stackBytes);
            emitMIPS(codegen, instr);
        }
        
        // If call has a result, move return value from $v0 to destination
        if (strlen(quad->result) > 0 && strcmp(quad->result, "") != 0) {
            int destReg = getReg(codegen, quad->result, irIndex);
            sprintf(instr, "    move %s, $v0", getRegisterName(destReg));
            emitMIPS(codegen, instr);
            
            // Update descriptors
            updateDescriptors(codegen, destReg, quad->result);
        }
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
        sprintf(instr, "    lw $ra, %d($sp)", frameSize - 4);
        emitMIPS(codegen, instr);
        sprintf(instr, "    lw $fp, %d($sp)", frameSize - 8);
        emitMIPS(codegen, instr);
        sprintf(instr, "    addi $sp, $sp, %d", frameSize);
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
    
    // Get base address of array (from $fp offset)
    char arrayLocation[128];
    getMemoryLocation(codegen, arrayName, arrayLocation);
    
    // Parse offset value (e.g., "-12" from "-12($fp)")
    int baseOffset = 0;
    if (strstr(arrayLocation, "($fp)") != NULL) {
        sscanf(arrayLocation, "%d($fp)", &baseOffset);
    }
    
    // Calculate element offset
    // element_size = 4 for int/float/pointers, 1 for char
    int elementSize = 4; // Default to 4
    
    // TODO: Look up actual element size from symbol table
    // For now, assume int arrays (4 bytes)
    
    if (isConstantValue(indexVar)) {
        // Constant index - calculate offset at compile time
        int index = atoi(indexVar);
        int totalOffset = baseOffset + (index * elementSize);
        
        // Load from calculated address
        int resultReg = getReg(codegen, resultVar, irIndex);
        sprintf(instr, "    lw %s, %d($fp)", getRegisterName(resultReg), totalOffset);
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
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
        
        // Load value at address
        int resultReg = getReg(codegen, resultVar, irIndex);
        sprintf(instr, "    lw %s, 0($t8)", getRegisterName(resultReg));
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
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
    
    // Get base address of array (from $fp offset)
    char arrayLocation[128];
    getMemoryLocation(codegen, arrayName, arrayLocation);
    
    // Parse offset value
    int baseOffset = 0;
    if (strstr(arrayLocation, "($fp)") != NULL) {
        sscanf(arrayLocation, "%d($fp)", &baseOffset);
    }
    
    // Element size
    int elementSize = 4; // Default to 4 (TODO: look up from symbol table)
    
    if (isConstantValue(indexVar)) {
        // Constant index - calculate offset at compile time
        int index = atoi(indexVar);
        int totalOffset = baseOffset + (index * elementSize);
        
        // Get value to store
        if (isConstantValue(valueVar)) {
            // Constant value - use loadVariable which handles floats
            loadVariable(codegen, valueVar, REG_T9);
            sprintf(instr, "    sw $t9, %d($fp)", totalOffset);
            emitMIPS(codegen, instr);
        } else {
            // Variable value
            int valueReg = getReg(codegen, valueVar, irIndex);
            sprintf(instr, "    sw %s, %d($fp)", getRegisterName(valueReg), totalOffset);
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
            sprintf(instr, "    sw $t9, 0($t8)");
            emitMIPS(codegen, instr);
        } else {
            valueReg = getReg(codegen, valueVar, irIndex);
            sprintf(instr, "    sw %s, 0($t8)", getRegisterName(valueReg));
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
        sprintf(instr, "    addi %s, $fp, %d", getRegisterName(resultReg), offset);
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
    } else {
        // Global variable - load address
        int resultReg = getReg(codegen, resultVar, irIndex);
        sprintf(instr, "    la %s, %s", getRegisterName(resultReg), varName);
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
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
}// ----------------------------------------------------------------------------
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
    emitMIPS(codegen, ".data");
    emitMIPS(codegen, "");
    
    // 1. Global/static variables from symbol table
    for (int i = 0; i < symCount; i++) {
        if (symtab[i].scope_level == 0 && !symtab[i].is_function) {
            char directive[256];
            
            if (symtab[i].is_array) {
                // Array allocation
                int totalSize = symtab[i].size;
                sprintf(directive, "%s: .space %d    # Array", symtab[i].name, totalSize);
            } else {
                // Simple variable
                sprintf(directive, "%s: .word 0", symtab[i].name);
            }
            
            emitMIPS(codegen, directive);
        }
    }
    
    // 2. String literals (scan IR for PARAM instructions with strings)
    codegen->stringCount = 0;
    for (int i = 0; i < codegen->irCount; i++) {
        if (strcmp(codegen->IR[i].op, "PARAM") == 0 || strcmp(codegen->IR[i].op, "param") == 0) {
            if (codegen->IR[i].arg1[0] == '"') {
                // Save string literal for later lookup
                strcpy(codegen->stringLiterals[codegen->stringCount], codegen->IR[i].arg1);
                
                char directive[512];
                sprintf(directive, "_str%d: .asciiz %s", codegen->stringCount, codegen->IR[i].arg1);
                emitMIPS(codegen, directive);
                codegen->stringCount++;
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
    sprintf(instr, "    addi $sp, $sp, -%d", record->frameSize);
    emitMIPS(codegen, instr);
    
    // Save $ra
    sprintf(instr, "    sw $ra, %d($sp)", record->frameSize - 4);
    emitMIPS(codegen, instr);
    
    // Save $fp
    sprintf(instr, "    sw $fp, %d($sp)", record->frameSize - 8);
    emitMIPS(codegen, instr);
    
    // Set new $fp
    sprintf(instr, "    addi $fp, $sp, %d", record->frameSize - 4);
    emitMIPS(codegen, instr);
    
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
    sprintf(instr, "    addi $sp, $sp, %d", record->frameSize);
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
        translateLabel(codegen, quad);
        return;
    }
    
    // Check if op field contains a label directly (like "L0", "L1", etc.)
    if (quad->op[0] == 'L' && isdigit(quad->op[1])) {
        translateLabel(codegen, quad);
        return;
    }
    
    // Arithmetic operations
    if (strcmp(quad->op, "ADD") == 0 || strcmp(quad->op, "SUB") == 0 ||
        strcmp(quad->op, "MUL") == 0 || strcmp(quad->op, "DIV") == 0 ||
        strcmp(quad->op, "MOD") == 0) {
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
    // DEREF - dereference operator (Phase 3)
    else if (strcmp(quad->op, "DEREF") == 0 || strcmp(quad->op, "*") == 0) {
        translateDeref(codegen, quad, irIndex);
    }
    // ASSIGN_DEREF - assign to dereferenced pointer (Phase 3)
    else if (strcmp(quad->op, "ASSIGN_DEREF") == 0) {
        translateAssignDeref(codegen, quad, irIndex);
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
    
    // Function label
    char label[128];
    sprintf(label, "%s:", funcName);
    emitMIPS(codegen, label);
    
    // Prologue
    generatePrologue(codegen, funcName);
    
    // Initialize descriptors for this function
    initDescriptors(codegen);
    
    // Translate each instruction
    for (int i = funcStart + 1; i < funcEnd; i++) {
        translateInstruction(codegen, i);
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
    
    printf("\n=== GENERATING MIPS ASSEMBLY CODE ===\n\n");
    
    // Generate .data section
    generateDataSection(codegen);
    
    // Generate .text section
    generateTextSection(codegen);
    
    // Close output file
    fclose(codegen->outputFile);
    codegen->outputFile = NULL;
    
    printf("MIPS code generated: %s\n\n", filename);
}

/**
 * Main entry point for testing Phase 2
 */
void testMIPSCodeGeneration() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  PHASE 2: MIPS Code Generation with Register Allocation  ║\n");
    printf("║  Testing Complete Code Generation                         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // First, run basic block analysis (needed for next-use info)
    printf("\n=== Running IR Analysis ===\n");
    fflush(stdout);
    analyzeIR();
    printf("IR Analysis complete\n");
    fflush(stdout);
    
    // Initialize code generator - use malloc to avoid stack overflow
    printf("=== Initializing Code Generator ===\n");
    fflush(stdout);
    MIPSCodeGenerator* codegen = (MIPSCodeGenerator*)malloc(sizeof(MIPSCodeGenerator));
    if (!codegen) {
        fprintf(stderr, "Error: Cannot allocate memory for code generator\n");
        return;
    }
    initMIPSCodeGen(codegen);
    printf("Code generator initialized\n");
    fflush(stdout);
    
    // Compute activation records (from Phase 1)
    printf("=== Computing Activation Records ===\n");
    fflush(stdout);
    computeActivationRecords();
    printf("Activation records computed: %d functions\n", activationRecordCount);
    fflush(stdout);
    
    // Copy activation records to codegen
    for (int i = 0; i < activationRecordCount; i++) {
        codegen->activationRecords[i] = activationRecords[i];
    }
    codegen->funcCount = activationRecordCount;
    printf("Activation records copied\n");
    fflush(stdout);
    
    // Generate MIPS code
    printf("=== Generating MIPS Assembly ===\n");
    fflush(stdout);
    generateMIPSCode(codegen);
    
    printf("=== MIPS CODE GENERATION COMPLETED ===\n\n");
    
    // Free memory
    free(codegen);
}

