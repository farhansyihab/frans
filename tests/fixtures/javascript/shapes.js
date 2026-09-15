class Shape {
    area() {
        return 0;
    }
}

class Circle extends Shape {
    radius() {
        return 0;
    }

    area() {
        return 3.14159 * this.radius() * this.radius();
    }
}

module.exports = { Shape, Circle };
