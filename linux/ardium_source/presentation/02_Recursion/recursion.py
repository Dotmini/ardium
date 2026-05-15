# Topic: Recursion
# Calculate Fibonacci(10)

def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

def main():
    result = fib(10)
    print(f"Fibonacci(10) = {result}")

if __name__ == "__main__":
    main()
