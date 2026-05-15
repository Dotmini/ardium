// Benchmark: Sieve of Eratosthenes

#import <Foundation/Foundation.h>

int run_sieve(int n) {
    int count = 0;
    // Using C array for best performance
    char *flags = malloc(n * sizeof(char));
    memset(flags, 1, n); // 1 = True
    
    int i = 2;
    while (i * i < n) {
        if (flags[i]) {
            int j = i * i;
            while (j < n) {
                flags[j] = 0;
                j += i;
            }
        }
        i++;
    }
    
    for (int k = 2; k < n; k++) {
        if (flags[k]) {
            count++;
        }
    }
    free(flags);
    return count;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        int n = 100000000;
        NSLog(@"Finding primes up to %d...", n);
        NSDate *start = [NSDate date];
        int c = run_sieve(n);
        NSTimeInterval diff = -[start timeIntervalSinceNow];
        NSLog(@"Found: %d", c);
        NSLog(@"Time: %f", diff);
    }
    return 0;
}
