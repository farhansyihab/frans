// ~/AgenAI/frans/tests/fixtures/cpp/circle.hpp
#pragma once

#include "shape.hpp"

// Assertion #4: Circle inherits Shape.
// Assertion #3 (dual-file merge): Circle::area punya snippet di .hpp
// (signature) dan .cpp (body).
class Circle : public Shape {
public:
    explicit Circle(double r);
    ~Circle() override;

    double area() const override;
    double perimeter() const override;

    double radius() const;

private:
    double radius_;
};
