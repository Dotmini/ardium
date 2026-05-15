# Topic: GUI (Tkinter)
# Create a Window with a Button

try:
    import tkinter as tk
except ImportError:
    print("Skipping: Tkinter not found")
    import sys
    sys.exit(0)

def on_click():
    print("Button Clicked!")

def main():
    root = tk.Tk()
    root.title("Python GUI")
    
    label = tk.Label(root, text="Hello Python GUI")
    label.pack()
    
    btn = tk.Button(root, text="Click Me!", command=on_click)
    btn.pack()
    
    # root.mainloop() # Blocking call

if __name__ == "__main__":
    main()
