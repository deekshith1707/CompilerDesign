# Register Allocation & Descriptor Management Analysis

## Current Architecture Overview

### Data Structures

#### 1. Register Descriptor (`RegisterDescriptor`)
```c
typedef struct RegisterDescriptor {
    char varNames[10][128];  // Variables currently in this register
    int varCount;            // Number of variables
    bool isDirty;            // Has the register been modified?
} RegisterDescriptor;
```

**Purpose**: Track which variables are currently held in each register
**Coverage**: 32 MIPS registers (but only $t0-$t9 are actively managed)

#### 2. Address Descriptor (`AddressDescriptor`)
```c
typedef struct AddressDescriptor {
    char varName[128];
    bool inMemory;           // Value is in memory
    int inRegister;          // Register number (-1 if not in register)
    int memoryOffset;        // Offset from $fp (for locals)
    bool isGlobal;           // Is this a global variable?
} AddressDescriptor;
```

**Purpose**: Track all locations where a variable's value exists (memory, register, or both)
**Capacity**: MAX_VARIABLES (1000)

---

## Core Functions Analysis

### 1. `getReg()` - Register Allocation Algorithm

**Location**: `src/mips_codegen.cpp:967-1090`

**Algorithm Flow**:
1. **Check if constant** → Load into available register, don't track
2. **Check if variable already in register** → Return that register (BEST CASE)
3. **Find empty register** → Load variable, update descriptors
4. **All registers full** → Spill victim register (using next-use heuristic)

**Issues Identified**:
- ✅ Handles constants correctly (no descriptor updates)
- ✅ Uses next-use information for victim selection
- ⚠️ **CRITICAL ISSUE**: When variable is in register, returns it WITHOUT checking if the value is STALE
  - After function calls, registers are cleared but address descriptors may still point to them
  - After scanf/pointer writes, memory is updated but register value becomes stale

### 2. `updateDescriptors()` - Descriptor Synchronization

**Location**: `src/mips_codegen.cpp:679-701`

**Current Behavior**:
- Adds variable to register descriptor
- Updates address descriptor to point to the register
- **PROBLEM**: No validation that the register actually contains the correct value

### 3. `clearRegisterDescriptor()` - Register Invalidation

**Location**: `src/mips_codegen.cpp:705-712`

**Current Behavior**:
- Clears register descriptor (removes all variables from register)
- Sets isDirty = false
- **PROBLEM**: Does NOT update corresponding address descriptors!
  - Address descriptors may still have `inRegister = regNum`
  - Creates inconsistency: register says "empty", address says "in register X"

### 4. `spillRegister()` - Register Eviction

**Location**: `src/mips_codegen.cpp:834-855`

**Current Behavior**:
- Stores dirty values to memory
- Marks `inMemory = true` in address descriptor
- Sets `inRegister = -1` in address descriptor ✅
- Clears register descriptor ✅
- **This one is CORRECT!**

---

## Problem Case Studies

### Problem 1: scanf Variable Loading

**Scenario**:
```ir
t20 = &input_int        // t20 gets address -132($fp)
param t20
call scanf
t31 = input_int         // Should LOAD from -132($fp)
param t31
call printf
```

**What Should Happen**:
1. `t20 = &input_int` → $t0 = -132 (address calculation)
2. `call scanf` → Writes user input to address -132($fp)
3. **scanf should invalidate ALL descriptors** (values in memory changed)
4. `t31 = input_int` → getReg() should see input_int NOT in register, load from memory
5. printf prints the loaded value

**What Actually Happens**:
1. `t20 = &input_int` → $t0 = -132
2. **UpdateDescriptors** records: `input_int inRegister=$t0` ❌ (WRONG! $t0 has ADDRESS not VALUE)
3. `call scanf` → Clears registers but NOT address descriptors properly
4. `t31 = input_int` → getReg() sees `input_int inRegister=$t0`, returns $t0
5. printf prints the ADDRESS instead of the value!

**Root Cause**: 
- Address-of operation (`&`) stores address in register
- Descriptor system incorrectly treats this as "variable is in register"
- Need to distinguish between "register holds VALUE" vs "register holds ADDRESS"

