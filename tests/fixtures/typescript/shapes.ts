export class Shape {
    area(): number {
        return 0;
    }
}

export class Circle extends Shape {
    radius(): number {
        return 0;
    }

    area(): number {
        return 3.14159 * this.radius() * this.radius();
    }
}
