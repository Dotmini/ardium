# Topic: JSON Handling
# Parse JSON string

import json

def main():
    json_str = '{"name": "Ardium", "vers": 2}'
    
    data = json.loads(json_str)
    
    print(f"Name: {data['name']}")
    print(f"Version: {data['vers']}")

if __name__ == "__main__":
    main()
