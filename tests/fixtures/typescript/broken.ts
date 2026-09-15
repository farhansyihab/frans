function valid() {
    return 1;
}

class Broken {
    method() {
        return 2;
    }
// missing closing brace — sengaja untuk test toleransi syntax error.
