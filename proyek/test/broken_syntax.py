def main():
    try:
        # Attempt to execute code with syntax errors
        print("Hello, World!")
        print("This is a test.")
        print("Another test")
        print("Final test")
    except Exception as e:
        print(f"An error occurred: {e}")
        return 1

if __name__ == "__main__":
    main()