#!/bin/bash
# Quick build helper script

BUILD_DIR="build"
JOBS=$(nproc)

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

build_component() {
    local component=$1
    echo -e "${YELLOW}Building ${component}...${NC}"
    case $component in
        android)
            cd afros-androsandbox && m -j$JOBS ;;
        kernel)
            cd afros-core && make -j$JOBS ;;
        runtime)
            cd $BUILD_DIR && cmake --build . --target afros-corebridge-core -- -j$JOBS ;;
        graphics)
            cd afros-dxvk && meson compile -C build ;;
        harmony)
            cd afros-harmonygate && gn gen output && ninja -C output -j$JOBS ;;
        ios)
            cd afros-incompat-engine && make -j$JOBS ;;
        windows)
            cd afros-winbridge && make -j$JOBS ;;
        *)
            echo "Unknown component: $component"
            exit 1
            ;;
    esac
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ ${component} built successfully${NC}"
    else
        echo -e "${RED}✗ Failed to build ${component}${NC}"
        exit 1
    fi
}

if [ -z "$1" ]; then
    echo "Usage: $0 <component>"
    echo "Components: android, kernel, runtime, graphics, harmony, ios, windows, all"
    exit 1
fi

if [ "$1" = "all" ]; then
    for comp in android kernel runtime graphics harmony ios windows; do
        build_component $comp
    done
else
    build_component $1
fi

echo -e "${GREEN}Build complete!${NC}"
