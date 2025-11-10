# Compiler Design Project - Work Plan & Analysis

## Assignment Requirements
- **Objective**: Write an end-to-end compiler that converts source programs into MIPS assembly code
- **Target Language**: MIPS Assembly
- **Simulator**: SPIM, MARS, or QtSpim
- **Due Date**: November 23, 2025
- **Current Date**: November 10, 2025 (13 days remaining)

---

## 🎯 CURRENT STATUS SUMMARY

### ✅ PHASE 0: IR ANALYSIS FOUNDATION - **COMPLETED** (November 10, 2025)
**Status:** All components implemented and tested
**Files Created:**
- ✅ `src/basic_block.cpp` - Basic block analysis implementation
- ✅ `src/basic_block.h` - Data structures and function declarations
- ✅ Updated `makefile` - Added compilation rules
- ✅ Updated `src/main.cpp` - Added `--analyze-blocks` option

**Implemented Features:**
- ✅ Basic Block Partitioning (Leader Finding Algorithm - Lecture 34)
- ✅ Flow Graph Construction (Successors/Predecessors - Lecture 34)
- ✅ Next-Use Information Computation (Backward Scan - Lecture 34)
- ✅ Liveness Analysis for variables
- ✅ Debug output functions for analysis verification

**Testing:**
- ✅ Tested on arithmetic operations (8 blocks)
- ✅ Tested on if-else statements (control flow verified)
- ✅ Tested on for loops (30 blocks with complex flow)

**This Enables:** Register allocation (getReg), code optimization, efficient MIPS generation

---

## 📚 QUICK REFERENCE: Key Algorithms from Lectures 32-36

### Lecture 32: Storage Allocation Strategies
- **Static Allocation:** Globals, statics, strings → `.data` section
- **Stack Allocation:** Locals, temps, params → Stack frames

### Lecture 33: Procedure Call Mechanism
- **Calling Sequence:** Caller evaluates params → saves $ra → calls function
- **Return Sequence:** Callee puts return in $v0 → restores registers → jumps to $ra
- **Parameter Passing:**
  - **Call-by-Value:** Pass value (copy)
  - **Call-by-Reference:** Pass address (pointer)
- **MIPS Convention:** First 4 params in $a0-$a3, rest on stack

### Lecture 34: Basic Blocks & Flow Graphs ⭐ CRITICAL
**Find Leaders Algorithm:**
```
1. First statement is a leader
2. Target of any goto/branch is a leader  
3. Statement after goto/branch is a leader
4. Basic block = leader + all statements until next leader
```

**Next-Use Algorithm (Backward Scan):**
```
For each statement S (from end to start of block):
    1. Attach current next-use info to S
    2. Mark result variable as "dead" (no next use)
    3. Mark operand variables as "live" with next-use = S
```

### Lecture 35: Code Generation & getReg ⭐⭐⭐ MOST CRITICAL
**Simple Code Gen for X = Y op Z:**
```
1. Call getReg to select registers Rx, Ry, Rz
2. If Y not in Ry: generate LD Ry, Y
3. If Z not in Rz: generate LD Rz, Z
4. Generate OP Rx, Ry, Rz
```

**getReg(Y) Algorithm:**
```
1. If Y already in register R: return R (BEST)
2. If empty register available: return it (GOOD)
3. If all registers full (HARD - must spill):
   a. Don't spill result variable X
   b. OK to spill if value already in memory
   c. OK to spill if variable is dead (no next use)
   d. Otherwise: generate ST to save before reusing
```

### Lecture 36: Optimizations
**Peephole Patterns:**
- Redundant load/store: `SW R,x` + `LW R,x` → eliminate `LW`
- Strength reduction: `MUL R,2` → `SLL R,1` (shift is faster)
- Algebraic: `ADD R,0` → `MOVE R`
- Jump elimination: `J L1; L1: J L2` → `J L2`

**Ershov Numbers (Optimal Expression Code):**
```
Leaf: 1
Unary: same as child
Binary: max(left, right) if different, else left+1
```

---

## Current Implementation Status

### ✅ COMPLETED: IR Generator Phase
Your IR generator implementation is **comprehensive and well-structured**:

#### 1. **Lexer & Parser (Flex + Bison)**
- `src/lexer.l`: Tokenizes C-like source code
- `src/parser.y`: Parses source and builds AST (2731 lines)
- Handles full C-like syntax including:
  - Arithmetic, logical, bitwise operators
  - Control flow (if/else, for, while, do-while, switch)
  - Functions (declarations, definitions, calls, recursion)
  - Arrays, pointers, structures, unions, enums
  - Type system (int, float, char, custom types via typedef)
  - Special keywords (static, const, goto, break, continue)
  - References (&)
  - Preprocessor directives

#### 2. **Abstract Syntax Tree (AST)**
- `src/ast.h` & `src/ast.cpp`: Clean AST node structure
- 40+ node types covering all language constructs
- Proper tree building and memory management

#### 3. **Symbol Table**
- `src/symbol_table.h` & `src/symbol_table.cpp`: Complete implementation
- Scope management (global, function, block)
- Type tracking, function signatures, parameter info
- Static variable handling
- Reference variable tracking

#### 4. **Intermediate Representation (IR) Generator**
- `src/ir_generator.h` & `src/ir_generator.cpp`: 2283 lines of comprehensive IR generation
- `src/ir_context.h` & `src/ir_context.cpp`: IR infrastructure & pretty printing
- **Three-Address Code (TAC)** format with quadruples:
  ```
  func_begin <function_name>
  t0 = a + b
  if t0 == 0 goto L0
  param "format"
  t1 = call printf, 1
  return t0
  func_end <function_name>
  ```

#### 5. **IR Features Implemented**
- ✅ Arithmetic operations (ADD, SUB, MUL, DIV, MOD)
- ✅ Logical operations (&&, ||, !)
- ✅ Bitwise operations (AND, OR, XOR, NOT, shifts)
- ✅ Relational operations (LT, GT, LE, GE, EQ, NE)
- ✅ Assignments & type casts
- ✅ Control flow (GOTO, IF_TRUE_GOTO, IF_FALSE_GOTO, labels)
- ✅ Function calls (PARAM, CALL, RETURN)
- ✅ Array operations (ARRAY_ACCESS, ASSIGN_ARRAY)
- ✅ Pointer operations (ADDR, DEREF, ASSIGN_DEREF)
- ✅ Structure member access (LOAD_MEMBER, ASSIGN_MEMBER, arrow operator)
- ✅ Increment/decrement (pre and post)
- ✅ Loop constructs (for, while, do-while, until)
- ✅ Switch-case statements
- ✅ Static variable handling
- ✅ Temporary variable generation
- ✅ Label generation
- ✅ Backpatching for control flow

#### 6. **Test Coverage**
18 comprehensive test cases covering:
1. Arithmetic & logical operations
2. If-else statements
3. For loops
4. While loops
5. Do-while loops
6. Switch-case statements
7. Arrays
8. Pointers
9. Structures
10. Printf/scanf
11. Functions & recursion
12. Goto, break, continue
13. Static keywords
14. Command-line input
15. Typedef
16. References
17. Enums & unions
18. Until loops

#### 7. **Build System**
- ✅ Well-structured Makefile with proper dependencies
- ✅ Separate obj/ directory for generated files
- ✅ Clean build process
- ✅ Modular compilation (AST, symbol table, IR context, IR generator)

#### 8. **Current Deliverables Status**
- ✅ `src/` directory: Complete with all IR generation files
- ✅ `test/` directory: 18 test cases (exceeds minimum of 5)
- ✅ `makefile`: Complete and functional
- ✅ `run.sh`: Script to run IR generator on all tests

---

