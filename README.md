# C Compiler - MIPS Code Generator

A comprehensive compiler implementation that translates C source code into Three-Address Code (TAC) intermediate representation and generates MIPS assembly code. Built using Flex (lexer) and Bison (parser) with a modular C++ architecture.

## Features

### Language Support
- **Data Types**: `int`, `float`, `char`, `bool`, `void`
- **Type Modifiers**: `static`, `const`, `typedef`
- **Pointers**: Single and multi-level pointers
- **Arrays**: Single and multi-dimensional arrays
- **Structures**: User-defined struct types
- **References**: C++ style references (`&`)
- **Enumerations**: Enum type definitions

### Control Flow
- **Conditional Statements**: `if`, `else if`, `else`
- **Loops**: `for`, `while`, `do-while`, custom `until` loop
- **Switch-Case**: Full switch-case statement support
- **Jump Statements**: `break`, `continue`, `goto`, `return`

### Functions
- Function declarations and definitions
- Function calls with arguments
- Recursive function calls
- Variable argument functions
- Function pointers
- Static functions

### Operators
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`, `++`, `--`
- **Logical**: `&&`, `||`, `!`
- **Relational**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Bitwise**: `&`, `|`, `^`, `~`, `<<`, `>>`
- **Assignment**: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, etc.
- **Member Access**: `.`, `->`
- **Pointer Operations**: `*` (dereference), `&` (address-of)

### I/O Operations
- `printf()` - formatted output
- `scanf()` - formatted input
- Command-line argument processing

## Project Structure

```
CompilerDesign-master/
├── src/                    # Source code files
│   ├── lexer.l            # Flex lexer specification
│   ├── parser.y           # Bison parser specification
│   ├── main.cpp           # Main driver program
│   ├── ast.cpp/h          # Abstract Syntax Tree implementation
│   ├── symbol_table.cpp/h # Symbol table management
│   ├── ir_context.cpp/h   # IR generation context
│   ├── ir_generator.cpp/h # Three-address code generator
│   ├── basic_block.cpp/h  # Basic block analysis
│   └── mips_codegen.cpp/h # MIPS assembly code generator
├── obj/                   # Generated object files and parser outputs
├── test/                  # Test cases with .txt, .ir, and .s files
├── makefile              # Build configuration
├── run.sh                # Batch test runner
├── run_spim.sh          # SPIM simulator runner (all tests)
└── run_spim_single.sh   # SPIM simulator runner (single test)
```

## 🛠️ Prerequisites

### Required Tools
- **g++** (with C++11 support)
- **flex** (Fast Lexical Analyzer)
- **bison** (GNU Parser Generator)
- **make** (Build automation)

### Optional Tools
- **spim** or **qtspim** (MIPS simulator for testing generated assembly)

### Installation on Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install g++ flex bison make
sudo apt-get install spim  # for running MIPS code
```

### Installation on macOS
```bash
brew install flex bison make
brew install spim  # for running MIPS Code
```

## Building the Compiler

### Compile the Project
```bash
make
```

This will:
1. Generate lexer (`lex.yy.c`) from `lexer.l`
2. Generate parser (`parser.tab.c` and `parser.tab.h`) from `parser.y`
3. Compile all source files
4. Link everything into the `ir_generator` executable

### Clean Build Artifacts
```bash
make clean
```

## Usage

### Basic Compilation
```bash
./ir_generator <input_file.txt>
```

This generates:
- `<input_file>.ir` - Three-address code (intermediate representation)

### Generate MIPS Assembly
```bash
./ir_generator <input_file.txt> --generate-mips
```

This generates both:
- `<input_file>.ir` - Three-address code
- `<input_file>.s` - MIPS assembly code

### Additional Options
```bash
./ir_generator <input_file.txt> [options]

Options:
  --analyze-blocks       : Perform basic block analysis and print results
  --activation-records   : Compute and print activation records for functions
  --generate-mips        : Generate MIPS assembly code
```

### Example
```bash
# Compile a test file
./ir_generator test/3.\ For\ Loop.txt --generate-mips

# Output files created:
# - test/3. For Loop.ir    (intermediate representation)
# - test/3. For Loop.s     (MIPS assembly code)
```

## Testing

### Run All Test Cases
```bash
./run.sh
```

