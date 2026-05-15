// Topic: Concurrency (GCD)
// Run a task in background

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        dispatch_group_t group = dispatch_group_create();
        
        NSLog(@"Main: Starting worker");
        dispatch_group_enter(group);
        
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            NSLog(@"Worker running... ID: 1");
            [NSThread sleepForTimeInterval:1.0];
            NSLog(@"Worker done.");
            dispatch_group_leave(group);
        });
        
        NSLog(@"Main: Waiting for worker");
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        
        NSLog(@"Main: Done");
    }
    return 0;
}
