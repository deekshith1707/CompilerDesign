void modify_array(int* arr, int size) {
    int i = 0;
    while (i < size) {
        *(arr + i) = *(arr + i) * 2;
        i = i + 1;
    }
}

int main() {
    int arr[3];
    
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    
    modify_array(arr, 3);
    
    if (arr[0] != 2) {
        return 1;
    }
    if (arr[2] != 6) {
        return 2;
    }
    
    return 0;
}
