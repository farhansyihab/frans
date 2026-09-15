// ~/AgenAI/frans/tests/fixtures/cpp/circle.cpp
#include "circle.hpp"

// Assertion #5: imports dari circle.cpp mengandung "circle.hpp".

Circle::Circle(double r) : radius_(r) {}

Circle::~Circle() {}

double Circle::area() const {
    // Assertion #3: snippet "Circle::area" dari .cpp ini.
    return 3.141592653589793 * radius_ * radius_;
}

double Circle::radius() const {
    return radius_;
}

double Circle::perimeter() const {
    // Assertion #12: panggil method via this-> ... wait, ini top-level method,
    // tidak ada `this->` di sini. Kita tambahkan `this->radius_` (field access
    // bukan call) — ini jadi bagian body saja.
    return 2.0 * 3.141592653589793 * this->radius_;
}