This script:
- Compiles the compiler
- Processes all `.txt` test files in the `test/` directory
- Generates `.ir` and `.s` files for each test
- Provides a summary of results

### Run MIPS Simulations
```bash
# Run all test cases in SPIM simulator
./run_spim.sh

# Run a single test case
./run_spim_single.sh "test/3. For Loop.txt"
```

### Test Cases Included

The `test/` directory contains 22+ comprehensive test cases covering:

1. **All Arithmetic & Logical Operators** - Operator precedence and evaluation
2. **If Else Statements** - Conditional branching
3. **For Loop** - For loop constructs
4. **While Loop** - While loop constructs
5. **Do While Loop** - Do-while loop constructs
6. **Switch Cases** - Switch-case statements
7. **Arrays (Int & Char)** - Array operations
8. **Pointers** - Pointer arithmetic and dereferencing
9. **Structures** - Struct definitions and member access
10. **Printf & Scanf** - I/O operations
11. **Function call with arguments** - Function calling conventions
12. **Goto, Break & Continue** - Jump statements
13. **Static Keywords** - Static variable handling
14. **Recursive Function Calls** - Recursion support
15. **Function call with variable arguments** - Variadic functions
16. **Function Pointer** - Function pointer usage
17. **Command line input** - argc/argv processing
18. **Typedef** - Type aliasing
19. **References** - Reference type support
20. **Enum Unions** - Enumeration and union types
21. **Until loop** - Custom until loop construct
22. **Multi level pointers** - Pointer to pointer operations

## Output Formats

### Three-Address Code (.ir)
Intermediate representation with operations like:
```
func_begin main
    a = 20
    b = 5
    t0 = a + b
    t1 = t0 * 2
    param t1
    call printf, 1
func_end
```

### MIPS Assembly (.s)
Target assembly code for MIPS architecture:
```assembly
.text
.globl main
main:
    # Function prologue
    addiu $sp, $sp, -32
    sw $ra, 28($sp)
    sw $fp, 24($sp)
    move $fp, $sp
    
    # Function body
    li $t0, 20
    sw $t0, 0($fp)
    ...
    
    # Function epilogue
    lw $ra, 28($sp)
    lw $fp, 24($sp)
    addiu $sp, $sp, 32
    jr $ra
```

## Architecture

### Compilation Pipeline

```
Source Code (.txt)
    ↓
[Lexer (flex)]
    ↓
Tokens
    ↓
[Parser (bison)]
    ↓
Abstract Syntax Tree (AST)
    ↓
[Symbol Table & Semantic Analysis]
    ↓
[IR Generator]
    ↓
Three-Address Code (.ir)
    ↓
[Basic Block Analysis] (optional)
    ↓
[MIPS Code Generator]
    ↓
MIPS Assembly (.s)
```

### Key Components

1. **Lexer** (`lexer.l`): Tokenizes input source code
2. **Parser** (`parser.y`): Builds AST and performs syntax checking
3. **AST** (`ast.cpp/h`): Tree representation of program structure
4. **Symbol Table** (`symbol_table.cpp/h`): Manages variable scopes and types
5. **IR Generator** (`ir_generator.cpp/h`): Produces three-address code
6. **Basic Block** (`basic_block.cpp/h`): Control flow analysis
7. **MIPS Codegen** (`mips_codegen.cpp/h`): Generates target assembly

## Error Handling

The compiler provides:
- **Syntax Error Detection**: Reports line numbers and error context
- **Semantic Error Checking**: Type checking, undefined variables, scope violations
- **Error Recovery**: Continues parsing after errors when possible
- **Detailed Error Messages**: Clear descriptions of what went wrong

## Advanced Features

### Symbol Table
- Hierarchical scope management
- Type information tracking
- Function signature validation
- Variable shadowing detection

### Activation Records
Compute stack frame layouts for functions including:
- Local variables
- Parameters
- Return address
- Saved registers

### Basic Block Analysis
Identifies:
- Leaders (start of basic blocks)
- Block boundaries
- Control flow structure

### Optimization Opportunities
The IR representation enables:
- Dead code elimination
- Constant folding
- Common subexpression elimination
- Register allocation

### Understanding the Code
- Review `src/parser.y` for grammar rules
- Check `src/ast.h` for AST node types
- Examine test cases for supported language features

