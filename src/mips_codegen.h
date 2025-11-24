/**
 * MIPS Assembly Code Generator
 * Following Lectures 32-36: Runtime Environment & Code Generation
 * 
 * This module will implement:
 * - Register allocation with getReg() algorithm (Lecture 35)
 * - Register and address descriptors (Lecture 35)
 * - Stack frame management (Lectures 32-33)
 * - Calling conventions (Lecture 33)
 * - MIPS instruction generation (Lecture 34-35)
 * - Peephole optimization (Lecture 36) - Optional
 */

#ifndef MIPS_CODEGEN_H
#define MIPS_CODEGEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>
#include "ir_context.h"
#include "basic_block.h"

// MIPS Register definitions
#define REG_ZERO 0   // $zero - constant 0
#define REG_V0   2   // $v0 - return value
#define REG_V1   3   // $v1 - return value 2
#define REG_A0   4   // $a0-$a3 - function arguments
#define REG_A1   5
#define REG_A2   6
#define REG_A3   7
#define REG_T0   8   // $t0-$t9 - temporary registers (caller-saved)
#define REG_T1   9
#define REG_T2   10
#define REG_T3   11
#define REG_T4   12
#define REG_T5   13
#define REG_T6   14
#define REG_T7   15
#define REG_S0   16  // $s0-$s7 - saved registers (callee-saved)
#define REG_S1   17
#define REG_S2   18
#define REG_S3   19
#define REG_S4   20
#define REG_S5   21
#define REG_S6   22
#define REG_S7   23
#define REG_T8   24
#define REG_T9   25
#define REG_SP   29  // $sp - stack pointer
#define REG_FP   30  // $fp - frame pointer
#define REG_RA   31  // $ra - return address

#define NUM_TEMP_REGS 10  // $t0-$t9
#define MAX_VARIABLES 1000
#define MAX_FUNCTIONS 100

/**
 * Register Descriptor (Lecture 35)
 * Tracks which variable(s) are currently in each register
 */
typedef struct RegisterDescriptor {
    char varNames[10][128];  // Variables currently in this register
    int varCount;            // Number of variables
    bool isDirty;            // Has the register been modified?
} RegisterDescriptor;

/**
 * Address Descriptor (Lecture 35)
 * Tracks all locations where a variable's current value exists
 * (could be in register, memory, or both)
 */
typedef struct AddressDescriptor {
    char varName[128];
    bool inMemory;           // Value is in memory
    int inRegister;          // Register number (-1 if not in register)
    int memoryOffset;        // Offset from $fp (for locals)
    bool isGlobal;           // Is this a global variable?
} AddressDescriptor;

/**
 * Function Activation Record Information (Lectures 32-33)
 * Stack frame layout and variable offsets
 */
typedef struct ActivationRecord {
    char funcName[128];
    int frameSize;           // Total size of stack frame
    int numLocals;           // Number of local variables
    int numParams;           // Number of parameters
    int maxTemps;            // Maximum temporaries needed
    int savedRegsSize;       // Space for saved $s registers
    
    // Variable offsets from $fp
    struct {
        char varName[128];
        int offset;          // Offset from $fp (negative for locals)
        int size;            // Size in bytes
    } variables[MAX_VARIABLES];
    int varCount;
} ActivationRecord;

/**
 * MIPS Code Generator Context
 * Main structure holding all code generation state
 */
typedef struct MIPSCodeGenerator {
    // Input IR
    Quadruple* IR;
    int irCount;
    
    // Basic block analysis results
    BasicBlock* blocks;
    int blockCount;
    NextUseInfo* nextUseInfo;
    
    // Register allocation (Lecture 35)
    RegisterDescriptor regDescriptors[32];  // All MIPS registers
    AddressDescriptor addrDescriptors[MAX_VARIABLES];
    int addrDescCount;
    
    // Runtime environment (Lectures 32-33)
    ActivationRecord activationRecords[MAX_FUNCTIONS];
    int funcCount;
    ActivationRecord* currentFunction;
    
    // Code generation state
    FILE* outputFile;
    int currentBlock;
    bool inFunction;
    char currentFuncName[128];
    
    // Function call state (Phase 3)
    int currentParamCount;      // Track params for current CALL
    int paramRegisterMap[10];   // Map param index to register number
    
    // String literal mapping (Phase 3)
    char stringLiterals[100][256];  // Store string literals
    int stringCount;                 // Count of string literals
} MIPSCodeGenerator;

// ============================================================================
// Main API Functions
// ============================================================================

/**
 * Initialize MIPS code generator
 */
void initMIPSCodeGen(MIPSCodeGenerator* codegen);

/**
 * Main entry point: Generate MIPS assembly from IR
 */
void generateMIPS(const char* irFilename, const char* outputFilename);

/**
 * Generate MIPS code for entire IR
 */
void generateMIPSCode(MIPSCodeGenerator* codegen);

// ============================================================================
// Phase 1: Runtime Environment (Lectures 32-33)
// ============================================================================

/**
 * Compute activation records for all functions
 */
void computeActivationRecords(MIPSCodeGenerator* codegen);

/**
 * Print activation records (for testing/debugging)
 */
void printActivationRecords(MIPSCodeGenerator* codegen);

/**
 * Test activation record computation (called from main.cpp)
 */
void testActivationRecords();

/**
 * Test MIPS code generation (called from main.cpp)
 */
void testMIPSCodeGeneration();

/**
 * Calculate frame size for a function
 */
int calculateFrameSize(MIPSCodeGenerator* codegen, const char* funcName);

/**
 * Assign memory offsets to variables
 */
void assignVariableOffsets(MIPSCodeGenerator* codegen, ActivationRecord* record);

