#!/bin/bash
# AfriOS Setup Script
# Initializes development environment

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
    exit 1
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo -e "${GREEN}   AfriOS Development Setup Script     ${NC}"
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo ""

# Check OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    print_status "Detected Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    print_status "Detected macOS"
else
    print_error "Unsupported OS: $OSTYPE"
fi

# Check for required tools
echo ""
echo -e "${YELLOW}Checking prerequisites...${NC}"

check_cmd() {
    if command -v $1 &> /dev/null; then
        print_status "Found $1"
        return 0
    else
        print_warning "Missing $1 - Please install it"
        return 1
    fi
}

MISSING=0
check_cmd "git" || MISSING=1
check_cmd "cmake" || MISSING=1
check_cmd "python3" || MISSING=1
check_cmd "make" || MISSING=1

if [ $MISSING -eq 1 ]; then
    echo ""
    echo "Installing missing dependencies..."
    
    if [ "$OS" = "linux" ]; then
        sudo apt-get update
        sudo apt-get install -y \
            build-essential cmake ninja-build python3 \
            libvulkan-dev spirv-tools clang lld pkg-config
    elif [ "$OS" = "macos" ]; then
        which brew > /dev/null || {
            print_error "Please install Homebrew first: https://brew.sh"
        }
        brew install cmake python@3.10 vulkan-tools spirv-tools
    fi
fi

print_status "All prerequisites installed"

# Create build directory
echo ""
echo -e "${YELLOW}Setting up build environment...${NC}"
mkdir -p build
cd build

# Configure CMake
echo ""
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja

print_status "Build environment configured"

# Initialize git submodules (if any)
cd ..
if [ -f .gitmodules ]; then
    echo ""
    echo -e "${YELLOW}Initializing git submodules...${NC}"
    git submodule update --init --recursive
    print_status "Submodules initialized"
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo -e "${GREEN}   Setup Complete!                     ${NC}"
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo ""
echo "Next steps:"
echo "  1. cd build"
echo "  2. ninja          # Build all components"
echo "  3. ctest          # Run tests"
echo ""
