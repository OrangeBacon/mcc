int main();
int (*fn)();
int putchar(int);
int* malloc(int);

int test(int, int, int, int, int, int, int, int, int, int j) {
    return j;
}

int fibCallCount = 0;
int foo2() {
    return fibCallCount;
}

int (*getFoo2())() {
    return foo2;
}

int* int2;
int** int3;

int add2(int *arg) {
    *arg += 2;
    return *arg;
}

int main() {
   int a = 5;
   int *b = &a;
   int c = (*b);
   int size = sizeof(size);

    int d = 3;
    if(size < 3) {
        d += 5;
    } else {
        d += 2;
    }

    while(a > d) {
        a--;
    }

   {
      int var = 0;
      int2 = &var;
      int3 = &int2;
      **int3 += a == 3 ? 5 : add2(&a);
   }

   int (*testFn)(int, int, int, int, int, int, int, int, int, int j) = test;
   putchar(testFn(1,2,3,4,5,6,7, sizeof(int*), 9,72)) || add2(a) && add2(*b);

   return a + c + (int)b + d;
}