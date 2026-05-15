// Topic: Structs
// Define Vector2 and Dot Product

#import <Foundation/Foundation.h>

typedef struct {
    int x;
    int y;
} Vector2;

int dot(Vector2 a, Vector2 b) {
    return (a.x * b.x) + (a.y * b.y);
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        Vector2 v1 = {10, 20};
        Vector2 v2 = {5, 5};

        int result = dot(v1, v2);
        NSLog(@"Dot Product: %d", result);
    }
    return 0;
}
