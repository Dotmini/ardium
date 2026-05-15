const vscode = require('vscode');
const { LanguageClient, TransportKind } = require('vscode-languageclient/node');

let client;

function activate(context) {
    console.log('Ardium extension is now active!');

    // Find the arc executable
    const arcPath = vscode.workspace.getConfiguration('ardium').get('compilerPath') || 'arc';

    const serverOptions = {
        command: arcPath,
        args: ['lsp'],
        transport: TransportKind.stdio
    };

    const clientOptions = {
        documentSelector: [{ scheme: 'file', language: 'ardium' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.ar')
        }
    };

    client = new LanguageClient(
        'ardiumLanguageServer',
        'Ardium Language Server',
        serverOptions,
        clientOptions
    );

    client.start();
    console.log('Ardium Language Server started');
}

function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}

module.exports = { activate, deactivate };
