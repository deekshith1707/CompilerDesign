// Exhaustive Test Suite for Switch-Case
// Integration: Loops, If-Else, Arithmetic, Logic
// Returns: 0 on Success, Line Number on Failure

int main() {
    int val = 0;
    int res = 0;
    int i = 0;

    // ============================================================
    // SECTION 1: BASIC SWITCH
    // ============================================================

    // Test 1.1: Simple Match
    val = 2;
    res = 0;
    switch (val) {
        case 1:
            res = 10;
            break;
        case 2:
            res = 20; // Should hit this
            break;
        case 3:
            res = 30;
            break;
    }
    if (res != 20) {
        return 28; // FAILED: Basic switch match
    }

    // Test 1.2: Default Case Trigger
    val = 99; // Not 1, 2, or 3
    res = 0;
    switch (val) {
        case 1: res = 10; break;
        case 2: res = 20; break;
        default: res = 100; // Should hit this
    }
    if (res != 100) {
        return 40; // FAILED: Default case logic
    }

    // Test 1.3: Default NOT Triggered
    // If a case matches, default should be skipped
    val = 1;
    res = 0;
    switch (val) {
        case 1: res = 10; break; // Should hit this
        default: res = 100;
    }
    if (res != 10) {
        return 52; // FAILED: Executed default when case existed
    }

    // ============================================================
    // SECTION 2: FALL-THROUGH LOGIC
    // ============================================================
    // This tests if the compiler correctly omits the jump when 'break' is missing.

    val = 1;
    res = 0;
    switch (val) {
        case 1:
            res = res + 1; // Executed. res = 1.
            // NO BREAK HERE - Should fall through to case 2
        case 2:
            res = res + 2; // Executed. res = 3.
            break;         // NOW break.
        case 3:
            res = res + 10; // Skipped.
            break;
    }
    if (res != 3) {
        return 72; // FAILED: Fall-through logic
    }

    // ============================================================
    // SECTION 3: COMPLEX EXPRESSIONS & NEGATIVES
    // ============================================================

    // Test 3.1: Switch variable is an expression
    int a = 5;
    int b = 10;
    res = 0;
    
    // Expression: (5 * 10) - 48 = 2
    switch ( (a * b) - 48 ) {
        case 1: res = 111; break;
        case 2: res = 222; break; // Should hit this
        default: res = 333;
    }
    if (res != 222) {
        return 91; // FAILED: Expression evaluation in switch
    }

    // Test 3.2: Negative Case Constants
    // MIPS comparison logic must handle signed integers correctly
    val = -5;
    res = 0;
    switch (val) {
        case -1: res = 1; break;
        case -5: res = 5; break; // Should hit this
        case 5:  res = 10; break;
        default: res = 0;
    }
    if (res != 5) {
        return 105; // FAILED: Negative case constant
    }

    // ============================================================
    // SECTION 4: INTEGRATION WITH LOOPS
    // ============================================================
    
    // Logic: Loop 0 to 4.
    // i=0: Case 0 (res+=1) -> res=1
    // i=1: Case 1 (res+=2) -> res=3
    // i=2: Default (res+=5) -> res=8
    // i=3: Default (res+=5) -> res=13
    // i=4: Default (res+=5) -> res=18
    
    res = 0;
    for (i = 0; i < 5; i = i + 1) {
        switch (i) {
            case 0:
                res = res + 1;
                break; // Breaks SWITCH, not loop
            case 1:
                res = res + 2;
                break;
            default:
                res = res + 5;
        }
    }
    if (res != 18) {
        return 132; // FAILED: Loop + Switch integration
    }

    // ============================================================
    // SECTION 5: NESTED SWITCHES
    // ============================================================

    int outer = 1;
    int inner = 2;
    res = 0;

    switch (outer) {
        case 1:
            // Entering inner switch
            switch (inner) {
                case 1: res = 10; break;
                case 2: res = 20; break; // Should hit this
            }
            // Break from outer case 1
            break; 
        case 2:
            res = 30;
            break;
    }
    if (res != 20) {
        return 158; // FAILED: Nested switch logic
    }

    // ============================================================
    // SECTION 6: CONTROL FLOW MIX (Break vs Continue)
    // ============================================================
    // Crucial Test: 'break' inside switch should NOT stop the surrounding loop.
    // 'continue' inside switch is not valid C in most standards, 
    // but 'continue' inside a loop that contains a switch should work.

    res = 0;
    i = 0;
    while (i < 5) {
        i = i + 1;
        switch (i) {
            case 1:
                res = res + 1; 
                break; // Breaks switch, continues loop
            case 2:
                // Force early loop iteration
                // Note: This 'continue' applies to the 'while' loop!
                continue; 
            case 3:
                res = res + 10;
                break;
        }
        // If i=2, we hit 'continue', so this line is skipped.
        // i=1: res=1 + 100 = 101
        // i=2: Skipped (+100)
        // i=3: res=101 + 10 + 100 = 211
        // i=4,5: Default cases (empty switch), res += 100
        res = res + 100;
    }
    // Expected Calculation:
    // i=1: res += 1 (switch) -> res += 100 (loop end) -> res = 101
    // i=2: continue (skips +100) -> res = 101
    // i=3: res += 10 (switch) -> res += 100 (loop end) -> res = 211
    // i=4: res += 100 (loop end) -> res = 311
    // i=5: res += 100 (loop end) -> res = 411
    
    if (res != 411) {
        return 196; // FAILED: Interaction of continue inside switch-in-loop
    }

    return 0; // ALL TESTS PASSED
}