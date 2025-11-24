// Test file for if-else control flow
// Integration: Uses Arithmetic, Logical, Bitwise operators in conditions.
// Returns 0 on success, or line number on failure.

int main() {
    int a = 10;
    int b = 20;
    int c = 30;
    int check = 0;

    // --- 1. Basic 'if' (True Condition) ---
    // Should execute the body
    check = 0;
    if (a < b) {
        check = 1;
    }
    if (check != 1) {
        return 18;
    }

    // --- 2. Basic 'if' (False Condition) ---
    // Should SKIP the body
    check = 0;
    if (a > b) {
        check = 1;
    }
    if (check != 0) {
        return 28;
    }

    // --- 3. Basic 'if-else' (True Path) ---
    check = 0;
    if (a == 10) {
        check = 100;
    } else {
        check = 200;
    }
    if (check != 100) {
        return 39;
    }

    // --- 4. Basic 'if-else' (False Path) ---
    check = 0;
    if (a == 999) {
        check = 100;
    } else {
        check = 200;
    }
    if (check != 200) {
        return 49;
    }

    // --- 5. Nested 'if' (Depth 2) ---
    check = 0;
    if (a == 10) {
        if (b == 20) {
            check = 1;
        }
    }
    if (check != 1) {
        return 60;
    }

    // --- 6. Nested 'if-else' Mixed ---
    // Logic: a=10, b=20. Outer is true, Inner is false.
    check = 0;
    if (a == 10) {
        if (b == 99) {
            check = 1;
        } else {
            check = 2; // Should hit this
        }
    } else {
        check = 3;
    }
    if (check != 2) {
        return 76;
    }

    // --- 7. Chained 'if - else if - else' ladder ---
    // We look for the middle condition
    check = 0;
    int val = 50;
    
    if (val < 10) {
        check = 1;
    } else if (val < 40) {
        check = 2;
    } else if (val == 50) {
        check = 3; // Should hit this
    } else {
        check = 4;
    }
    
    if (check != 3) {
        return 94;
    }

    // --- 8. Complex Condition Integration (Arithmetic + Logical) ---
    // Condition: (10 + 20 == 30) AND (30 > 10) -> True
    if ( ((a + b) == c) && (c > a) ) {
        check = 1;
    } else {
        check = 0;
    }
    if (check != 1) {
        return 105;
    }

    // --- 9. Complex Condition (Bitwise + Arithmetic) ---
    // a = 10 (1010), b = 20 (10100)
    // a | b = 30 (11110)
    // Condition: (30 - 30) == 0 -> True
    if ( ( (a | b) - c ) == 0 ) {
        check = 55;
    } else {
        check = 66;
    }
    if (check != 55) {
        return 118;
    }

    // --- 10. The "Dangling Else" Test ---
    // In C, an 'else' attaches to the nearest 'if'.
    // if (false) if (true) X else Y
    // Should skip EVERYTHING. If else attached to outer, Y runs (Wrong).
    check = 0;
    int x = 0; // false
    int y = 1; // true
    
    if (x) 
        if (y) 
            check = 1;
        else 
            check = 2;
    
    // Since x is false, we enter neither inner if nor inner else.
    // check should remain 0.
    if (check != 0) {
        return 138; // Failed dangling else parsing/generation
    }

    // --- 11. Constant Conditions (Optimization Check) ---
    // if (1) should always run. if (0) should never run.
    
    check = 5;
    if (0) {
        check = 10; // Should check dead code elimination or jump logic
    }
    if (check != 5) {
        return 149;
    }
    
    if (1) {
        check = 20;
    }
    if (check != 20) {
        return 155;
    }

    // --- 12. Empty Bodies ---
    // Parser/Generator should not crash on empty statements
    if (a == 10) ; 
    else ;
    
    // --- 13. Scope/Stack modification inside block ---
    // Ensure modifying a variable inside a block affects the variable outside
    // (unless you implemented block-scoping with shadowing, assume standard C)
    a = 0;
    if (1) {
        a = 500;
        int inner = 10; // stack grows
        a = a + inner;  // 510
    }
    // stack shrinks, 'inner' is gone, but 'a' should remain 510
    if (a != 510) {
        return 172;
    }

    return 0; // All if-else tests passed
}