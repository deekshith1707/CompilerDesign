// Test file for all operators
// Returns 0 on success, or line number on failure.

int main() {
    int a, b, c;
    int zero = 0;
    int pos = 10;
    int neg = -20;
    
    // --- 1. Arithmetic Operators (+, -, *, /, %) ---

    // Test 1.1: Addition (Pos + Pos)
    a = 15 + 7;
    if (a != 22) {
        return 16;
    }
    
    // Test 1.2: Addition (Pos + Neg)
    a = pos + neg; // 10 + (-20) = -10
    if (a != -10) {
        return 22;
    }
    
    // Test 1.3: Addition (Neg + Neg)
    a = neg + neg; // (-20) + (-20) = -40
    if (a != -40) {
        return 28;
    }

    // Test 1.4: Addition with Zero
    a = pos + zero;
    if (a != 10) {
        return 34;
    }
    
    // Test 1.5: Subtraction (Pos - Pos)
    a = 15 - 7;
    if (a != 8) {
        return 40;
    }

    // Test 1.6: Subtraction (Pos - Neg)
    a = pos - neg; // 10 - (-20) = 30
    if (a != 30) {
        return 46;
    }
    
    // Test 1.7: Subtraction (Neg - Pos)
    a = neg - pos; // (-20) - 10 = -30
    if (a != -30) {
        return 52;
    }

    // Test 1.8: Subtraction from Zero
    a = zero - pos;
    if (a != -10) {
        return 58;
    }
    
    // Test 1.9: Multiplication (Pos * Pos)
    a = 6 * 7;
    if (a != 42) {
        return 64;
    }
    
    // Test 1.10: Multiplication (Pos * Neg)
    a = pos * neg; // 10 * (-20) = -200
    if (a != -200) {
        return 70;
    }
    
    // Test 1.11: Multiplication by Zero
    a = pos * zero;
    if (a != 0) {
        return 76;
    }
    
    // Test 1.12: Multiplication by 1
    a = pos * 1;
    if (a != 10) {
        return 82;
    }

    // Test 1.13: Multiplication by -1
    a = pos * -1;
    if (a != -10) {
        return 88;
    }

    // Test 1.14: Division (Integer Truncation)
    a = 21 / 5; // Should be 4
    if (a != 4) {
        return 94;
    }
    
    // Test 1.15: Division (Neg / Pos)
    a = -21 / 5; // Should be -4
    if (a != -4) {
        return 100;
    }

    // Test 1.16: Division (Pos / Neg)
    a = 21 / -5; // Should be -4
    if (a != -4) {
        return 106;
    }
    
    // Test 1.17: Division (Neg / Neg)
    a = -21 / -5; // Should be 4
    if (a != 4) {
        return 112;
    }

    // Test 1.18: Division (Zero / Pos)
    a = zero / pos;
    if (a != 0) {
        return 118;
    }
    // NOTE: Division by zero (e.g., pos / zero) is typically
    // handled by the MIPS processor itself, raising an
    // exception. We can't "test" this with a return code.

    // Test 1.19: Modulo (Pos % Pos)
    a = 21 % 5; // Remainder is 1
    if (a != 1) {
        return 127;
    }
    
    // Test 1.20: Modulo (Smaller % Larger)
    a = 5 % 21; // Remainder is 5
    if (a != 5) {
        return 133;
    }
    
    // Test 1.21: Modulo (Neg % Pos)
    a = -21 % 5; // Remainder is -1 (in C/Java semantics)
    if (a != -1) {
        return 139;
    }

    // Test 1.22: Modulo (Pos % Neg)
    a = 21 % -5; // Remainder is 1 (in C/Java semantics)
    if (a != 1) {
        return 145;
    }

    // Test 1.23: Modulo by Zero
    // Like division by zero, this raises an exception.
    
    
    // --- 2. Relational Operators (==, !=, <, >, <=, >=) ---
    // These are implicitly tested by 'if' but good to be explicit.
    
    a = 10;
    b = 10;
    c = 20;

    // Test 2.1: Equal (==) - True
    if ( !(a == b) ) {
        return 161;
    }
    
    // Test 2.2: Equal (==) - False
    if ( a == c ) {
        return 167;
    }

    // Test 2.3: Not Equal (!=) - True
    if ( !(a != c) ) {
        return 173;
    }
    
    // Test 2.4: Not Equal (!=) - False
    if ( a != b ) {
        return 179;
    }
    
    // Test 2.5: Less Than (<) - True
    if ( !(a < c) ) {
        return 185;
    }

    // Test 2.6: Less Than (<) - False (equal)
    if ( a < b ) {
        return 191;
    }
    
    // Test 2.7: Less Than (<) - False (greater)
    if ( c < a ) {
        return 197;
    }
    
    // Test 2.8: Greater Than (>) - True
    if ( !(c > a) ) {
        return 203;
    }

    // Test 2.9: Greater Than (>) - False (equal)
    if ( a > b ) {
        return 209;
    }
    
    // Test 2.10: Less/Equal (<=) - True (less)
    if ( !(a <= c) ) {
        return 215;
    }
    
    // Test 2.11: Less/Equal (<=) - True (equal)
    if ( !(a <= b) ) {
        return 221;
    }
    
    // Test 2.12: Less/Equal (<=) - False
    if ( c <= a ) {
        return 227;
    }
    
    // Test 2.13: Greater/Equal (>=) - True (greater)
    if ( !(c >= a) ) {
        return 233;
    }
    
    // Test 2.14: Greater/Equal (>=) - True (equal)
    if ( !(a >= b) ) {
        return 239;
    }

    // Test 2.15: Relational with Negatives
    if ( !(neg < pos) ) {
        return 245;
    }
    if ( neg >= pos ) {
        return 248;
    }
    
    
    // --- 3. Logical Operators (!, &&, ||) ---
    
    a = 1; // C-style true
    b = 0; // C-style false
    c = 1;
    
    // Test 3.1: Logical NOT (!) - True
    if ( !b ) {
        // good
    } else {
        return 262;
    }
    
    // Test 3.2: Logical NOT (!) - False
    if ( !a ) {
        return 268;
    }

    // Test 3.3: Logical AND (&&) - True
    if ( !(a && c) ) {
        return 274;
    }
    
    // Test 3.4: Logical AND (&&) - False (Left)
    if ( b && a ) {
        return 280;
    }
    
    // Test 3.5: Logical AND (&&) - False (Right)
    if ( a && b ) {
        return 286;
    }

    // Test 3.6: Logical OR (||) - True (Left)
    if ( !(a || b) ) {
        return 292;
    }
    
    // Test 3.7: Logical OR (||) - True (Right)
    if ( !(b || a) ) {
        return 298;
    }
    
    // Test 3.8: Logical OR (||) - False
    if ( b || b ) {
        return 304;
    }

    // Test 3.9: Short-Circuit AND (&&)
    // This test ensures `b` (which is 0) is not evaluated,
    // which would cause a division by zero if it were.
    a = 0; // false
    b = 10;
    if ( a && ( (b / a) == 1) ) { // (b / a) is not run
        return 313;
    }
    // If your compiler is correct, we avoid the crash.

    // Test 3.10: Short-Circuit OR (||)
    // This test ensures the division by zero is not run
    // because the first operand is true.
    a = 1; // true
    b = 0;
    if ( a || ( (10 / b) == 1) ) { // (10 / b) is not run
        // good
    } else {
        return 325;
    }
    // If your compiler is correct, we avoid the crash.


    // --- 4. Bitwise Operators (&, |, ^, ~, <<, >>) ---
    
    a = 5;  // 0000 0101
    b = 12; // 0000 1100
    
    // Test 4.1: Bitwise AND (&)
    c = a & b; // 0000 0100 (which is 4)
    if (c != 4) {
        return 338;
    }
    
    // Test 4.2: Bitwise OR (|)
    c = a | b; // 0000 1101 (which is 13)
    if (c != 13) {
        return 344;
    }
    
    // Test 4.3: Bitwise XOR (^)
    c = a ^ b; // 0000 1001 (which is 9)
    if (c != 9) {
        return 350;
    }
    
    // Test 4.4: Bitwise NOT (~)
    c = ~a; // ~0000 0101 = 1111 1010 (which is -6)
    if (c != -6) {
        return 356;
    }
    if (~(-1) != 0) { // ~1111 1111 = 0000 0000
        return 359;
    }

    // Test 4.5: Left Shift (<<)
    c = a << 2; // 0000 0101 << 2 = 0001 0100 (which is 20)
    if (c != 20) {
        return 365;
    }
    
    // Test 4.6: Right Shift (>>) (Arithmetic)
    c = neg >> 2; // -20 >> 2 = -5
                  // 1110 1100 >> 2 = 1111 1011 (which is -5)
    if (c != -5) {
        return 372;
    }
    
    // Test 4.7: Right Shift (>>) (Logical on positive)
    c = pos >> 2; // 10 >> 2 = 2
                  // 0000 1010 >> 2 = 0000 0010 (which is 2)
    if (c != 2) {
        return 379;
    }

    // Test 4.8: Shift by Zero
    if ( (a << 0) != a) {
        return 384;
    }
    if ( (a >> 0) != a) {
        return 387;
    }

    return 0; // All operator tests passed
}