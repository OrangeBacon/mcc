int main();
int (*fn)();

int main() {
   fn = main;
   int a = 5;
   int *b = &a;
   b++;
   b--;
   int c = (*b)++;
   return a + c;
}