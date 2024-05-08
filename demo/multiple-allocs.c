int test(int index) {
  volatile int x[32];
  volatile int y[32];
  volatile int z[64];
  return x[index] + y[index] + z[index];
}

int main(int argc, char **argv) { return test(argc); }
