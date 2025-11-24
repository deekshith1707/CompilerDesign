int main() {
    int intArr[10];
    int i;
    
    // Populate array
    for (i = 0; i < 10; i = i + 1) {
        intArr[i] = i * 10;
    }
    
    // Check values
    if (intArr[0] != 0) return 1;
    if (intArr[3] != 30) return 3;
    if (intArr[9] != 90) return 9;
    
    return 0;
}
