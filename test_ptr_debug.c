int main() {
    int arr[5];
    int* aptr;
    
    arr[0] = 5;
    arr[1] = 10;
    arr[2] = 15;
    arr[3] = 20;
    arr[4] = 25;
    
    aptr = arr;
    
    int value = aptr[3];
    
    if (value != 20) {
        return 1;
    }
    
    return 0;
}
