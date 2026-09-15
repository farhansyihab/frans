// ~/AgenAI/frans/tests/fixtures/cpp/main.cpp
#include <iostream>

#include "circle.hpp"

// Assertion #13: shape.area() di main() → calls dari "main" ke "area".
// (obj.method() — kita ambil leaf name-nya saja.)
int main() {
    Circle c(5.0);

    // Direct call — leaf name "area".
    const double a = c.area();

    // Chain call — leaf name "radius".
    const double r = c.radius();

    std::cout << "area=" << a << " radius=" << r << "\n";
    return 0;
}
