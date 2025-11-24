# Remaining Issues - Fix Plan

## Current Status
- Pass Rate: **50% (9/18)**
- ✅ Alignment error fixed
- 4 timeouts (loops)
- 5 failures (arrays, pointers, I/O, edge cases)

## Priority Order (Impact vs Complexity)

### 🎯 **PRIORITY 1: Fix Loop Timeouts** (Highest Impact - 4 tests)
**Tests affected**: 3_for_loop, 4_while_loop, 5_do_while_loop, 18_until_loop
**Impact**: +22% pass rate (4 more tests)
**Complexity**: Medium
**Estimated time**: 30-60 minutes

**Problem**: Loop counters not being updated in memory, causing infinite loops

**Root Cause Analysis**:
```
L0:
    i = 0
L1:
    if i >= 10 goto L3
    // ... do work ...
    i = i + 1  # <- This update not written to memory
    goto L1
L3:
```

**Solution**:
1. Check if loop counter updates mark registers as dirty
2. Ensure loop counter is spilled to memory before branch
3. Verify loop counter is reloaded at loop start

**Implementation**:
- File: `src/mips_codegen.cpp`
- Function: `translateAssignment()` - ensure isDirty flag is set
- Function: `translateLabel()` - already spills dirty registers (verify)
- Add: Force store for loop control variables

---

### 🎯 **PRIORITY 2: Fix Array Values** (Medium Impact - 1 test)
**Tests affected**: 7_arrays
**Impact**: +5.5% pass rate
**Complexity**: Low
**Estimated time**: 20-30 minutes

**Problem**: `arr1[0] = 100` stores wrong value, loads show 0

**Root Cause**: Array initialization or assignment not working properly

**Investigation needed**:
```bash
# Check what arr1[0] = 100 generates
grep -A 5 "arr1\[0\]" test/7_arrays.ir
grep -A 10 "li.*100" test/7_arrays.s
```

**Likely issues**:
1. Constant 100 not being loaded correctly
2. Array offset calculation wrong
3. Register allocation overwriting value

---

### 🎯 **PRIORITY 3: Fix Pointer Values** (Medium Impact - 1 test)
**Tests affected**: 8_pointers
**Impact**: +5.5% pass rate
**Complexity**: Medium
**Estimated time**: 30-45 minutes

**Problem**: Pointer contains data value (10) instead of address (0x0a is 10 decimal)

**Root Cause**: `ptr = &x` not computing address properly

**Solution**:
- Verify `translateAddr()` is being called for `&` operator
- Check if pointer assignment uses ADDR operation
- Ensure address computation stores result

---

### 🎯 **PRIORITY 4: I/O and Edge Cases** (Low Impact - 3 tests)
**Tests affected**: 10_printf_scanf, 14_command_line_input, 15_typedef
**Impact**: +16.5% pass rate
**Complexity**: High (each test has different issues)
**Estimated time**: 1-2 hours

**Defer to end** - Focus on loops, arrays, pointers first

---

## Implementation Plan

### Phase 1: Fix Loop Timeouts (START HERE)
**Goal**: Get 4 loop tests passing → 72% pass rate

#### Step 1.1: Diagnose Loop Counter Issue
```bash
# Check for_loop IR and assembly
./ir_generator test/3_for_loop.txt 2>&1 | grep -A 20 "L[0-9]:"
# Look for loop counter variable updates
```

#### Step 1.2: Test Simple Loop
Create minimal test case:
```c
int main() {
    int i;
    for(i = 0; i < 3; i++) {
        printf("%d\n", i);
    }
    return 0;
}
```

#### Step 1.3: Fix Register Spilling
Ensure loop counter is written to memory before branch:
- Check `translateAssignment()` - verify isDirty = true
- Check branch translation - ensure spill before jump
- Add special handling for loop control variables

#### Step 1.4: Verify Fix
```bash
make && bash run.sh >/dev/null 2>&1
timeout 5 spim -file test/3_for_loop.s 2>&1 | head -30
# Should complete in < 1 second and print correct output
```

---

### Phase 2: Fix Array Values
**Goal**: Get arrays test passing → 78% pass rate

#### Step 2.1: Check Array Initialization
```bash
# See how arr1[0] = 100 is translated
grep -B 5 -A 5 "arr1\[0\] = 100" test/7_arrays.ir
grep "li.*100" test/7_arrays.s
```

#### Step 2.2: Trace Value Flow
Follow the constant 100 from source → IR → assembly:
- Is 100 loaded into a register?
- Is it stored to the right offset?
- Is it loaded back correctly for printf?

#### Step 2.3: Fix Issue
Likely fixes:
- Constant loading in `getReg()`
- Array subscript offset calculation
- Store/load instruction selection

---

### Phase 3: Fix Pointer Values
**Goal**: Get pointers test passing → 83% pass rate

#### Step 3.1: Check Pointer Assignment
```bash
# See how ptr = &x is handled
grep "&" test/8_pointers.txt
grep "ADDR\|&" test/8_pointers.ir
```

#### Step 3.2: Verify translateAddr
- Check if ADDR operation is emitted for `&x`
- Verify `translateAddr()` computes correct address
- Ensure result is stored in pointer variable

---

## Success Metrics

### After Phase 1 (Loops):
- ✅ 13/18 tests passing (72%)
- ✅ All loops complete in < 1 second
- ✅ No timeouts

### After Phase 2 (Arrays):
- ✅ 14/18 tests passing (78%)
- ✅ Array values correct
- ✅ String printing works

### After Phase 3 (Pointers):
- ✅ 15/18 tests passing (83%)
- ✅ Pointer arithmetic works
- ✅ Dereferencing correct

### Final Goal:
- 🎯 15-16/18 tests passing (83-89%)
- 🎯 Only I/O edge cases remaining
- 🎯 Core language features all working

---

## Time Estimate
- Phase 1 (Loops): 30-60 min
- Phase 2 (Arrays): 20-30 min
- Phase 3 (Pointers): 30-45 min
- **Total**: 1.5-2.5 hours to reach 83% pass rate

## START: Phase 1 - Loop Timeout Fix
