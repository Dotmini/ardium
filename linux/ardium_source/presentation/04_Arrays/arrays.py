# Topic: Arrays (Lists)
# Sum elements of an array

def main():
    size = 5
    arr = []

    # Fill
    for i in range(size):
        arr.append(i * 10)

    # Sum
    total = 0
    for val in arr:
        total += val

    print(f"Sum: {total}")

if __name__ == "__main__":
    main()
