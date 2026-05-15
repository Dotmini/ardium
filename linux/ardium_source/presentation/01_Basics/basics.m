// Topic: Basic Control Flow
// Count 0-9 and identify Even/Odd

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        int i = 0;
        while (i < 10) {
            if (i % 2 == 0) {
                NSLog(@"Count: %d (Even)", i);
            } else {
                NSLog(@"Count: %d (Odd)", i);
            }
            i++;
        }
    }
    return 0;
}