### Problem 2: Function Call Register Invalidation

**Scenario**:
```ir
x = 10
call foo
y = x    // Should reload x from memory
```

**Current Fix**: ✅ WORKING
- `clearRegisterDescriptor()` called for all $t0-$t9 after function calls
- **BUT**: Still doesn't update address descriptors properly

---

## Proposed Solutions

### Solution 1: Fix `clearRegisterDescriptor()` to Update Address Descriptors

**Change**: When clearing a register, invalidate all address descriptors pointing to it

```c
void clearRegisterDescriptor(MIPSCodeGenerator* codegen, int regNum) {
    // Clear register descriptor
    codegen->regDescriptors[regNum].varCount = 0;
    codegen->regDescriptors[regNum].isDirty = false;
    
    // CRITICAL FIX: Invalidate all address descriptors pointing to this register
    for (int i = 0; i < codegen->addrDescCount; i++) {
        if (codegen->addrDescriptors[i].inRegister == regNum) {
            codegen->addrDescriptors[i].inRegister = -1;
        }
    }
    
    for (int i = 0; i < 10; i++) {
        codegen->regDescriptors[regNum].varNames[i][0] = '\0';
    }
}
```

**Impact**: 
- ✅ Maintains consistency between register and address descriptors
- ✅ Forces subsequent loads to reload from memory
- ✅ Fixes function call issues completely

### Solution 2: Distinguish Address vs Value in Descriptors

**Problem**: When `t20 = &input_int` executes, we store address in $t0, but descriptor thinks VALUE is in $t0

**Option A**: Add flag to AddressDescriptor
```c
typedef struct AddressDescriptor {
    char varName[128];
    bool inMemory;
    int inRegister;          // Register number (-1 if not in register)
    bool registerHasAddress; // TRUE if register has &var, FALSE if register has value ⭐ NEW
    int memoryOffset;
    bool isGlobal;
} AddressDescriptor;
```

**Option B**: Don't track address-of operations in descriptors
- When `result = &variable` executes, only update register descriptor for `result`, not for `variable`
- This way `input_int` won't have `inRegister=$t0` set

**Recommended**: Option B - Simpler, less state to track

### Solution 3: Force Memory Reload After Pointer Operations

**Affected Operations**:
- `scanf()` - writes through pointers
- `*ptr = value` - dereference assignment
- Any function call that might modify memory

**Implementation**: Clear ALL address descriptors (set `inRegister = -1` for all)

```c
// After scanf, pointer writes, or unknown function calls
void invalidateAllAddressDescriptors(MIPSCodeGenerator* codegen) {
    for (int i = 0; i < codegen->addrDescCount; i++) {
        codegen->addrDescriptors[i].inRegister = -1;
    }
}
```

---

## Implementation Plan

### Phase 1: Fix Core Descriptor Consistency (HIGH PRIORITY) ⭐
**Files**: `src/mips_codegen.cpp`

1. **Fix `clearRegisterDescriptor()`**
   - Add loop to invalidate address descriptors
   - **Impact**: Fixes function call register invalidation completely
   - **Estimated Lines**: +5 lines

2. **Fix address-of operations**
   - In `translateAssignment()` when `arg1[0] == '&'`:
     - Calculate address and store in register for RESULT
     - Do NOT update address descriptor for the variable being addressed
   - **Impact**: Prevents "address in register" being confused with "value in register"
   - **Estimated Lines**: +2 lines (remove one updateDescriptors call)

### Phase 2: Fix scanf Implementation (HIGH PRIORITY) ⭐
**Files**: `src/mips_codegen.cpp`

1. **After scanf, invalidate all descriptors**
   - Replace current invalidation with comprehensive one
   - **Already attempted, needs Phase 1 fixes first**

2. **Test with simple case first**
   ```c
   int x;
   scanf("%d", &x);
   printf("%d", x);
   ```

### Phase 3: Add Defensive Checks (MEDIUM PRIORITY)
**Files**: `src/mips_codegen.cpp`

1. **Add validation to `getReg()`**
   - Before returning cached register, verify value is still valid
   - Check if register was cleared but address descriptor wasn't updated
   
2. **Add debug logging (optional)**
   - Log descriptor state at critical points
   - Help diagnose future issues

