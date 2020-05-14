int fib(int);
int putchar(int);
int* malloc(int);

int fibCallCount = 0;

int test(int, int, int, int, int, int, int, int, int, int j) {
    return j;
}

int foo(int **, int (*)(int, int));

int* bar(int** arg) {
    return *arg;
}

int foo2() {
    return fibCallCount;
}

int (*getFoo2())() {
    return foo2;
}

int main() {
    int a = 5, b, c;
    b = a--;
    ++b;
    --a;
    c = b++;

    int num = 5;
    int* ptrcast = (int*)num;
    int samenum = (int)ptrcast;

    c += samenum;
    a -= 3;
    b /= 2;
    if(a < 2) {
        int x = 3;
        c *= x;
    } else {
        int x = 2;
        a %= x;
    }
    for(a -= 2; a < 11; a+=3);
    for(int i = 3; i < 7; i++) {
        a++;
        b--;
        break;
    }
    do {
        a *= 2;
    } while(a < 100);

    int seven = 5;
    int* j = &seven;
    *bar(&j) = 7;

    int size = 5;
    int* arr = malloc(size * 8);

    for(int i = 0; i < size; i++) {
        *(arr + i) = 0;
        *(arr + i) += i * 2;
        *(arr + (2 * i) - i) -= i;
    }

    int ten = 0;
    for(int i = 0; i < size; i++) {
        arr++;
        arr--;
        ++arr;
        --arr;
        (*arr)++;
        (*arr)--;
        ten += *arr;
        arr += 2;
        arr -= 1;
    }

    while(b < ten) {
        b += 4;
        if(b > seven) {
            a++;
            b--;
            continue;
        }
        c -= 1;
    }
    b <<= 3;
    c >>= 1;
    a &= b > a ? 3 + (b %= 8) : 11 * (c /= 4);
    b |= 7;
    c ^= 9;
    a = (b += 7) * a;

    int (*testFn)(int, int, int, int, int, int, int, int, int, int j) = test;
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,72));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,101));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,108));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,108));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,111));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,44));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,32));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,87));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,111));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,114));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,108));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,100));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,33));
    putchar(testFn(1,2,3,4,5,6,7, 8, 9,10));

    int x = 115;
    int *z = &x;
    (*z) += 5;
    (*&*z) -= 2;
    (*z)++;
    --(*&*&*z);

    int (*(**g)(int a))(int (*b)(int g));

    int fib11 = fib(11);
    for(;;)
    return 101 - (getFoo2()() - (fib11 -((-a + 2 * b + c + x) % 256)));
}

int fib(int n) {
    fibCallCount++;
    if (n == 0 || n == 1) {
        return n;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}