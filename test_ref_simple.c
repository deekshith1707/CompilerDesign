void increment(int* x) {
    *x = *x + 1;
}

int main() {
    int a = 10;
    
    increment(&a);
    
    if (a != 11) {
        return 1;
    }
    
    return 0;
}
