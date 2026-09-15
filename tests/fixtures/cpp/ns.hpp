// ~/AgenAI/frans/tests/fixtures/cpp/ns.hpp
#pragma once

// Assertion #7: snippet "foo::bar::Baz" (class) dan "foo::bar::Baz::qux" (method).
// Assertion #8: snippet "foo::bar::free_function" (function).
namespace foo {
namespace bar {

class Baz {
public:
    void qux();
};

void free_function();

} // namespace bar
} // namespace foo