## 🎯 REMAINING WORK: MIPS Assembly Code Generator

### High-Level Architecture (Based on Lectures 32-36)

```
Source Code (.txt/.c)
        ↓
    [Lexer]                                      ✅ COMPLETE
        ↓
    [Parser] → AST                               ✅ COMPLETE
        ↓
[Symbol Table & Semantic Analysis]               ✅ COMPLETE
        ↓
   [IR Generator] → Three-Address Code (.ir)     ✅ COMPLETE
        ↓
[Basic Block & Flow Graph Analysis]              ✅ COMPLETE (Phase 0)
        ↓
[Next-Use Information]                           ✅ COMPLETE (Phase 0)
        ↓
[Runtime Environment Setup]          ← **LECTURES 32-33** ← **NEXT: Phase 1**
  - Stack Frame Layout
  - Calling Conventions
  - Storage Allocation
        ↓
[MIPS Code Generator]                ← **LECTURES 34-35** ← **Phase 2**
  - Instruction Selection
  - Register Allocation (getReg)
  - Address/Register Descriptors
        ↓
[Code Optimization]                  ← **LECTURES 35-36** ← **Phase 4 (Optional)**
  - Peephole Optimization
  - Optimal Code for Expressions
        ↓
MIPS Assembly (.s/.asm)
        ↓
   [Simulator: SPIM/MARS/QtSpim]
        ↓
    Execution
```

### ⚠️ REMAINING CRITICAL STEPS (From Lectures 32-36)

#### **Phase 0: IR Analysis & Optimization Foundation (LECTURES 34-36)** ✅ **COMPLETED**

**Status:** Fully implemented and tested (November 10, 2025)

These steps come AFTER IR generation but BEFORE MIPS code generation:

##### **Step 0.1: Basic Block Partitioning (Lecture 34)** ✅ COMPLETE
**What:** Partition the IR code into basic blocks for analysis.

**Implementation Status:** ✅ DONE
- Implemented in `src/basic_block.cpp`
- Leader finding algorithm working correctly
- Handles FUNC_BEGIN, labels, jumps, and fall-through cases
- Tested on multiple test cases (8-30 blocks generated)

**Algorithm:**
1. **Find Leaders** (first statement of each basic block):
   - First IR instruction after `func_begin` is a leader
   - Any instruction that is a label target (e.g., `L0:`) is a leader  
   - Any instruction immediately following a `goto` or conditional branch is a leader

2. **Create Basic Blocks:**
   - Each leader starts a new basic block
   - Block extends from leader to (but not including) the next leader

**Why Essential:** Basic blocks are the foundation for:
- Flow graph construction
- Next-use analysis
- Register allocation
- All optimizations

##### **Step 0.2: Flow Graph Construction (Lecture 34)** ✅ COMPLETE
**What:** Build a directed graph showing control flow between basic blocks.

**Implementation Status:** ✅ DONE
- Implemented in `src/basic_block.cpp`
- Successors and predecessors tracked correctly
- Handles both fall-through and jump edges
- Control flow verified on if-else and loop test cases

**Algorithm:**
1. Each basic block becomes a node
2. Add edge from block B1 to B2 if:
   - B2 immediately follows B1 in execution order, OR
   - B1 ends with jump/branch to B2's label

**Why Essential:** Needed for:
- Understanding program structure
- Liveness analysis
- Global optimizations
- Dead code elimination

##### **Step 0.3: Next-Use Information (Lecture 34)** ✅ COMPLETE
**What:** For each variable at each point, determine if it will be "used again" later in the block.

**Implementation Status:** ✅ DONE
- Backward scan algorithm implemented
- Live/dead status tracked per variable per instruction
- Next-use indices computed correctly
- Critical for getReg algorithm in Phase 2

**Algorithm (Backward scan of each basic block):**
```
For each statement S: X = Y op Z (from end to beginning):
    1. Attach to S the current next-use info for X, Y, Z
    2. Mark X as "not live" and "no next use" in symbol table
    3. Mark Y and Z as "live" and set their next-use to S
```

**Example:**
```
a = 10           # a: live (used at line 3)
b = 3            # b: live (used at line 3)
t0 = a + b       # a, b: last use → can free their registers
                 # t0: live (used at line 4)
sum = t0         # t0: last use → can free its register
```

**Why CRITICAL:** The `getReg()` function (Lecture 35) absolutely requires this information to:
- Decide which registers to reuse
- Avoid spilling variables that are still needed
- Minimize memory traffic

##### **Step 0.4: Ershov Number Calculation (Lecture 36)** 🔧 OPTIONAL (For Optimization)
**What:** Calculate minimum registers needed for expression evaluation.

**Algorithm:**
```
For expression tree node N:
    If N is a leaf (variable/constant): label(N) = 1
    If N is unary (op child): label(N) = label(child)
    If N is binary (op left, right):
        If label(left) ≠ label(right):
            label(N) = max(label(left), label(right))
        Else:
            label(N) = label(left) + 1
```

**Why Useful:** Generates optimal code with minimal register usage, reducing spills.

---

### Phase 1: Runtime Environment & Storage Management (LECTURES 32-33) ⭐ ESSENTIAL

### Phase 1: Runtime Environment & Storage Management (LECTURES 32-33) ⭐ ESSENTIAL

This phase implements the runtime support required before code generation can begin.

#### Task 1.0: Storage Allocation Strategy Design (Lecture 32)
**What:** Decide how to allocate storage for different types of variables.

