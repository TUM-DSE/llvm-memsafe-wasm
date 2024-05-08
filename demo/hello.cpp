#include <iostream>

extern "C" {
unsigned long __cxa_allocate_exception(unsigned long) { abort(); }

void __cxa_throw(unsigned long, unsigned long, unsigned long) { abort(); }
}

int main() {
  std::cout << "Hello, World!\n";
  return 0;
}
