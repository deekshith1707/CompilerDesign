// Exhaustive Test Suite for Arrays (int and char)
// Integration: Loops, Switch, If-Else, Arithmetic
// Returns: 0 on Success, Line Number on Failure

int main() {
    // Note: Array sizes must be compile-time constants in most C subsets
    int intArr[10]; 
    char charArr[6];
    int i = 0;
    int sum = 0;
    int check = 0;

    // ============================================================
    // SECTION 1: BASIC INTEGER ARRAYS
    // ============================================================

    // Test 1.1: Direct Write and Read
    // Verifies base address calculation and offset 0
    intArr[0] = 42;
    intArr[1] = 99;
    intArr[9] = 123; // Last element

    if (intArr[0] != 42) {
        return 24; // FAILED: Read/Write index 0
    }
    if (intArr[1] != 99) {
        return 27; // FAILED: Read/Write index 1
    }
    if (intArr[9] != 123) {
        return 30; // FAILED: Read/Write last index
    }

    // Test 1.2: Overwriting values
    intArr[0] = 55;
    if (intArr[0] != 55) {
        return 36; // FAILED: Overwrite index 0
    }

    // ============================================================
    // SECTION 2: LOOP INTEGRATION (Population & Aggregation)
    // ============================================================

    // Test 2.1: Populate using FOR loop
    // Logic: Fill array with 0, 10, 20, ... 90
    for (i = 0; i < 10; i = i + 1) {
        intArr[i] = i * 10;
    }

    // Verify a few random spots
    if (intArr[5] != 50) {
        return 50; // FAILED: Loop population index 5
    }
    if (intArr[8] != 80) {
        return 53; // FAILED: Loop population index 8
    }

    // Test 2.2: Aggregate (Sum) using WHILE loop
    // Logic: Sum = 0+10+20...+90 = 450
    sum = 0;
    i = 0;
    while (i < 10) {
        sum = sum + intArr[i];
        i = i + 1;
    }
    if (sum != 450) {
        return 65; // FAILED: Array reading inside while loop
    }

    // ============================================================
    // SECTION 3: COMPLEX INDEX ARITHMETIC
    // ============================================================
    // This tests if the compiler correctly computes [Base + (Expr * 4)]

    // Test 3.1: Expression in Index
    i = 2;
    // intArr[2 + 1] -> intArr[3] -> should be 30
    check = intArr[i + 1]; 
    if (check != 30) {
        return 77; // FAILED: Index arithmetic (i+1)
    }

    // Test 3.2: Array element as Index
    // intArr[0] is 0. intArr[intArr[0]] -> intArr[0] -> 0
    intArr[0] = 2;
    // intArr[2] is 20.
    // intArr[ intArr[0] ] -> intArr[2] -> 20.
    check = intArr[ intArr[0] ];
    if (check != 20) {
        return 87; // FAILED: Nested array indexing
    }

    // Restore index 0 for safety
    intArr[0] = 0;

    // ============================================================
    // SECTION 4: CHAR ARRAYS (STRINGS)
    // ============================================================
    // Note: char usually takes 1 byte (or 4 depending on implementation).
    // This tests byte-addressing or word-aligned char access.

    // Test 4.1: Basic Char Write/Read
    charArr[0] = 'H'; // Ascii 72
    charArr[1] = 'e'; // Ascii 101
    charArr[2] = 'l'; // Ascii 108
    charArr[3] = 'l'; // Ascii 108
    charArr[4] = 'o'; // Ascii 111
    charArr[5] = 0;   // Null terminator

    if (charArr[0] != 'H') {
        return 108; // FAILED: Char array index 0
    }
    if (charArr[4] != 'o') {
        return 111; // FAILED: Char array index 4
    }

    // Test 4.2: Char vs Int types
    // Ensure accessing charArr doesn't grab 4 bytes of data if packed,
    // or grabs the correct word if aligned.
    // 'e' is 101.
    if (charArr[1] != 101) {
        return 119; // FAILED: Char value check
    }

    // Test 4.3: Loop over Char Array
    // Sum ascii values: H(72)+e(101)+l(108)+l(108)+o(111) = 500
    sum = 0;
    for (i = 0; i < 5; i = i + 1) {
        sum = sum + charArr[i];
    }
    if (sum != 500) {
        return 129; // FAILED: Char iteration sum
    }

    // ============================================================
    // SECTION 5: INTEGRATION (SWITCH & IF)
    // ============================================================

    // Test 5.1: Switch on Array Element
    // intArr[3] is 30
    check = 0;
    switch (intArr[3]) {
        case 20: 
            check = 1; 
            break;
        case 30: 
            check = 2; // Should hit this
            break; 
        default: 
            check = 3;
    }
    if (check != 2) {
        return 149; // FAILED: Switch statement on array value
    }

    // Test 5.2: Writing to Array inside If-Else
    i = 5;
    if (intArr[i] > 40) { // 50 > 40 is True
        intArr[i] = 999;
    } else {
        intArr[i] = 111;
    }
    
    if (intArr[5] != 999) {
        return 161; // FAILED: Modification inside if-block
    }

    return 0; // ALL TESTS PASSED
}