import math


class Shape:
    def area(self):
        return 0.0


class Circle(Shape):
    def __init__(self, radius):
        self.radius = radius

    def area(self):
        return math.pi * self.radius ** 2


def describe(shape):
    return f"area={shape.area():.2f}"
