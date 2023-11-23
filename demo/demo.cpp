#include <iostream>

class Test {
public:
  volatile int x[16];

  Test() {}
};

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " index value\n";
    return 1;
  }

  int index = std::stoi(argv[1]);
  int value = std::stoi(argv[2]);

  auto *t = new Test();
  t->x[index] = value;

  std::cout << "x[" << index << "] = " << value << "\n";
}
