// Topic: File I/O
// Write to test.txt

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSString *str = @"Hello Obj-C File IO";
        NSString *filename = @"test.txt";
        NSError *error;
        
        [str writeToFile:filename atomically:YES encoding:NSUTF8StringEncoding error:&error];
        
        if (error) {
            NSLog(@"Failed to open file");
        } else {
            NSLog(@"Written to test.txt");
        }
    }
    return 0;
}