**Two Allocation Strategies (We're using):**

1. **Static Allocation** (Compile-time):
   - **Use for:** Global variables, static variables, string literals
   - **Location:** MIPS `.data` section
   - **Binding:** Names → addresses at compile time
   - **Lifetime:** Entire program execution
   - **MIPS directives:** `.word`, `.float`, `.asciiz`, `.space`

2. **Stack Allocation** (Runtime - Stack):
   - **Use for:** Local variables, function parameters, temporaries, return addresses
   - **Location:** MIPS stack (managed via `$sp`)
   - **Binding:** Names → offsets from frame pointer at compile time
   - **Lifetime:** Duration of function activation
   - **Management:** Push/pop activation records

#### Task 1.1: Activation Record (Stack Frame) Design (Lecture 32-33) ⭐ CRITICAL

**What:** Design the exact layout of each function's stack frame.

**Standard MIPS Stack Frame Layout:**
```
High Address
+------------------+  ← Previous $fp
| Argument 5       |  (args beyond 4 go on stack)
| Argument 6       |
| ...              |
+------------------+
| Saved $ra        |  +0($fp)  Return address
+------------------+
| Saved $fp        |  -4($fp)  Old frame pointer
+------------------+
| Saved $s0        |  -8($fp)  Callee-saved regs (if used)
| Saved $s1        |  -12($fp)
| ...              |
+------------------+
| Local var 1      |  Offset computed from symbol table
| Local var 2      |
| ...              |
+------------------+
| Temporary 1      |  Spilled registers
| Temporary 2      |
| ...              |
+------------------+  ← $sp (current stack pointer)
Low Address
```

**Implementation Steps:**
1. **Calculate Frame Size** for each function:
   ```
   frame_size = 8 (for $ra, $fp)
              + 4 * num_saved_registers
              + 4 * num_local_vars
              + 4 * max_temps_needed
              + 4 * max_args_beyond_4
   ```

2. **Compute Offsets** for each variable:
   - Store in a map: `variable_name → offset_from_$fp`
   - Example: Local variable `a` at `-16($fp)`

3. **Track Register Usage**:
   - Determine which `$s` registers are used (need saving)

#### Task 1.2: Calling Sequence Implementation (Lecture 33) ⭐ CRITICAL

**What:** Implement the exact sequence of steps for function calls and returns.

**CALLER's Responsibilities (before `jal`):**
```mips
# 1. Evaluate and pass arguments
lw $a0, offset_arg1($fp)    # First 4 args → $a0-$a3
lw $a1, offset_arg2($fp)
lw $t0, offset_arg5($fp)    # Args 5+ → push onto stack
sw $t0, 0($sp)
addi $sp, $sp, -4

# 2. Save caller-saved registers ($t0-$t9) if needed
# (Only if they contain live values needed after call)

# 3. Call function
jal function_name

# 4. Restore stack for arguments 5+ (if any)
addi $sp, $sp, 4

# 5. Copy return value
move $t0, $v0               # Return value is in $v0
sw $t0, offset_result($fp)
```

**CALLEE's Responsibilities (function prologue):**
```mips
function_name:
# 1. Allocate stack frame
addi $sp, $sp, -frame_size

# 2. Save $ra and $fp
sw $ra, frame_size-4($sp)
sw $fp, frame_size-8($sp)

# 3. Set up new frame pointer
addi $fp, $sp, frame_size

# 4. Save callee-saved registers ($s0-$s7) if used
sw $s0, -12($fp)
sw $s1, -16($fp)

# 5. Initialize local variables (if needed)
```

**CALLEE's Return Sequence (function epilogue):**
```mips
# 1. Place return value in $v0
lw $v0, offset_return_var($fp)

# 2. Restore callee-saved registers
lw $s0, -12($fp)
lw $s1, -16($fp)

# 3. Restore $fp and $ra
lw $ra, -4($fp)
lw $fp, -8($fp)

# 4. Deallocate stack frame
addi $sp, $sp, frame_size

# 5. Return to caller
jr $ra
```

#### Task 1.3: Parameter Passing Mechanisms (Lecture 33)

**What:** Implement how parameters are passed to functions.

**Two Methods:**

1. **Call-by-Value** (default for your compiler):
   - Pass the actual VALUE of the argument
   - Callee gets a COPY in its activation record
   - Changes don't affect caller's data
   - **Implementation:** Just load value and pass it

2. **Call-by-Reference** (for pointers/references):
   - Pass the ADDRESS of the argument
   - Callee can modify caller's data
   - **Implementation:** Pass address using `la` instruction

**MIPS Convention:**
- First 4 parameters: `$a0`, `$a1`, `$a2`, `$a3`
- Parameters 5+: Push onto stack in REVERSE order
- Return value: `$v0` (and `$v1` for 64-bit)

#### Task 1.4: Access to Non-Local Variables (Lecture 33)

**What:** Handle variable scoping and access.

**For C-like languages (no nested functions):**
- **Local variables:** Access via offset from `$fp`
- **Global variables:** Access via label in `.data` section
- **Static variables:** Access via label in `.data` section (with mangled name)

**Example:**
```mips
# Local variable access
lw $t0, -16($fp)        # Load local var at offset -16

# Global variable access
lw $t0, global_var      # Load global directly

# Static variable access (function-scoped)
lw $t0, main.static_var # Load static with function prefix
```

**If you have nested procedures (SKIP if not):**
- Use access links (display registers)
- Follow chain of frame pointers to find enclosing scope

---

### Phase 2: MIPS Code Generator Infrastructure (LECTURES 34-35) ⭐ CRITICAL
### Phase 2: MIPS Code Generator Infrastructure (LECTURES 34-35) ⭐ CRITICAL

#### Task 2.1: Create MIPS Code Generator Module
**Files to create:**
- `src/mips_codegen.h` - Interface for MIPS code generation
- `src/mips_codegen.cpp` - MIPS assembly generation implementation
- `src/basic_block.h` - Basic block analysis (Lecture 34)
- `src/basic_block.cpp` - Basic block implementation

**Key Components:**
```cpp
// Basic structure (following Lecture 34-35)
class MIPSCodeGenerator {
private:
    // Data Structures from Lectures
    vector<BasicBlock*> basicBlocks;           // Lecture 34
    map<string, int> registerDescriptor;       // Lecture 35: tracks what's in each register
    map<string, set<string>> addressDescriptor; // Lecture 35: tracks where each variable is
    map<string, int> nextUse;                  // Lecture 34: next-use information
    map<string, bool> liveVariable;            // Lecture 34: liveness info
    
    // Runtime Environment (Lectures 32-33)
    map<string, int> variableOffsets;          // Variable → offset from $fp
    map<string, int> frameSizes;               // Function → total frame size
    int currentFrameSize;
    
    // Register allocation
    set<int> freeRegisters;                    // Available $t registers
    map<string, int> varToRegister;            // Current allocation
    
    // Code generation
    ofstream outputFile;
    Symbol* symbolTable;
    int irCount;
    Quadruple* IR;
    
public:
    // Main entry point
    void generateMIPS(Quadruple IR[], int count, const char* filename);
    
    // Phase 0: IR Analysis (Lecture 34)
    void buildBasicBlocks();                   // Partition IR into basic blocks
    void buildFlowGraph();                     // Construct control flow graph
    void computeNextUse();                     // Backward scan for next-use info
    
    // Phase 1: Runtime Environment (Lectures 32-33)
    void computeActivationRecords();           // Calculate frame sizes and offsets
    int calculateFrameSize(string funcName);   // Compute total frame size
    void assignVariableOffsets(string funcName); // Assign offsets to locals
    
    // Phase 2: Code Generation (Lecture 34-35)
    void generateDataSection();                // Static allocation
    void generateTextSection();                // Code generation
    void generateFunction(string funcName, int start, int end);
    
    // Instruction Selection (Lecture 34)
    void translateInstruction(Quadruple& quad);
    string selectInstruction(string op, string arg1, string arg2, string result);
    
    // Register Allocation (Lecture 35 - getReg algorithm)
    int getReg(string var, Quadruple& quad);   // ⭐ THE CORE ALGORITHM
    void spillRegister(int reg);               // Handle register spilling
    void updateDescriptors(int reg, string var);
    
    // Function call sequences (Lecture 33)
    void generatePrologue(string funcName);    // Calling sequence
    void generateEpilogue(string funcName);    // Return sequence
    void generateCall(string funcName, int numArgs);
    
    // Helper functions
    bool isRegisterAvailable();
    int getEmptyRegister();
    int selectRegisterToSpill();
    string getLocation(string var);            // From address descriptor
    void loadVariable(string var, int reg);
    void storeVariable(string var, int reg);
};

// Basic Block structure (Lecture 34)
class BasicBlock {
public:
    int id;
    int startIndex;                            // First IR instruction
    int endIndex;                              // Last IR instruction
    vector<BasicBlock*> successors;            // Outgoing edges in flow graph
    vector<BasicBlock*> predecessors;          // Incoming edges
    map<string, int> nextUseInfo;              // Next-use at block entry
    set<string> liveVars;                      // Live variables at block entry
};
```

#### Task 2.2: Register Allocation with Descriptors (Lecture 35) ⭐⭐⭐ MOST CRITICAL

**What:** Implement the getReg() algorithm from Lecture 35.

**Data Structures (Lecture 35):**

1. **Register Descriptor:** Tracks what variable(s) are currently in each register
   ```cpp
   map<int, set<string>> regDescriptor;
   // Example: regDescriptor[$t0] = {"a", "temp1"}
   ```

2. **Address Descriptor:** Tracks all locations where a variable's current value can be found
   ```cpp
   map<string, set<string>> addrDescriptor;
   // Example: addrDescriptor["a"] = {"$t0", "memory"}
   ```

**The getReg(Y) Algorithm (Lecture 35):**
```cpp
int MIPSCodeGenerator::getReg(string Y, Quadruple& quad) {
    // Goal: Find a register to hold variable Y
    
    // CASE 1: Y is already in a register
    for (int r = T0; r <= T9; r++) {
        if (regDescriptor[r].count(Y) > 0) {
            return r;  // Best case - already loaded!
        }
    }
    
    // CASE 2: Find an empty register
    for (int r = T0; r <= T9; r++) {
        if (regDescriptor[r].empty()) {
            loadVariable(Y, r);
            return r;  // Good case - no spill needed
        }
    }
    
    // CASE 3: Must spill a register (Hard case)
    // Select victim register based on heuristics:
    for (int r = T0; r <= T9; r++) {
        bool canSpill = true;
        
        for (string V : regDescriptor[r]) {
            // Don't spill if:
            // 1. V is the target variable (X in X = Y op Z)
            if (V == quad.result) continue;
            
            // 2. V's value is already in memory (safe to overwrite)
            if (addrDescriptor[V].count("memory") > 0) {
                continue;
            }
            
            // 3. V is "dead" (no next use) - OK to discard
            if (!liveVariable[V] || nextUse[V] == -1) {
                continue;
            }
            
            // 4. Otherwise, must save V to memory
            canSpill = false;
            break;
        }
        
        if (canSpill) {
            spillRegister(r);  // Save if necessary
            loadVariable(Y, r);
            return r;
        }
    }
    
    // Worst case: Just pick $t0 and spill everything
    spillRegister(T0);
    loadVariable(Y, T0);
    return T0;
}

void MIPSCodeGenerator::spillRegister(int reg) {
    // Save all variables in this register to memory
    for (string V : regDescriptor[reg]) {
        // Only store if V is NOT already in memory
        if (addrDescriptor[V].count("memory") == 0) {
            emit("sw $t" + to_string(reg - T0) + ", " + getMemoryLocation(V));
            addrDescriptor[V].insert("memory");
        }
        addrDescriptor[V].erase("$t" + to_string(reg - T0));
    }
    regDescriptor[reg].clear();
}

void MIPSCodeGenerator::updateDescriptors(int reg, string var) {
    // Update after generating code
    regDescriptor[reg].insert(var);
    addrDescriptor[var].insert("$t" + to_string(reg - T0));
}
```

#### Task 2.3: Next-Use Analysis Implementation (Lecture 34) ⭐ CRITICAL

**What:** Compute next-use information for getReg to work properly.

**Algorithm (Backward Scan - Lecture 34):**
```cpp
void MIPSCodeGenerator::computeNextUse() {
    for (BasicBlock* block : basicBlocks) {
        // Initialize: all variables have no next use
        map<string, int> nextUseTable;
        map<string, bool> liveTable;
        
        // Scan block BACKWARDS from end to start
        for (int i = block->endIndex; i >= block->startIndex; i--) {
            Quadruple& quad = IR[i];
            
            // For instruction: result = arg1 op arg2
            
            // Step 1: Attach current next-use info to this statement
            nextUse[quad.arg1] = nextUseTable[quad.arg1];
            nextUse[quad.arg2] = nextUseTable[quad.arg2];
            liveVariable[quad.arg1] = liveTable[quad.arg1];
            liveVariable[quad.arg2] = liveTable[quad.arg2];
            
            // Step 2: Result is now "dead" (being assigned)
            nextUseTable[quad.result] = -1;  // No next use
            liveTable[quad.result] = false;
            
            // Step 3: Operands are "live" and have next use = here
            if (!quad.arg1.empty() && !isConstant(quad.arg1)) {
                nextUseTable[quad.arg1] = i;
                liveTable[quad.arg1] = true;
            }
            if (!quad.arg2.empty() && !isConstant(quad.arg2)) {
                nextUseTable[quad.arg2] = i;
                liveTable[quad.arg2] = true;
            }
        }
    }
}
```

#### Task 2.4: Stack Frame Management (Lectures 32-33)

#### Task 2.4: Stack Frame Management (Lectures 32-33)

**What:** Compute exact memory layout for each function.

```cpp
void MIPSCodeGenerator::computeActivationRecords() {
    for (each function in IR) {
        string funcName = extractFunctionName();
        
        // Count locals from symbol table
        int numLocals = countLocalVariables(funcName);
        
        // Count maximum temporaries needed (from IR analysis)
        int maxTemps = countTemporaries(funcName);
        
        // Count callee-saved registers used
        int numSavedRegs = countSavedRegistersUsed(funcName);
        
        // Calculate frame size
        frameSizes[funcName] = 8                    // $ra + $fp
                             + 4 * numSavedRegs     // Saved $s registers
                             + 4 * numLocals        // Local variables
                             + 4 * maxTemps;        // Spilled temps
        
        // Assign offsets
        assignVariableOffsets(funcName);
    }
}

void MIPSCodeGenerator::assignVariableOffsets(string funcName) {
    int offset = -12;  // Start after $ra(-4) and $fp(-8)
    
    // Assign offsets to local variables
    for (Symbol& sym : symbolTable) {
        if (sym.function_scope == funcName && !sym.is_static) {
            variableOffsets[sym.name] = offset;
            offset -= sym.size;  // Move down in stack
        }
    }
    
    // Temporaries get offsets dynamically as needed
}
```

---

### Phase 3: Data Section Generation (Priority: HIGH)

#### Task 2.1: Generate .data Section
Map IR data declarations to MIPS:

```assembly
.data
    # Global/static integers
    global_var: .word 42
    
    # Floats
    float_var: .float 3.14
    
    # Strings
    str1: .asciiz "Hello, World\n"
    
    # Arrays
    array1: .word 0:10     # int array[10]
    array2: .space 40      # 10 ints * 4 bytes
```

**Process:**
1. Scan IR for global assignments before first FUNC_BEGIN
2. Extract static variables from `staticVars[]` array
3. Collect string literals from PARAM instructions
4. Generate appropriate MIPS directives

---

### Phase 3: Data Section Generation (Static Allocation - Lecture 32)

#### Task 3.1: Generate .data Section
Map IR data declarations to MIPS (following **Static Allocation** from Lecture 32):

```assembly
.data
    # Global/static integers (Static allocation)
    global_var: .word 42
    
    # Floats
    float_var: .float 3.14
    
    # Strings (Static allocation for literals)
    str1: .asciiz "Hello, World\n"
    str_newline: .asciiz "\n"
    
    # Arrays (Static allocation)
    array1: .word 0:10     # int array[10]
    array2: .space 40      # 10 ints * 4 bytes
```

**Process:**
1. Scan IR for global assignments before first FUNC_BEGIN
2. Extract static variables from `staticVars[]` array
3. Collect string literals from PARAM instructions
4. Generate appropriate MIPS directives

---

### Phase 4: Instruction Translation (Lecture 35)

### Phase 4: Instruction Translation (Lecture 35)

**Following the Simple Code Generation Algorithm from Lecture 35:**

#### Algorithm for Translating: `X = Y op Z`

```cpp
void MIPSCodeGenerator::translateInstruction(Quadruple& quad) {
    // Following Lecture 35 algorithm
    
    // Step 1: Call getReg to select registers
    int Rx = getReg(quad.result, quad);  // Register for X
    int Ry = getReg(quad.arg1, quad);    // Register for Y  
    int Rz = getReg(quad.arg2, quad);    // Register for Z
    
    // Step 2: Generate load instructions if needed
    if (addrDescriptor[quad.arg1].count("$t" + to_string(Ry - T0)) == 0) {
        // Y not in Ry yet
        emit("lw $t" + to_string(Ry - T0) + ", " + getLocation(quad.arg1));
        updateDescriptors(Ry, quad.arg1);
    }
    
    if (!quad.arg2.empty() && 
        addrDescriptor[quad.arg2].count("$t" + to_string(Rz - T0)) == 0) {
        // Z not in Rz yet
        emit("lw $t" + to_string(Rz - T0) + ", " + getLocation(quad.arg2));
        updateDescriptors(Rz, quad.arg2);
    }
    
    // Step 3: Generate the operation
    string op = selectMIPSInstruction(quad.op);
    emit(op + " $t" + to_string(Rx - T0) + ", $t" + to_string(Ry - T0) + 
         ", $t" + to_string(Rz - T0));
    
    // Step 4: Update descriptors
    updateDescriptors(Rx, quad.result);
}
```

#### Task 4.1: Arithmetic Operations
```
IR: t0 = a + b
MIPS:
    lw $t0, <offset_a>($fp)   # Load a
    lw $t1, <offset_b>($fp)   # Load b
    add $t2, $t0, $t1         # Add
    sw $t2, <offset_t0>($fp)  # Store t0

IR: t0 = a + 5
MIPS:
    lw $t0, <offset_a>($fp)
    addi $t1, $t0, 5
    sw $t1, <offset_t0>($fp)
```

**Operations to implement:**
- `ADD` → `add/addi`
- `SUB` → `sub/subi`
- `MUL` → `mul` (pseudo-instruction) or `mult + mflo`
- `DIV` → `div + mflo` (quotient)
- `MOD` → `div + mfhi` (remainder)
- `NEG` → `neg` or `sub $t0, $zero, $t1`

#### Task 3.2: Logical & Bitwise Operations
```
IR: t0 = a & b
MIPS:
    lw $t0, <offset_a>($fp)
    lw $t1, <offset_b>($fp)
    and $t2, $t0, $t1
    sw $t2, <offset_t0>($fp)
```

**Operations:**
- `BITAND` → `and/andi`
- `BITOR` → `or/ori`
- `BITXOR` → `xor/xori`
- `BITNOT` → `nor $t0, $t1, $zero` (NOT using NOR)
- `LSHIFT` → `sll/sllv`
- `RSHIFT` → `srl/srlv`
- `NOT` → `seq $t0, $t1, $zero` (logical NOT: 1 if 0, 0 otherwise)

#### Task 3.3: Relational Operations
```
IR: t0 = a < b
MIPS:
    lw $t0, <offset_a>($fp)
    lw $t1, <offset_b>($fp)
    slt $t2, $t0, $t1         # Set if less than
    sw $t2, <offset_t0>($fp)
```

**Operations:**
- `LT` → `slt` (set less than)
- `GT` → `slt` with reversed operands, or use `sgt` (pseudo)
- `LE` → `!(a > b)` = `slt` + logic
- `GE` → `!(a < b)` = `slt` + logic
- `EQ` → `seq` (pseudo) or `sub + seq`
- `NE` → `sne` (pseudo) or `sub + sne`

#### Task 3.4: Assignments
```
IR: a = 10
MIPS:
    li $t0, 10
    sw $t0, <offset_a>($fp)

IR: a = b
MIPS:
    lw $t0, <offset_b>($fp)
    sw $t0, <offset_a>($fp)
```

---

### Phase 4: Control Flow (Priority: HIGH)

#### Task 4.1: Labels & Unconditional Jumps
```
IR: L0:
MIPS: L0:

IR: goto L0
MIPS: j L0
```

#### Task 4.2: Conditional Branches
```
IR: if t0 == 0 goto L0
MIPS:
    lw $t0, <offset_t0>($fp)
    beq $t0, $zero, L0

IR: if t0 != 0 goto L1
MIPS:
    lw $t0, <offset_t0>($fp)
    bne $t0, $zero, L1
```

**Branch instructions:**
- `beq` - branch if equal
- `bne` - branch if not equal
- `blt` - branch if less than (pseudo)
- `bgt` - branch if greater than (pseudo)
- `ble` - branch if less than or equal (pseudo)
- `bge` - branch if greater than or equal (pseudo)

---

### Phase 5: Function Calls (Priority: HIGH)

#### Task 5.1: Function Prologue
```
IR: func_begin main
MIPS:
main:
    # Save return address
    addi $sp, $sp, -<frame_size>
    sw $ra, <frame_size-4>($sp)
    sw $fp, <frame_size-8>($sp)
    
    # Set frame pointer
    addi $fp, $sp, <frame_size>
    
    # Save callee-saved registers if used
    # sw $s0, -12($fp)
    # ...
```

#### Task 5.2: Parameter Passing
```
IR: 
    param arg1
    param arg2
    t0 = call func, 2
    
MIPS:
    # Load arg1 into $a0
    lw $a0, <offset_arg1>($fp)
    
    # Load arg2 into $a1
    lw $a1, <offset_arg2>($fp)
    
    # Call function
    jal func
    
    # Get return value
    sw $v0, <offset_t0>($fp)
```

**Calling convention:**
1. First 4 args in $a0-$a3
2. Additional args on stack (push in reverse order)
3. Caller saves $t0-$t9 if needed
4. Callee saves $s0-$s7 if used
5. Return value in $v0 (and $v1 for 64-bit)

#### Task 5.3: Function Epilogue & Return
```
IR: return t0
MIPS:
    # Load return value into $v0
    lw $v0, <offset_t0>($fp)
    
    # Restore registers
    lw $fp, -8($fp)
    lw $ra, -4($fp)
    
    # Deallocate stack frame
    addi $sp, $sp, <frame_size>
    
    # Return
    jr $ra
```

---

### Phase 6: Arrays & Pointers (Priority: MEDIUM)

#### Task 6.1: Array Access
```
IR: t0 = arr[i]
MIPS:
    # Load index i
    lw $t0, <offset_i>($fp)
    
    # Calculate offset: i * 4 (for int)
    sll $t0, $t0, 2           # Multiply by 4 (shift left 2)
    
    # Get array base address
    la $t1, <offset_arr>($fp) # Or load if pointer
    
    # Add offset
    add $t2, $t1, $t0
    
    # Load value
    lw $t3, 0($t2)
    
    # Store result
    sw $t3, <offset_t0>($fp)
```

#### Task 6.2: Pointer Operations
```
IR: t0 = &a
MIPS:
    la $t0, <offset_a>($fp)   # Load address
    sw $t0, <offset_t0>($fp)

IR: t0 = *ptr
MIPS:
    lw $t0, <offset_ptr>($fp) # Load pointer
    lw $t1, 0($t0)            # Dereference
    sw $t1, <offset_t0>($fp)

IR: *ptr = a
MIPS:
    lw $t0, <offset_a>($fp)   # Load value
    lw $t1, <offset_ptr>($fp) # Load pointer
    sw $t0, 0($t1)            # Store at pointer
```

---

### Phase 7: Structures (Priority: MEDIUM)

#### Task 7.1: Structure Member Access
```
IR: t0 = s.member
MIPS:
    # Calculate member offset (from symbol table)
    # offset = base_offset + member_offset
    la $t0, <offset_s>($fp)
    lw $t1, <member_offset>($t0)
    sw $t1, <offset_t0>($fp)
```

---

### Phase 8: I/O Support (Priority: HIGH)

#### Task 8.1: Printf Implementation
```
IR:
    param "Value: %d\n"
    param 42
    t0 = call printf, 2
```

**Approach 1: Simple syscalls**
```mips
# Print integer
li $v0, 1         # syscall code for print_int
lw $a0, <value>   # Load integer
syscall

# Print string
li $v0, 4         # syscall code for print_string
la $a0, str_label # Load string address
syscall

# Print newline
li $v0, 4
la $a0, newline
syscall
```

**Approach 2: Parse format string**
- Scan format string for %d, %f, %s, %c
- Generate appropriate syscalls for each
- More complex but more accurate

#### Task 8.2: Scanf Implementation
```mips
# Read integer
li $v0, 5         # syscall code for read_int
syscall
sw $v0, <offset>  # Store result
```

---

### Phase 9: Code Optimization (LECTURES 35-36) 🔧 OPTIONAL BUT RECOMMENDED

After basic code generation works, apply these optimizations:

#### Task 9.1: Peephole Optimization (Lecture 36) ⭐ EASY WINS

**What:** Examine a small "sliding window" of generated instructions and replace with better sequences.

**Algorithm (Lecture 36):**
```cpp
void MIPSCodeGenerator::peepholeOptimize() {
    // Scan generated MIPS code with a small window (2-4 instructions)
    for (int i = 0; i < instructions.size() - 1; i++) {
        // Pattern 1: Redundant Load/Store
        if (instructions[i].opcode == "sw" && 
            instructions[i+1].opcode == "lw" &&
            instructions[i].reg == instructions[i+1].reg &&
            instructions[i].addr == instructions[i+1].addr) {
            // Remove the redundant load
            instructions.erase(instructions.begin() + i + 1);
        }
        
        // Pattern 2: Strength Reduction
        if (instructions[i].opcode == "mul" && 
            instructions[i].src2 == "2") {
            // Replace: mul $t0, $t1, 2
            // With:    sll $t0, $t1, 1  (left shift is faster)
            instructions[i].opcode = "sll";
            instructions[i].src2 = "1";
        }
        
        // Pattern 3: Algebraic Simplification
        if (instructions[i].opcode == "add" && 
            instructions[i].src2 == "0") {
            // Replace: add $t0, $t1, 0
            // With:    move $t0, $t1
            instructions[i].opcode = "move";
            instructions[i].src2 = "";
        }
        
        // Pattern 4: Redundant Jumps
        if (instructions[i].opcode == "j" && 
            instructions[i+1].isLabel &&
            instructions[i+1].label == instructions[i+2].label) {
            // Replace: j L1
            //          L1: j L2
            // With:    j L2
            instructions[i].target = instructions[i+2].target;
            instructions.erase(instructions.begin() + i + 1);
        }
    }
}
```

**Common Peephole Patterns (Lecture 36):**
1. **Redundant Load/Store:** `sw $t0, x` followed by `lw $t0, x` → eliminate `lw`
2. **Strength Reduction:**
   - `mul $t0, $t1, 2` → `sll $t0, $t1, 1` (shift is faster)
   - `div $t0, $t1, 2` → `sra $t0, $t1, 1`
3. **Algebraic Simplification:**
   - `add $t0, $t1, 0` → `move $t0, $t1`
   - `mul $t0, $t1, 1` → `move $t0, $t1`
   - `mul $t0, $t1, 0` → `li $t0, 0`
4. **Redundant Jumps:**
   - `j L1` + `L1: j L2` → `j L2`
5. **Dead Code:** Instructions that define variables never used again

#### Task 9.2: Optimal Code for Expressions (Lecture 36) 🔧 ADVANCED

**What:** Use Ershov numbers to generate optimal code with minimal register usage.

**When You Have Enough Registers (from Lecture 36):**
```cpp
void generateOptimalCode(TreeNode* node, int base) {
    if (node->isLeaf()) {
        emit("lw $t" + to_string(base) + ", " + node->value);
        return;
    }
    
    TreeNode* left = node->left;
    TreeNode* right = node->right;
    int L = left->ershov;
    int R = right->ershov;
    
    if (L != R) {
        // Evaluate "big child" first
        if (L > R) {
            generateOptimalCode(left, base);
            generateOptimalCode(right, base + L);
            emit(node->op + " $t" + to_string(base) + ", $t" + to_string(base) + 
                 ", $t" + to_string(base + L));
        } else {
            generateOptimalCode(right, base);
            generateOptimalCode(left, base + R);
            emit(node->op + " $t" + to_string(base) + ", $t" + to_string(base + R) + 
                 ", $t" + to_string(base));
        }
    } else {
        // L == R: Evaluate right first
        generateOptimalCode(right, base + 1);
        generateOptimalCode(left, base);
        emit(node->op + " $t" + to_string(base) + ", $t" + to_string(base) + 
             ", $t" + to_string(base + 1));
    }
}
```

**When Registers Are Insufficient (k > r):**
```cpp
void generateWithSpill(TreeNode* node, int numRegs) {
    int k = node->ershov;  // Registers needed
    int r = numRegs;       // Registers available
    
    if (k <= r) {
        generateOptimalCode(node, 0);
        return;
    }
    
    // Must spill (Lecture 36 algorithm)
    TreeNode* bigChild = (node->left->ershov > node->right->ershov) ? 
                         node->left : node->right;
    TreeNode* littleChild = (bigChild == node->left) ? node->right : node->left;
    
    // 1. Evaluate big child using all r registers
    generateWithSpill(bigChild, r);
    
    // 2. Spill result to temporary location
    string temp = newTemp();
    emit("sw $t" + to_string(r-1) + ", " + temp);
    
    // 3. Evaluate little child using all r registers
    generateWithSpill(littleChild, r);
    
    // 4. Load spilled value back
    emit("lw $t" + to_string(r-2) + ", " + temp);
    
    // 5. Perform operation
    emit(node->op + " $t" + to_string(r-1) + ", $t" + to_string(r-2) + 
         ", $t" + to_string(r-1));
}
```

#### Task 9.3: Local Optimizations (Lecture 36)

Apply these within basic blocks:
- **Constant Folding:** `t0 = 2 + 3` → `t0 = 5`
- **Copy Propagation:** `a = b; c = a + 1` → `c = b + 1`
- **Dead Code Elimination:** Remove assignments to variables never used
- **Common Subexpression Elimination:** Reuse previously computed values

**Note:** These are OPTIONAL. Focus on getting correct code first, then optimize.

---

### Phase 10: Floating Point (Priority: LOW - Optional)

MIPS has separate floating-point registers ($f0-$f31) and coprocessor 1 instructions:
- `add.s`, `sub.s`, `mul.s`, `div.s` for single precision
- `add.d`, `sub.d`, `mul.d`, `div.d` for double precision
- `lwc1`, `swc1` for load/store

**Note:** This is complex. You may choose to:
1. Implement full FP support
2. Convert floats to integers
3. Skip floating point tests
4. Emit error for float operations

---

### Phase 10: Integration & Testing (Priority: HIGH)

#### Task 10.1: Update Build System

**Makefile changes:**
```makefile
TARGET = compiler  # Or mips_compiler

CPP_SOURCES = ... $(SRC_DIR)/mips_codegen.cpp

$(OBJ_DIR)/mips_codegen.o: $(SRC_DIR)/mips_codegen.cpp $(SRC_DIR)/mips_codegen.h
    $(CC) $(CFLAGS) -c $< -o $@
```

#### Task 10.2: Update Main Driver

**main.cpp changes:**
```cpp
#include "mips_codegen.h"

// After IR generation
if (ast_root) {
    generate_ir(ast_root);
    printIR(ir_output_file);
    
    // NEW: Generate MIPS assembly
    string asm_output_file = inputFile.substr(0, lastDot) + ".s";
    generate_mips(IR, irCount, asm_output_file.c_str());
}
```

#### Task 10.3: Update run.sh

```bash
#!/bin/bash
make clean && make

for test_file in test/*.txt; do
    echo "Testing: $test_file"
    ./compiler "$test_file"
    
    # Optional: Run in SPIM/MARS
    asm_file="${test_file%.txt}.s"
    if command -v spim &> /dev/null; then
        spim -file "$asm_file"
    fi
done
```

#### Task 10.4: Create README.md

Include:
- Project description
- Supported features
- Build instructions: `make`
- Usage: `./compiler input.txt`
- Output: `input.s` (MIPS assembly)
- Simulator: SPIM/MARS/QtSpim
- Test execution: `./run.sh`
- Example usage

---

## Implementation Priority & Timeline (Based on Lectures 32-36)

### Week 1 (Nov 9-14): Foundation & Analysis
**Priority: CRITICAL - Must Complete**

1. ✅ **Verify IR generator** - DONE
2. **Phase 0: IR Analysis (Lecture 34)** ⭐⭐⭐
   - Implement basic block partitioning
   - Build flow graph
   - Compute next-use information
   - **Time:** 6-8 hours
   - **Why Critical:** Required for getReg to work

3. **Phase 1: Runtime Environment (Lectures 32-33)** ⭐⭐⭐
   - Design activation record layout
   - Implement calling/return sequences
   - Calculate stack frame sizes and offsets
   - **Time:** 8-10 hours
   - **Why Critical:** Foundation for all code generation

4. **Phase 2.1-2.2: Code Generator Infrastructure** ⭐⭐
   - Create MIPSCodeGenerator class
   - Implement register/address descriptors
   - Implement getReg() algorithm
   - **Time:** 8-10 hours
   - **Why Critical:** Core of code generation

### Week 2 (Nov 15-21): Code Generation
**Priority: HIGH - Core Features**

5. **Phase 2.3-2.4: Stack Management**
   - Compute activation records
   - Assign variable offsets
   - **Time:** 4-6 hours

6. **Phase 3: Data Section (Static Allocation)**
   - Generate .data section
   - Handle globals, statics, strings
   - **Time:** 3-4 hours

7. **Phase 4: Basic Instruction Translation (Lecture 35)**
   - Arithmetic operations (add, sub, mul, div, mod)
   - Assignment operations
   - **Time:** 6-8 hours
   - **Test:** Simple arithmetic program

8. **Phase 5: Control Flow**
   - Labels and unconditional jumps
   - Conditional branches
   - **Time:** 4-6 hours
   - **Test:** If-else, loops

9. **Phase 6: Function Calls (Lecture 33)**
   - Function prologue/epilogue
   - Parameter passing
   - Return values
   - **Time:** 8-10 hours
   - **Test:** Function calls, recursion

10. **Phase 4.2-4.3: Logical/Relational Ops**
    - Bitwise operations
    - Relational comparisons
    - **Time:** 4-6 hours

11. **Phase 8: I/O Support**
    - Printf syscalls
    - Scanf syscalls
    - **Time:** 4-6 hours
    - **Test:** Hello World, input/output

### Week 3 (Nov 22-23): Polish & Optimization
**Priority: MEDIUM - Finishing Touches**

12. **Phase 7: Arrays & Pointers**
    - Array indexing
    - Pointer operations
    - **Time:** 6-8 hours
    - **Test:** Array manipulation

13. **Phase 9.1: Peephole Optimization (Lecture 36)** 🔧
    - Redundant load/store elimination
    - Strength reduction
    - Algebraic simplification
    - **Time:** 4-6 hours
    - **Why Useful:** Easy performance wins

14. **Phase 11: Integration & Testing**
    - Update makefile
    - Update main.cpp
    - Update run.sh
    - **Time:** 4-6 hours

15. **Comprehensive Testing**
    - Test all 18 test cases
    - Run in SPIM/MARS
    - Fix bugs
    - **Time:** 8-12 hours

16. **Phase 11.4: Documentation**
    - Create README.md
    - Document features
    - Usage instructions
    - **Time:** 2-3 hours

17. **Final Submission**
    - Code cleanup
    - Final testing
    - Package deliverables
    - **Time:** 2-3 hours

---

## CRITICAL PATH (Minimum Viable Compiler)

### Progress Tracking:

### ✅ Completed (November 10, 2025):
1. ✅ IR Generator - **COMPLETE**
2. ✅ Basic block analysis (Lecture 34) - **COMPLETE** (~6 hours)
3. ✅ Next-use information (Lecture 34) - **COMPLETE** (included in #2)
4. ✅ Flow graph construction - **COMPLETE** (included in #2)

### 🔄 Next Steps (Priority Order):

**Must Have (70% functionality) - Remaining:**
4. **Activation records** (Lectures 32-33) - 6 hours
5. **getReg algorithm** (Lecture 35) - 8 hours
6. **Data section** - 3 hours
7. **Simple arithmetic** - 4 hours
8. **Assignments** - 2 hours
9. **Printf (basic)** - 3 hours
10. **Labels & jumps** - 2 hours
11. **Function calls** - 8 hours

**Total: ~46 hours** → Doable in 2 weeks with 3-4 hours/day

### Should Have (90% functionality):
12. Conditional branches - 4 hours
13. Logical/bitwise ops - 4 hours
14. Relational ops - 3 hours
15. Arrays - 6 hours

### Nice to Have (100% functionality):
16. Pointers - 4 hours
17. Peephole optimization - 4 hours
18. Structures - 6 hours

---

## Key Success Factors (From Lectures)

1. ✅ **IR is complete** - You have a solid foundation
2. **Understand Lectures 32-36** - These are your blueprint:
   - **Lecture 32-33:** Runtime environment (MUST understand)
   - **Lecture 34:** Basic blocks, next-use (CRITICAL for register allocation)
   - **Lecture 35:** getReg algorithm (THE CORE ALGORITHM)
   - **Lecture 36:** Optimizations (nice to have)
3. **Follow the algorithms exactly** - Don't reinvent the wheel
4. **Implement next-use analysis** - getReg won't work without it
5. **Test incrementally** - After each phase, test with simple programs
6. **Use MARS debugger** - Visualize what's happening
7. **Start with integer-only** - Skip floating point initially

---

## Testing Strategy

1. **Unit Testing**: Test each MIPS generation function individually
2. **Integration Testing**: Full pipeline for simple programs
3. **Regression Testing**: All 18 test cases
4. **Simulator Testing**: Run in SPIM/MARS, verify output
5. **Edge Cases**: Empty functions, complex expressions, nested calls

---

## Recommended MIPS Simulator

**Option 1: SPIM** (Command-line)
- Lightweight, widely available
- Good for automated testing
- `sudo apt install spim` or download from CS Wisconsin

**Option 2: MARS** (GUI)
- Java-based, cross-platform
- Excellent debugging features
- Download JAR from Missouri State University

**Option 3: QtSpim** (GUI)
- Modern Qt-based interface
- Good visualization
- Available in most Linux repos

---

## Key Success Factors

1. ✅ **IR is complete** - You have a solid foundation
2. **Focus on correctness over optimization** - Get it working first
3. **Start simple** - Implement arithmetic, then control flow, then functions
4. **Test incrementally** - Don't wait until everything is done
5. **Use MARS debugger** - Visualize register/memory states
6. **Document assumptions** - Especially for printf/scanf handling

---

## Expected Deliverables Checklist

- [ ] `src/` directory with MIPS code generator files
- [ ] `test/` directory with ≥5 test cases (.txt and .s files)
- [ ] `makefile` that compiles to executable
- [ ] `run.sh` that processes all tests
- [ ] `README.md` with clear instructions
- [ ] Working compiler: `./compiler input.txt` → `input.s`
- [ ] Generated assembly runs in SPIM/MARS/QtSpim

---

## Additional Resources

- **MIPS Reference**: [Stanford CS107 MIPS Guide](https://web.stanford.edu/class/cs107/guide/)
- **MIPS Instruction Set**: [MIPS Green Sheet](https://inst.eecs.berkeley.edu/~cs61c/resources/MIPS_Green_Sheet.pdf)
- **Calling Convention**: [MIPS Calling Convention](https://courses.cs.washington.edu/courses/cse410/09sp/examples/MIPSCallingConventionsSummary.pdf)
- **SPIM Documentation**: [SPIM Manual](http://spimsimulator.sourceforge.net/)

---

## Summary

**Current Status**: ✅ IR Generator COMPLETE (~75% of compiler)

**Remaining Work**: 
1. **IR Analysis (Lecture 34)** - 10% but CRITICAL
2. **Runtime Environment (Lectures 32-33)** - 5% but ESSENTIAL  
3. **MIPS Code Generator (Lecture 35)** - 10% but THE CORE
4. **Optimizations (Lecture 36)** - Optional

**Estimated Effort**: 
- **Minimum (working compiler):** 46 hours over 2 weeks
- **Complete (with optimizations):** 60-70 hours

**Risk Level**: MEDIUM-HIGH (tight timeline, complex algorithms)

**CRITICAL REQUIREMENTS FROM LECTURES:**

1. **Basic Block Analysis (Lecture 34)** ⭐⭐⭐
   - Partition IR into basic blocks
   - Build control flow graph
   - **Why:** Foundation for all analysis

2. **Next-Use Information (Lecture 34)** ⭐⭐⭐
   - Backward scan of basic blocks
   - Track which variables are live
   - **Why:** getReg() algorithm REQUIRES this

3. **Activation Records (Lectures 32-33)** ⭐⭐⭐
   - Stack frame layout
   - Calling/return sequences
   - Parameter passing
   - **Why:** Code won't work without proper runtime environment

4. **getReg Algorithm (Lecture 35)** ⭐⭐⭐⭐⭐
   - Register/address descriptors
   - Smart register allocation
   - Spilling when necessary
   - **Why:** THE MOST IMPORTANT ALGORITHM - determines code quality

5. **Peephole Optimization (Lecture 36)** ⭐
   - Easy performance improvements
   - Low effort, high value
   - **Why:** Shows you understand optimization

**Recommendation (Following Lecture Order):**

**Phase 0 (MUST DO FIRST):** ✅ **COMPLETED - November 10, 2025**
1. ✅ Implement basic block partitioning (Lecture 34)
2. ✅ Build flow graph (Lecture 34)
3. ✅ Compute next-use information (Lecture 34)

**Phase 1 (ESSENTIAL) - NEXT:**
4. Design activation records (Lectures 32-33)
5. Implement calling conventions (Lecture 33)
6. Calculate stack frame sizes and offsets

**Phase 2 (CORE CODE GENERATION):**
7. Implement register/address descriptors (Lecture 35)
8. Implement getReg() algorithm (Lecture 35)
9. Translate simple instructions (Lecture 35)
10. Generate data section (static allocation from Lecture 32)

**Phase 3 (COMPLETE THE COMPILER):**
11. Control flow (branches, jumps)
12. Function calls (prologue, epilogue, parameters)
13. I/O support (syscalls for printf/scanf)
14. Arrays and pointers

**Phase 4 (POLISH - OPTIONAL):**
15. Peephole optimization (Lecture 36)
16. Optimal expression code (Ershov numbers - Lecture 36)

**What Makes This Different from Generic MIPS Generator:**
- ✅ Uses **basic block analysis** (not just line-by-line translation) - **DONE**
- ✅ Uses **next-use information** (smart register allocation) - **DONE**
- ⏳ Implements **getReg algorithm** (follows Lecture 35 exactly) - **NEXT**
- ⏳ Uses **register/address descriptors** (proper tracking) - **NEXT**
- ⏳ Implements **proper calling conventions** (follows Lecture 33) - **NEXT**
- 🔧 Optional: **Peephole optimization** (Lecture 36 techniques)

**Common Mistakes to Avoid:**
1. ✅ ~~Skipping next-use analysis~~ → **DONE - We implemented it!**
2. ❌ Ignoring activation record design → runtime errors - **Phase 1 Focus**
3. ❌ Not using descriptors → inefficient register usage - **Phase 2 Focus**
4. ❌ Wrong calling convention → function calls fail - **Phase 1 Focus**
5. ❌ Forgetting to spill registers → data corruption - **Phase 2 Focus**

**Testing Strategy (Incremental):**
1. ✅ Test basic blocks on simple programs first - **VERIFIED**
2. Test arithmetic with 2-3 registers (verify spilling works)
3. Test function calls with simple functions
4. Test recursion (factorial, fibonacci)
5. Test arrays with simple indexing
6. Run all 18 test cases only after basics work

---

## 📋 NEXT SESSION PLAN: Phase 1 - Runtime Environment (6-8 hours)

**Goal:** Implement stack frame management and calling conventions (Lectures 32-33)

**Files to Create/Modify:**
1. `src/mips_codegen.cpp` - Start implementation with Phase 1 functions
2. Update `makefile` - Add mips_codegen.cpp compilation

**What to Implement:**

### Task 1.1: Activation Record Design (3 hours)
- [ ] Design MIPS stack frame layout
- [ ] Calculate frame sizes for each function
- [ ] Compute variable offsets from $fp
- [ ] Track saved registers ($s0-$s7)

### Task 1.2: Variable Offset Assignment (2 hours)
- [ ] Scan IR for local variables per function
- [ ] Assign stack offsets to locals
- [ ] Handle parameters (first 4 in registers, rest on stack)
- [ ] Reserve space for temporaries

### Task 1.3: Frame Size Calculation (1 hour)
- [ ] Count local variables
- [ ] Count maximum temporaries needed
- [ ] Add space for saved $ra, $fp
- [ ] Add space for saved $s registers

### Task 1.4: Test Framework (2 hours)
- [ ] Add function to print activation record info
- [ ] Test on simple single-function programs
- [ ] Verify offset calculations
- [ ] Document frame layout

**Expected Output After Phase 1:**
```
Function: main
  Frame Size: 48 bytes
  Variables:
    - $ra at offset -4
    - $fp at offset -8
    - a at offset -12
    - b at offset -16
    - t0 at offset -20
    ...
```

**Success Criteria:**
- ✅ Correct frame size calculation
- ✅ Proper variable offset assignment
- ✅ Handles functions with different numbers of locals
- ✅ Accounts for parameters correctly

---

## 🎯 Updated Timeline (13 days remaining)

**Week 1 (Nov 10-16):**
- ✅ Day 1 (Nov 10): Phase 0 Complete - Basic Block Analysis
- Day 2 (Nov 11): Phase 1 - Activation Records & Stack Frames
- Day 3 (Nov 12): Phase 2 Start - Register Descriptors & getReg
- Day 4 (Nov 13): Phase 2 Continue - Instruction Translation (Arithmetic)
- Day 5 (Nov 14): Phase 3 - Control Flow & Branches
- Day 6 (Nov 15): Phase 3 - Function Calls (Prologue/Epilogue)
- Day 7 (Nov 16): Phase 3 - Data Section & I/O

**Week 2 (Nov 17-23):**
- Day 8-9 (Nov 17-18): Complete all instruction types
- Day 10-11 (Nov 19-20): Integration testing & bug fixes
- Day 12 (Nov 21): Arrays, pointers, advanced features
- Day 13 (Nov 22): Final testing, optimization, documentation
- **Day 14 (Nov 23): SUBMISSION**

**You're in a strong position!** Phase 0 is complete. The foundation is solid!
2. Implement the runtime environment (activation records)
3. Follow the getReg algorithm from Lecture 35
4. Generate MIPS following the patterns in lectures

The lectures give you the EXACT algorithms to use. Don't overthink it - just implement what's in Lectures 32-36 and you'll have a working compiler that matches what your professor expects!
