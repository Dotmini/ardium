# Topic: File I/O
# Write to test.txt

def main():
    try:
        with open("test.txt", "w") as f:
            f.write("Hello Python File IO")
        print("Written to test.txt")
    except IOError:
        print("Failed to open file")

if __name__ == "__main__":
    main()