/**
 * Get memory offset for a variable
 */
int getVariableOffset(MIPSCodeGenerator* codegen, const char* varName);

// ============================================================================
// Phase 2: Code Generation Sections
// ============================================================================

/**
 * Generate .data section (static allocation - Lecture 32)
 */
void generateDataSection(MIPSCodeGenerator* codegen);

/**
 * Generate .text section (code)
 */
void generateTextSection(MIPSCodeGenerator* codegen);

/**
 * Generate code for a single function
 */
void generateFunction(MIPSCodeGenerator* codegen, int funcStart, int funcEnd);

// ============================================================================
// Phase 3: Function Call Conventions (Lecture 33)
// ============================================================================

/**
 * Generate function prologue (stack frame setup)
 */
void generatePrologue(MIPSCodeGenerator* codegen, const char* funcName);

/**
 * Generate function epilogue (stack frame teardown and return)
 */
void generateEpilogue(MIPSCodeGenerator* codegen, const char* funcName);

/**
 * Generate function call
 */
void generateCall(MIPSCodeGenerator* codegen, const char* funcName, int numArgs);

// ============================================================================
// Phase 4: Register Allocation (Lecture 35 - THE CORE ALGORITHM)
// ============================================================================

/**
 * getReg Algorithm (Lecture 35)
 * Find a register to hold a variable
 * Returns register number (REG_T0 to REG_T9)
 */
int getReg(MIPSCodeGenerator* codegen, const char* varName, int irIndex);

/**
 * Spill a register to memory
 */
void spillRegister(MIPSCodeGenerator* codegen, int regNum);

/**
 * Load variable into register
 */
void loadVariable(MIPSCodeGenerator* codegen, const char* varName, int regNum);

/**
 * Store register value to variable's memory location
 */
void storeVariable(MIPSCodeGenerator* codegen, const char* varName, int regNum);

/**
 * Update register and address descriptors
 */
void updateDescriptors(MIPSCodeGenerator* codegen, int regNum, const char* varName);

/**
 * Clear register descriptor
 */
void clearRegisterDescriptor(MIPSCodeGenerator* codegen, int regNum);

/**
 * Find variable in address descriptors
 */
int findAddressDescriptor(MIPSCodeGenerator* codegen, const char* varName);

// ============================================================================
// Phase 5: Instruction Translation
// ============================================================================

/**
 * Translate a single IR instruction to MIPS
 */
void translateInstruction(MIPSCodeGenerator* codegen, int irIndex);

/**
 * Translate arithmetic operation (ADD, SUB, MUL, DIV, MOD)
 */
void translateArithmetic(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate logical operation (AND, OR, NOT)
 */
void translateLogical(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate bitwise operation
 */
void translateBitwise(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate relational operation (LT, GT, EQ, etc.)
 */
void translateRelational(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate assignment
 */
void translateAssignment(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate conditional branch
 */
void translateConditionalBranch(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate unconditional jump
 */
void translateGoto(MIPSCodeGenerator* codegen, Quadruple* quad);

/**
 * Translate label
 */
void translateLabel(MIPSCodeGenerator* codegen, Quadruple* quad, bool skipSpill = false);

/**
 * Translate array access (reading from array)
 */
void translateArrayAccess(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate array assignment (writing to array)
 */
void translateAssignArray(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate ADDR operation (address-of &)
 */
void translateAddr(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate DEREF operation (dereference *)
 */
void translateDeref(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate ASSIGN_DEREF operation (*ptr = value)
 */
void translateAssignDeref(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate pointer operations (deprecated - use specific functions above)
 */
void translatePointerOp(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Emit a MIPS instruction to output file
 */
void emitMIPS(MIPSCodeGenerator* codegen, const char* instruction);

/**
 * Get register name string (e.g., "$t0")
 */
const char* getRegisterName(int regNum);

/**
 * Check if string is a temporary variable (starts with 't')
 */
bool isTemporary(const char* varName);

/**
 * Check if variable is global
 */
bool isGlobalVariable(MIPSCodeGenerator* codegen, const char* varName);

/**
 * Get memory location string for a variable
 */
void getMemoryLocation(MIPSCodeGenerator* codegen, const char* varName, char* location);

// ============================================================================
// Phase 3: Advanced Features (Function Calls, Arrays, Pointers, I/O)
// ============================================================================

/**
 * Translate PARAM instruction
 * First 4 params go to $a0-$a3, rest pushed to stack
 */
void translateParam(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate CALL instruction
 * Generate jal, handle return value, cleanup stack
 */
void translateCall(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate RETURN instruction
 * Move value to $v0, generate epilogue, jr $ra
 */
void translateReturn(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate ARRAY_ACCESS operation
 * Calculate address: base + index * element_size
 */
void translateArrayAccess(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate ADDR operation (address-of &)
 * Get address of variable
 */
void translateAddr(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Translate DEREF operation (dereference *)
 * Load value from pointer
 */
void translateDeref(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex);

/**
 * Generate MIPS syscall for I/O
 */
void generateSyscall(MIPSCodeGenerator* codegen, int syscall_num, int reg);

// ============================================================================
// Phase 6: Optimization (Lecture 36) - OPTIONAL
// ============================================================================

/**
 * Peephole optimization
 * Apply pattern-based optimizations to generated code
 */
void peepholeOptimize(MIPSCodeGenerator* codegen);

/**
 * Dead code elimination
 */
void eliminateDeadCode(MIPSCodeGenerator* codegen);

#ifdef __cplusplus
}
#endif

#endif // MIPS_CODEGEN_H
