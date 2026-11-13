File: CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.16)
project(demo_a44 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
add_executable(demo src/main.cpp)
```

File: src/main.cpp
```cpp
#include <iostream>

int main() {
    // Sengaja salah: identifier tidak dideklarasikan.
    std::cout << undeclared_variable << std::endl;
    return 0;
}
```
