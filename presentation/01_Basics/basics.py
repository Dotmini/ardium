# Topic: Basic Control Flow
# Count 0-9 and identify Even/Odd

def main():
    i = 0
    while i < 10:
        if i % 2 == 0:
            print(f"Count: {i} (Even)")
        else:
            print(f"Count: {i} (Odd)")
        i += 1

if __name__ == "__main__":
    main()
