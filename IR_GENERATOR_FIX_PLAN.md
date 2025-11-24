# IR Generator Fix Plan - Memory Alignment Issues

## Problem Analysis

### Root Cause Identified
The memory alignment errors occur because of **missing and incomplete handling of pointer operations** in the IR to MIPS translation:

1. **ARRAY_ADDR operation not handled**: The IR generator creates `ARRAY_ADDR` for `&arr[i]` but the MIPS codegen has NO handler for it
2. **Pointer subscripting optimization**: When `ptr = arr1` is followed by `ptr[5]`, the assignment gets optimized away but the subscripting code doesn't properly compute the address
3. **Array name in arithmetic**: Operations like `t7 = arr1 + 3` should compute arr1's base address + 12, but instead use whatever value happens to be in a register

### Current Test Results
- **Pass Rate**: 50% (9/18 tests)
- **Passing**: arithmetic, if-else, switch, structures, functions, recursion, goto/break, static, references, enums
- **Failing**: arrays (alignment), pointers (alignment), printf/scanf, command-line, typedef
- **Timeout**: for, while, do-while, until loops

### Specific Error
```
Exception occurred at PC=0x004001a8
  Unaligned address in store: 0x00000016
  Exception 5  [Address error in store]  occurred and ignored
```
Address 0x16 (22 decimal) is not 4-byte aligned because it comes from loop counter (10) + offset (12) instead of arr1's base address.

## Implementation Plan

### Phase 1: Add ARRAY_ADDR Handler (Priority: HIGH)
**File**: `src/mips_codegen.cpp`
**Location**: Lines 2575-2580 in `translateInstruction()`

**What to do**:
```cpp
// After line 2575 (ADDR handler):
else if (strcmp(quad->op, "ADDR") == 0 || strcmp(quad->op, "&") == 0) {
    translateAddr(codegen, quad, irIndex);
}
// ADD THIS:
else if (strcmp(quad->op, "ARRAY_ADDR") == 0) {
    translateArrayAddr(codegen, quad, irIndex);
}
```

**Create new function** `translateArrayAddr()` around line 2290:
```cpp
/**
 * Translate ARRAY_ADDR operation: result = &array[index]
 * Computes the address of an array element
 */
void translateArrayAddr(MIPSCodeGenerator* codegen, Quadruple* quad, int irIndex) {
    char instr[256];
    
    const char* arrayName = quad->arg1;
    const char* indexVar = quad->arg2;
    const char* resultVar = quad->result;
    
    // Check if array or pointer
    Symbol* arraySym = lookupSymbol(arrayName);
    bool isPointerAccess = (arraySym != NULL && arraySym->ptr_level > 0 && !arraySym->is_array);
    
    // Get element size
    bool isCharArray = false;
    int elementSize = getArrayElementInfo(arrayName, &isCharArray);
    
    if (isPointerAccess) {
        // ptr[i] - load pointer value, add index offset
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
        
        // Calculate offset
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
        
        // Add to pointer base
        int resultReg = getReg(codegen, resultVar, irIndex);
        sprintf(instr, "    add %s, %s, $t9", 
                getRegisterName(resultReg), getRegisterName(ptrReg));
        emitMIPS(codegen, instr);
        
        updateDescriptors(codegen, resultReg, resultVar);
    } else {
        // arr[i] - compute from $fp offset
        char arrayLocation[128];
        getMemoryLocation(codegen, arrayName, arrayLocation);
        
        int baseOffset = 0;
        if (strstr(arrayLocation, "($fp)") != NULL) {
            sscanf(arrayLocation, "%d($fp)", &baseOffset);
        }
        
        // Get index
        int indexReg;
        if (isConstantValue(indexVar)) {
            int constIndex = atoi(indexVar);
            int totalOffset = baseOffset + (constIndex * elementSize);
            
            int resultReg = getReg(codegen, resultVar, irIndex);
            sprintf(instr, "    addi %s, $fp, %d", 
                    getRegisterName(resultReg), totalOffset);
            emitMIPS(codegen, instr);
            
            updateDescriptors(codegen, resultReg, resultVar);
        } else {
            indexReg = getReg(codegen, indexVar, irIndex);
            
            // Calculate offset
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
            
            // Add base offset
            sprintf(instr, "    addi $t9, $t9, %d", baseOffset);
            emitMIPS(codegen, instr);
            
            // Add to $fp
            int resultReg = getReg(codegen, resultVar, irIndex);
            sprintf(instr, "    add %s, $fp, $t9", getRegisterName(resultReg));
            emitMIPS(codegen, instr);
            
            updateDescriptors(codegen, resultReg, resultVar);
        }
    }
}
```

**Expected Impact**: Fixes address computations for `&arr[i]` operations. This is used in pointer arithmetic like `t7 = arr1 + 3` which gets converted to `&arr1[3]`.

---

### Phase 2: Fix IR Generation for Pointer Arithmetic (Priority: MEDIUM)
**File**: `src/ir_generator.cpp`
**Location**: Binary expression handling around line 750-900

