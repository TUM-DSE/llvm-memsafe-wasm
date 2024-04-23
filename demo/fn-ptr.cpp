

class X {
public:
  virtual int print() { return 0; }
};

class Y : public X {
public:
  int print() override { return 1; }
};

int func1(int a, int b) { return a + b; }

typedef int (*fn)(int, int);

fn add() { return func1; }

fn x = func1;

int call(fn f) {
  if (!f) {
    return 0;
  }
  return f(1, 2);
}

int inner(X *x) { return x->print(); }

int main() {
  Y y;
  return inner(&y);
}
