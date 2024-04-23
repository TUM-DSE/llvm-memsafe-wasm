#include <stdio.h>

int func1(int a, int b) { return a + b; }

typedef int (*fn)(int, int);

fn add() { return func1; }

int call(fn f) {
  if (!f) {
    return 0;
  }
  return f(1, 2);
}

int main() { printf("call: %d\n", call(add())); }
