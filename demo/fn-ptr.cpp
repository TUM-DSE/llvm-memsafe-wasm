#include <stdio.h>
#include <iostream>

extern "C" {
unsigned long __cxa_allocate_exception(unsigned long) { abort(); }

void __cxa_throw(unsigned long, unsigned long, unsigned long) { abort(); }
}

class X {
public:
  virtual int print() {
    printf("X::print\n");
    //std::cout << "X::print\n";
    return 0;
  }
};

class Y : public X {
public:
  int print() override {
    printf("Y::print\n");
    //std::cout << "Y::print\n";
    return 1;
  }
};

int inner(X *x) { return x->print(); }

int main() {
  Y y;
  return inner(&y);
}
