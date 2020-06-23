int main() {
   int a = 5;
   int b = a + 4;
   a = a - 3;
   a += 3;
   b <<= 2;
   a *= 7 * (a < b);
   b <<= (b != a);
   return a + b;
}