#!/usr/bin/env bash
set -euo pipefail

# Install Go if missing
if ! command -v go >/dev/null 2>&1; then
    echo "Go not found, installing..."
    if [[ "$(uname)" == "Darwin" ]]; then
        if command -v brew >/dev/null 2>&1; then
            brew install go
        else
            echo "Homebrew not found. Install Go manually from https://go.dev/dl/" >&2
            exit 1
        fi
    elif [[ "$(uname)" == "Linux" ]]; then
        if command -v apt-get >/dev/null 2>&1; then
            sudo apt-get update && sudo apt-get install -y golang-go
        elif command -v dnf >/dev/null 2>&1; then
            sudo dnf install -y golang
        else
            echo "Unsupported Linux distro. Install Go manually from https://go.dev/dl/" >&2
            exit 1
        fi
    else
        echo "Unsupported OS. Install Go manually from https://go.dev/dl/" >&2
        exit 1
    fi
fi

# Ensure GOPATH/bin is on PATH so freshly installed Go tools are found
GOBIN="$(go env GOPATH)/bin"
export PATH="$GOBIN:$PATH"

# Install mage if missing
if ! command -v mage >/dev/null 2>&1; then
    echo "mage not found, installing..."
    go install github.com/magefile/mage@latest
fi

npm install
npm run build
mage -v buildAll
