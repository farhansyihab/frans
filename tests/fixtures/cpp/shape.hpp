// ~/AgenAI/frans/tests/fixtures/cpp/shape.hpp
#pragma once

// Base class untuk test inheritance + method signature tanpa body.
//
// Blueprint §10a.7:
//   - Assertion #1: parse shape.hpp saja → snippet "Shape" (class)
//     + "Shape::area" (method, signature-only) + "Shape::perimeter" (method).
//   - Assertion #3: dual-file merge → findBySymbol("Shape::area")
//     mengembalikan 2 snippet (signature dari .hpp + body dari .cpp).
//   - Assertion #4: findBySymbol("Circle") → inherits Shape.
class Shape {
public:
    virtual ~Shape() {}
    virtual double area() const;
    virtual double perimeter() const;
};
