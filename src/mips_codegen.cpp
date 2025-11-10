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
    if (addrIdx >= 0) {
        sprintf(location, "%d($fp)", codegen->addrDescriptors[addrIdx].memoryOffset);
    } else {
        // Default offset if not found
        sprintf(location, "-12($fp)");
    }
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
        // Load immediate value
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
        // For constants, just use an empty register
        for (int r = REG_T0; r <= REG_T9; r++) {
            if (codegen->regDescriptors[r].varCount == 0) {
                return r;
            }
        }
        return REG_T0;  // Fallback
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
            loadVariable(codegen, varName, r);
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
    
    // Load our variable into the freed register
    loadVariable(codegen, varName, victimReg);
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
    sprintf(label, "%s:", quad->result);
    emitMIPS(codegen, label);
}

/**
 * Translate unconditional jump
 */
void translateGoto(MIPSCodeGenerator* codegen, Quadruple* quad) {
    char instr[128];
    sprintf(instr, "    j %s", quad->result);
    emitMIPS(codegen, instr);
}

/**
 * Translate conditional branch
 */
void translateConditionalBranch(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    // Get register for condition
    int regCond = getReg(codegen, quad->arg1, irIndex);
    
    if (strstr(quad->op, "IF_TRUE_GOTO") || strstr(quad->op, "!= 0")) {
        // Branch if not zero
        sprintf(instr, "    bnez %s, %s", getRegisterName(regCond), quad->result);
    } else if (strstr(quad->op, "IF_FALSE_GOTO") || strstr(quad->op, "== 0")) {
        // Branch if zero
        sprintf(instr, "    beqz %s, %s", getRegisterName(regCond), quad->result);
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
    int stringCount = 0;
    for (int i = 0; i < codegen->irCount; i++) {
        if (strcmp(codegen->IR[i].op, "PARAM") == 0) {
            if (codegen->IR[i].arg1[0] == '"') {
                char directive[512];
                sprintf(directive, "_str%d: .asciiz %s", stringCount++, codegen->IR[i].arg1);
                emitMIPS(codegen, directive);
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
    
    // Check if this is a label (op field contains label like "L0")
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
    // Conditional branches
    else if (strstr(quad->op, "if") != NULL && strstr(quad->op, "goto") != NULL) {
        translateConditionalBranch(codegen, quad, irIndex);
    }
    // Unconditional jump
    else if (strcmp(quad->op, "GOTO") == 0 || strcmp(quad->op, "goto") == 0) {
        translateGoto(codegen, quad);
    }
    // RETURN
    else if (strcmp(quad->op, "RETURN") == 0 || strcmp(quad->op, "return") == 0) {
        if (strlen(quad->arg1) > 0) {
            // Return with value
            int regRet = getReg(codegen, quad->arg1, irIndex);
            char instr[128];
            sprintf(instr, "    move $v0, %s", getRegisterName(regRet));
            emitMIPS(codegen, instr);
        }
    }
    // TODO: Add more instruction types (CALL, PARAM, arrays, pointers, etc.)
}

/**
 * Generate code for a single function
 */
void generateFunction(MIPSCodeGenerator* codegen, int funcStart, int funcEnd) {
    const char* funcName = codegen->IR[funcStart].arg1;
    strcpy(codegen->currentFuncName, funcName);
    
    // Set current function pointer
    codegen->currentFunction = getActivationRecord(funcName);
    
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
    
    // Epilogue
    generateEpilogue(codegen, funcName);
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

