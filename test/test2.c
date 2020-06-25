int globl = 5;
int globl2;

int main() {
   int a = 5;
   int b = a + 4;
   a = a - 3;
   a += 3;
   b <<= 2;
   a *= 7 * (a < b);
   b <<= (b != a);
   int c = a + b;
   int* d = &c- &c + &c;
   int e = *d;
   globl2 += 1;
   return e;
}

int globl2 = 7;
int *globl3;

int* a(int b) {return &b;}