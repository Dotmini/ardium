# Topic: Concurrency (Threading)
# Run a task in background

import threading
import time

def worker(arg):
    print(f"Worker running... ID: {arg}")
    time.sleep(1)
    print("Worker done.")

def main():
    print("Main: Starting worker")
    t = threading.Thread(target=worker, args=(1,))
    t.start()
    
    print("Main: Waiting for worker")
    t.join()
    
    print("Main: Done")

if __name__ == "__main__":
    main()