**Problem**: When generating IR for `ptr = arr1`, it creates a simple assignment instead of computing the address. Then `t7 = arr1 + 3` doesn't properly recognize that arr1 is an array.

**What to do**:
1. Check if RHS is an array name in assignment context
2. If assigning array to pointer, generate `ADDR` operation:
```cpp
// In generate_ir for NODE_ASSIGNMENT_EXPRESSION
// Around line 1100
if (lhs is pointer && rhs is array name) {
    emit("ADDR", array_name, "", ptr_var);
}
```

3. For binary ADD/SUB with array name:
```cpp
// In generate_ir for NODE_ADDITIVE_EXPRESSION  
// Around line 800
if (left operand is array name) {
    // Convert to ARRAY_ADDR operation
    emit("ARRAY_ADDR", array_name, right_operand, temp);
} else {
    // Normal arithmetic
    emit("ADD", left, right, temp);
}
```

**Expected Impact**: Ensures `ptr = arr1` generates proper address computation and `arr + offset` uses ARRAY_ADDR.

---

### Phase 3: Improve Pointer Subscripting Detection (Priority: LOW)
**File**: `src/mips_codegen.cpp`
**Location**: `translateAssignArray()` and `translateArrayAccess()` (lines 2030-2130)

**Current Status**: Already implemented in previous iteration ✅
- Detects pointer subscripting via `ptr_level > 0 && !is_array`
- Loads pointer value and computes offset
- Uses lb/sb for char, lw/sw for int

**What to verify**:
- Ensure symbol table lookup works correctly
- Check that IR format matches expectations (may need to print debug)

---

### Phase 4: Fix Loop Control (Priority: MEDIUM)
**File**: Multiple files
**Problem**: 4 tests timeout (for, while, do-while, until)

**Analysis Needed**:
1. Check if loop counter updates are generated correctly
2. Verify loop condition jumps to right labels
3. Examine one failing loop test in detail:
```bash
./ir_generator test/3_for_loop.txt test/3_for_loop.s 2>&1 | grep -A 20 "L[0-9]:"
spim -file test/3_for_loop.s 2>&1 | head -30
```

**Expected root cause**: Loop counter variable not being stored back to memory, so condition always evaluates same way.

---

## Testing Strategy

### Step 1: Test ARRAY_ADDR Fix
```bash
make clean && make
bash run.sh
# Test arrays specifically
timeout 10 spim -file test/7_arrays.s 2>&1
# Should show different behavior - may still fail but with different error
```

### Step 2: Check Generated Code
```bash
# Verify ARRAY_ADDR translation
grep -n "addi.*\$fp" test/7_arrays.s | head -20
# Look for proper address calculations
```

### Step 3: Full Test Suite
```bash
./test_mips.sh
# Expectation: Pass rate should increase from 50% to 60-70%
# Arrays and pointers tests should pass or get closer
```

### Step 4: Debug Remaining Issues
```bash
# For each still-failing test:
cat test/X.output  # Check error
cat test/X.ir | grep -A 5 -B 5 "problem_line"
cat test/X.s | grep -A 10 "problem_instruction"
```

---

## Expected Outcomes

### Immediate (Phase 1 only):
- Arrays test: Should eliminate alignment error at 0x16
- Pointers test: Should improve (may have other issues)
- Pass rate: 50% → 55-60% (10-11 tests passing)

### After Phase 2:
- Arrays test: PASS ✅
- Pointers test: PASS ✅  
- Pass rate: 60% → 65-70% (11-13 tests passing)

### After Phase 4:
- Loop tests: PASS ✅
- Pass rate: 70% → 85-90% (15-16 tests passing)

### Final:
- All structural tests passing
- Only I/O and edge cases remaining
- Pass rate: 85-90%

---

## Risk Assessment

### Low Risk:
- ✅ Phase 1 (ARRAY_ADDR handler) - Straightforward addition, won't break existing tests
- ✅ Phase 3 verification - Already implemented

### Medium Risk:
- ⚠️ Phase 2 (IR generation changes) - Could affect how other operations work
- **Mitigation**: Test incrementally, check all 18 tests after each change

### High Risk:
- ⚠️ Phase 4 (loop control) - Complex, affects multiple test cases
- **Mitigation**: Fix one loop type at a time, compare with working tests

---

## Implementation Order

1. **NOW**: Phase 1 - Add ARRAY_ADDR handler (30 min)
2. **NEXT**: Test and verify improvement (15 min)
3. **THEN**: Phase 2 - Fix IR generation (1-2 hours)
4. **AFTER**: Phase 4 - Debug loops (2-3 hours)
5. **FINALLY**: Polish and optimize remaining issues

---

## Success Metrics

- ✅ Alignment error at 0x16 eliminated
- ✅ Arrays test produces valid output
- ✅ Pointers test passes
- ✅ No regression in currently passing tests
- 🎯 Pass rate ≥ 60% after Phase 1
- 🎯 Pass rate ≥ 70% after Phase 2
- 🎯 Pass rate ≥ 85% after Phase 4
