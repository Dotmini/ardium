#!/bin/bash
set -e

echo "🚀 [Major] Building Arc Language CLI (arc)..."
make build

echo "✅ 'arc' CLI built successfully."
echo "--------------------------------"
./arc doctor

