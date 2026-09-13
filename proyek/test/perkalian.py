def multiply(a, b):
    return a * b

if __name__ == "__main__":
    # Test cases
    assert multiply(2, 3) == 6, "Test case 1 failed"
    assert multiply(-1, 5) == -5, "Test case 2 failed"
    assert multiply(0, 10) == 0, "Test case 3 failed"
    assert multiply(7.5, 4) == 30.0, "Test case 4 failed"

    print("All tests passed!")