// Topic: Advanced Global Scope
// Manage Global State

#import <Foundation/Foundation.h>

static NSString *AppStatus = @"Active";
static int AppUsers = 0;

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"Current Status: %@", AppStatus);
        
        AppStatus = @"Maintenance";
        
        NSLog(@"New Status: %@", AppStatus);
    }
    return 0;
}
