int function_a(int x, int y) {
    return x + y / x - y * x;
}

int main() {
    return function_a(2, 3);
}