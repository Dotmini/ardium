import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
    // Use the installed 'ar' binary from PATH
    const serverExecutable = 'ar';

    // If the server is just a command, run it with arguments
    const serverOptions: ServerOptions = {
        run: { command: serverExecutable, args: ['lsp'] },
        debug: { command: serverExecutable, args: ['lsp'] }
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'ardium' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/.clientrc')
        }
    };

    client = new LanguageClient(
        'ardiumLsp',
        'Ardium Language Server',
        serverOptions,
        clientOptions
    );

    client.start();

    // --- Command Handlers ---
    let terminal: vscode.Terminal | undefined;

    const runCommand = (cmd: string) => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) { return; }
        const file = editor.document.fileName;

        if (!terminal || terminal.exitStatus) {
            terminal = vscode.window.createTerminal("Ardium");
        }
        terminal.show();
        terminal.sendText(`ar ${cmd} "${file}"`);
    };

    context.subscriptions.push(
        vscode.commands.registerCommand('ardium.run', () => runCommand('run')),
        vscode.commands.registerCommand('ardium.build', () => runCommand('build')),
        vscode.commands.registerCommand('ardium.dev', () => runCommand('dev'))
    );

    vscode.window.showInformationMessage('Ardium LSP Active 🚀');
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
