# Ardium Language Extension for VS Code

Provides rich language support for the Ardium programming language.

## Features

- **Syntax Highlighting**: Full TextMate grammar for `.ar` files
- **Real-time Diagnostics**: Parse errors shown as red underlines instantly
- **Go to Definition**: Ctrl+Click on function names to jump to their definition
- **Hover Information**: See function signatures on hover

## Requirements

- Ardium compiler (`arc`) must be installed and available in PATH
- Or set `ardium.compilerPath` in VS Code settings

## Installation

### From Source

```bash
cd vscode-ardium
npm install
npm run compile
code --install-extension ardium-1.0.0.vsix
```

### Quick Test (Developer Mode)

1. Open the `vscode-ardium` folder in VS Code
2. Press F5 to launch Extension Development Host
3. Open any `.ar` file to test

## Configuration

| Setting | Description | Default |
|---------|-------------|---------|
| `ardium.compilerPath` | Path to the `arc` executable | `arc` |

## Known Issues

- Incremental document sync not yet implemented (full document sent on each change)

## Release Notes

### 1.0.0

- Initial release with diagnostics, hover, and go-to-definition
