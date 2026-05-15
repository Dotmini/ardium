// Topic: Recursion
// Calculate Fibonacci(10)

#import <Foundation/Foundation.h>

int fib(int n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        int result = fib(10);
        NSLog(@"Fibonacci(10) = %d", result);
    }
    return 0;
}
