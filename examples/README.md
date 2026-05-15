# Ardium 2.0.0 - Example Programs

This directory contains working examples demonstrating each framework.

## Core Examples

### hello.ar - Hello World

```ardium
import "Core"

fn main() {
    println("Hello, Ardium 2.0! 🚀")
    return 0
}
```

### string_concat.ar - String Operations

```ardium
import "Core"

fn main() {
    let name = "John"
    let greeting = "Hello, " + name + "!"
    println(greeting)
    
    print("First: ")
    print(name)
    println("")
    
    return 0
}
```

## CoreUI Examples

### simple_button.ar - Basic UI with Button

```ardium
import "Core"
import "CoreUI"

fn on_click() {
    println("Button clicked! 🎉")
}

fn ui() {
    VStack(resolve_symbol("content"))
}

fn content() {
    Title("Simple Button Demo")
    Spacer(30)
    Button("Click Me", resolve_symbol("on_click"))
}

fn main() {
    App("Button Demo", resolve_symbol("ui"))
    return 0
}
```

### login_form.ar - Complete Login Form

```ardium
import "Core"
import "CoreUI"

let username_handle = 0
let password_handle = 0

fn do_login() {
    let user_h = peek(resolve_symbol("username_handle"))
    let pass_h = peek(resolve_symbol("password_handle"))
    
    let username = get_input_value(user_h)
    let password = get_input_value(pass_h)
    
    print("Login attempt: ")
    println(username)
    
    if (username == "admin") {
        println("✅ Access granted!")
    } else {
        println("❌ Access denied!")
    }
}

fn ui() {
    VStack(resolve_symbol("form"))
}

fn form() {
    Title("Login")
    Subtitle("Enter your credentials")
    Spacer(20)
    
    Text("Username:")
    let user = TextField("Enter username")
    poke(resolve_symbol("username_handle"), user)
    
    Spacer(10)
    
    Text("Password:")
    let pass = SecureField("Enter password")
    poke(resolve_symbol("password_handle"), pass)
    
    Spacer(20)
    Button("Login", resolve_symbol("do_login"))
}

fn main() {
    App("Login Demo", resolve_symbol("ui"))
    return 0
}
```

## CoreData Examples

### file_write.ar - Write to File

```ardium
import "Core"
import "CoreData"

fn main() {
    println("Writing to file...")
    Save("/tmp/ardium_test.txt", "Ardium 2.0 is awesome!")
    println("✅ File written successfully")
    return 0
}
```

### file_read.ar - Read from File

```ardium
import "Core"
import "CoreData"

fn main() {
    println("Reading file...")
    let content = Load("/tmp/ardium_test.txt")
    println("Content:")
    println(content)
    return 0
}
```

### save_and_load.ar - Complete I/O Example

```ardium
import "Core"
import "CoreData"

fn main() {
    let data = "User: John\nAge: 25\nStatus: Active"
    
    println("Saving data...")
    Save("/tmp/user.txt", data)
    
    println("Loading data...")
    let loaded = Load("/tmp/user.txt")
    
    println("\nLoaded content:")
    println(loaded)
    
    return 0
}
```

## CoreNetwork Examples

### fetch_url.ar - HTTP GET Request

```ardium
import "Core"
import "CoreNetwork"

fn main() {
    println("Fetching URL...")
    let response = Fetch("https://httpbin.org/get")
    
    println("\nResponse:")
    println(response)
    
    return 0
}
```

## CoreCrypto Examples

### hash_password.ar - SHA-256 Hashing

```ardium
import "Core"
import "CoreCrypto"

fn main() {
    let password = "mysecret123"
    
    println("Hashing password...")
    let hash = SHA256(password)
    
    print("Hash: ")
    println(hash)
    
    return 0
}
```

## CoreKits Examples

### debug_log.ar - Debug Logging

```ardium
import "Core"
import "CoreKits"

fn main() {
    Log("Application starting...")
    
    let data = List()
    Log("Data structure created")
    
    Log("Processing...")
    println("Main work here")
    
    Log("Application finished")
    return 0
}
```

## Combined Examples

### data_and_crypto.ar - Save Encrypted Data

```ardium
import "Core"
import "CoreData"
import "CoreCrypto"

fn main() {
    let secret = "my_secret_data"
    
    println("Encrypting data...")
    let hash = SHA256(secret)
    
    println("Saving encrypted data...")
    Save("/tmp/encrypted.txt", hash)
    
    println("Loading encrypted data...")
    let loaded = Load("/tmp/encrypted.txt")
    
    println("Encrypted data:")
    println(loaded)
    
    return 0
}
```

### ui_with_network.ar - Fetch and Display

```ardium
import "Core"
import "CoreUI"
import "CoreNetwork"

fn fetch_data() {
    println("Fetching from server...")
    let data = Fetch("https://httpbin.org/uuid")
    println(data)
}

fn ui() {
    VStack(resolve_symbol("content"))
}

fn content() {
    Title("Network Demo")
    Spacer(20)
    Button("Fetch Data", resolve_symbol("fetch_data"))
}

fn main() {
    App("Network UI Demo", resolve_symbol("ui"))
    return 0
}
```

## Running Examples

```bash
# Run any example
dune exec -- ardium run examples/hello.ar

# Or compile first
dune exec -- ardium build examples/hello.ar -o hello
./hello
```

## Tips

1. **UI Examples**: Run these to see actual macOS windows
2. **File I/O**: Check `/tmp/` for created files
3. **Network**: Requires internet connection
4. **Combine**: Mix and match frameworks for powerful apps

## Next Steps

- Modify these examples
- Create your own programs
- Read the [API Reference](../docs/API_REFERENCE.md)
- Share your creations!
