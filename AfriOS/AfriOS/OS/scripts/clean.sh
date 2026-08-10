#!/bin/bash
# Clean build artifacts
# Usage: ./scripts/clean.sh [--deep]

DEEP=${1:-""}

if [ "$DEEP" = "--deep" ]; then
    echo "Performing deep clean..."
    find . -type f \( -name "*.o" -o -name "*.a" -o -name "*.so" -name "*.exe" \) -delete
    find . -type d -name "build" -exec rm -rf {} + 2>/dev/null
    find . -type d -name "cmake_install" -exec rm -rf {} + 2>/dev/null
    echo "Deep clean complete"
else
    echo "Cleaning build artifacts..."
    rm -rf build/*
    echo "Clean complete (use --deep for more aggressive cleaning)"
fi
