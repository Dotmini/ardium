// Topic: JSON Handling
// Parse JSON string

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSString *jsonStr = @"{\"name\": \"Ardium\", \"vers\": 2}";
        NSData *jsonData = [jsonStr dataUsingEncoding:NSUTF8StringEncoding];
        
        NSDictionary *json = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:nil];
        
        NSLog(@"Name: %@", json[@"name"]);
        NSLog(@"Version: %@", json[@"vers"]);
    }
    return 0;
}
