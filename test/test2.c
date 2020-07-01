

int main() {
   int a = 5;
   int *b = &a;
   b++;
   b--;
   int c = (*b)++;
   return a + c;
}