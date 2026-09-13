import unittest

from perkalian import multiply

class TestPerkalian(unittest.TestCase):
    def test_positive_numbers(self):
        self.assertEqual(multiply(2, 3), 6)
        self.assertEqual(multiply(5, 7), 35)

    def test_negative_numbers(self):
        self.assertEqual(multiply(-1, 5), -5)
        self.assertEqual(multiply(-2, -3), 6)

    def test_zero(self):
        self.assertEqual(multiply(0, 10), 0)
        self.assertEqual(multiply(7.5, 4), 30.0)

if __name__ == "__main__":
    unittest.main()