### Phase 4: Handle Pointer Operations (MEDIUM PRIORITY)
**Files**: `src/mips_codegen.cpp`

1. **After `DEREF` assignment (`*ptr = value`)**
   - Invalidate all address descriptors
   - Cannot know what memory location was modified

2. **After unknown function calls**
   - Already handled via `clearRegisterDescriptor()`
   - Verify it's working with Phase 1 fixes

---

## Testing Strategy

### Test Case 1: Simple scanf
```c
int main() {
    int x;
    scanf("%d", &x);
    printf("%d\n", x);
    return 0;
}
```
**Expected**: Print the input value
**Currently**: Prints garbage (address)

### Test Case 2: Multiple scanf
```c
int main() {
    int a, b;
    scanf("%d", &a);
    scanf("%d", &b);
    printf("%d %d\n", a, b);
    return 0;
}
```

### Test Case 3: Function call with registers
```c
void foo() {}
int main() {
    int x = 5;
    foo();
    printf("%d\n", x);  // Should print 5
    return 0;
}
```
**Expected**: Already working after previous fixes

---

## Risk Assessment

### High Risk Changes
- ❌ **None** - All proposed changes are localized and well-understood

### Medium Risk Changes
- ⚠️ Modifying `clearRegisterDescriptor()` - Widely used function
  - **Mitigation**: Comprehensive testing after change
  
### Low Risk Changes  
- ✅ Fixing address-of in `translateAssignment()` - Localized change
- ✅ Invalidating descriptors after scanf - Already attempted, just needs refinement

---

## Success Metrics

### Before Implementation
- ✅ 15/18 tests passing (83%)
- ❌ Test 10 (printf_scanf): FAIL - garbage output
- ❌ Test 15 (typedef): TIMEOUT
- ❌ Test 18 (until_loop): TIMEOUT

### After Phase 1
- 🎯 Test 10 should PASS (scanf working)
- 🎯 Verify all 15 passing tests still pass

### After Phase 2  
- 🎯 16/18 tests passing (89%)

### After Full Implementation
- 🎯 Goal: 16-17/18 tests passing (89-94%)
- Note: typedef and until_loop may be IR generation issues, not codegen issues

---

## Code Locations Quick Reference

| Function | Location | Purpose |
|----------|----------|---------|
| `getReg()` | Line 967 | Main register allocation |
| `updateDescriptors()` | Line 679 | Sync reg & addr descriptors |
| `clearRegisterDescriptor()` | Line 705 | **⭐ NEEDS FIX** |
| `spillRegister()` | Line 834 | Evict register to memory |
| `loadVariable()` | Line 858 | Load from memory |
| `storeVariable()` | Line 963 | Store to memory |
| `translateAssignment()` | Line 1310 | **⭐ NEEDS FIX** (line 1348) |
| `translateCall()` | Line 1740 | Function calls (includes scanf) |

---

## Next Steps

1. ✅ **Implement Phase 1, Step 1**: Fix `clearRegisterDescriptor()`
2. ✅ **Implement Phase 1, Step 2**: Fix address-of in assignment
3. 🧪 **Test**: Run all 18 tests, verify no regressions
4. ✅ **Implement Phase 2**: Test scanf specifically
5. 📊 **Measure**: Check if test 10 passes
6. 🔄 **Iterate**: If still failing, add debug output to trace issue

---

## Estimated Effort

- **Phase 1**: 30 minutes (code + test)
- **Phase 2**: 15 minutes (already mostly done)
- **Phase 3**: 30 minutes (validation + debug)
- **Total**: ~75 minutes for complete fix

---

## Conclusion

The descriptor management system has **two critical bugs**:

1. **Bug #1**: `clearRegisterDescriptor()` doesn't invalidate address descriptors
   - **Severity**: HIGH - Causes stale register issues after function calls
   - **Fix**: Simple 5-line loop
   
2. **Bug #2**: Address-of operations update wrong descriptors  
   - **Severity**: HIGH - Causes scanf to fail
   - **Fix**: Don't track variable when only address is computed

Both fixes are **low-risk, high-impact** changes that should resolve the scanf issue completely.
