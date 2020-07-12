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

int main() {
   int a = 5;
   int *b = &a;
   int c = (*b);
   int size = sizeof(size);

    int d = 3;
    if(size < 3) {
        d = d + 5;
    } else {
        d = d + 2;
    }

   {
      int var = 0;
      int2 = &var;
      int3 = &int2;
      **int3 += 5;
   }

   int (*testFn)(int, int, int, int, int, int, int, int, int, int j) = test;
   putchar(testFn(1,2,3,4,5,6,7, sizeof(int*), 9,72));
   getFoo2();

   return a + c + (int)b + d;
}