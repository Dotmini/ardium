# Topic: Structs
# Define Vector2 and Dot Product

class Vector2:
    def __init__(self, x, y):
        self.x = x
        self.y = y

def dot(a, b):
    return (a.x * b.x) + (a.y * b.y)

def main():
    v1 = Vector2(10, 20)
    v2 = Vector2(5, 5)

    result = dot(v1, v2)
    print(f"Dot Product: {result}")

if __name__ == "__main__":
    main()
