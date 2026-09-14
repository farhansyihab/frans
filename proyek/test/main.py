python
from calculator import Calculator

def main():
    calc = Calculator()
    
    print("Adding 5 and 3:", calc.add(5, 3))  # Expected output: 8
    print("Subtracting 3 from 5:", calc.subtract(5, 3))  # Expected output: 2

if __name__ == "__main__":
    main()