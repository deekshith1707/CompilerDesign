int main() {
    int intArr[10];
    int i;
    
    // Populate array
    for (i = 0; i < 10; i = i + 1) {
        intArr[i] = i * 10;
    }
    
    // Check value
    int val = intArr[3];
    
    if (val != 30) {
        return 1;  // Error
    }
    
    // Test switch
    int check = 0;
    switch (intArr[3]) {
        case 20:
            check = 1;
            break;
        case 30:
            check = 2;
            break;
        default:
            check = 3;
    }
    
    if (check != 2) {
        return 2;  // Switch failed
    }
    
    return 0;  // Success
}
