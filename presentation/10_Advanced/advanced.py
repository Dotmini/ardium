# Topic: Advanced Global Scope
# Manage Global State

state = {
    "status": "Active",
    "users": 0
}

def main():
    global state
    
    # Python relies on dictionary access or global keyword
    print(f"Current Status: {state['status']}")
    
    state['status'] = "Maintenance"
    
    print(f"New Status: {state['status']}")

if __name__ == "__main__":
    main()
