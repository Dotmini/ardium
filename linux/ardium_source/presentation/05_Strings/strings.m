// Topic: Strings
// Concatenation and Printing

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSString *s1 = @"Hello";
        NSString *s2 = @" World";
        
        NSString *s3 = [s1 stringByAppendingString:s2];
        
        NSLog(@"Result: %@", s3);
    }
    return 0;
}
