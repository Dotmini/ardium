// Topic: Arrays (C-Array)
// Sum elements of an array

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        int size = 5;
        // Using C array for fair low-level comparison, or NSMutableArray for Obj-C style
        // Let's use NSMutableArray to show object overhead vs C style
        NSMutableArray *arr = [NSMutableArray arrayWithCapacity:size];

        // Fill
        for (int i = 0; i < size; i++) {
            [arr addObject:@(i * 10)];
        }

        // Sum
        int sum = 0;
        for (NSNumber *val in arr) {
            sum += [val intValue];
        }

        NSLog(@"Sum: %d", sum);
    }
    return 0;
}
