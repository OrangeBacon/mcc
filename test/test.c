int* malloc(int);

int main() {
    int size = 5;
    int* arr = malloc(size * sizeof(int));

    for(int i = 0; i < size; i++) {
        int* addr = arr + i;
        *addr = 0;
        *addr += i;
    }
}