# AfriOS Main Makefile
# Unified build system for all repositories

.PHONY: all android kernel runtime graphics harmony ios windows clean distclean test help

# Build variables
BUILD_DIR ?= build
JOBS ?= $(shell nproc)
VERBOSE ?= 0

# Color output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[0;33m
NC := \033[0m

help:
	@echo "$(GREEN)=== AfriOS Build System ===$(NC)"
	@echo ""
	@echo "Available targets:"
	@echo "  $(YELLOW)make all$(NC)              - Build entire AfriOS project"
	@echo "  $(YELLOW)make android$(NC)          - Build Android framework (afros-androsandbox)"
	@echo "  $(YELLOW)make kernel$(NC)           - Build AfriOS kernel (afros-core)"
	@echo "  $(YELLOW)make runtime$(NC)          - Build runtime manager (afros-corebridge-core)"
	@echo "  $(YELLOW)make graphics$(NC)         - Build graphics system (afros-dxvk)"
	@echo "  $(YELLOW)make harmony$(NC)          - Build HarmonyOS compatibility (afros-harmonygate)"
	@echo "  $(YELLOW)make ios$(NC)              - Build iOS compatibility (afros-incompat-engine)"
	@echo "  $(YELLOW)make windows$(NC)          - Build Windows support (afros-winbridge)"
	@echo "  $(YELLOW)make test$(NC)             - Run all tests"
	@echo "  $(YELLOW)make clean$(NC)            - Clean build artifacts"
	@echo "  $(YELLOW)make distclean$(NC)        - Full clean (remove all generated files)"
	@echo ""

all: android kernel runtime graphics harmony ios windows
	@echo "$(GREEN)✓ Build complete!$(NC)"

android:
	@echo "$(GREEN)Building afros-androsandbox...$(NC)"
	@cd afros-androsandbox && m -j$(JOBS)

kernel:
	@echo "$(GREEN)Building afros-core...$(NC)"
	@cd afros-core && $(MAKE) -j$(JOBS)

runtime:
	@echo "$(GREEN)Building afros-corebridge-core...$(NC)"
	@mkdir -p $(BUILD_DIR)/corebridge-core
	@cd $(BUILD_DIR)/corebridge-core && cmake ../../afros-corebridge-core && $(MAKE) -j$(JOBS)

graphics:
	@echo "$(GREEN)Building afros-dxvk...$(NC)"
	@mkdir -p $(BUILD_DIR)/dxvk
	@cd $(BUILD_DIR)/dxvk && meson ../../afros-dxvk && ninja -j$(JOBS)

harmony:
	@echo "$(GREEN)Building afros-harmonygate...$(NC)"
	@cd afros-harmonygate && gn build output

ios:
	@echo "$(GREEN)Building afros-incompat-engine...$(NC)"
	@mkdir -p $(BUILD_DIR)/incompat
	@cd $(BUILD_DIR)/incompat && cmake ../../afros-incompat-engine && $(MAKE) -j$(JOBS)

windows:
	@echo "$(GREEN)Building afros-winbridge...$(NC)"
	@cd afros-winbridge && ./configure && $(MAKE) -j$(JOBS)

test:
	@echo "$(GREEN)Running test suite...$(NC)"
	@cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
	@rm -rf $(BUILD_DIR)/*
	@cd afros-androsandbox && m clean 2>/dev/null || true
	@echo "$(GREEN)Clean complete!$(NC)"

distclean: clean
	@echo "$(YELLOW)Full distclean...$(NC)"
	@rm -rf $(BUILD_DIR)
	@find . -name "*.o" -delete
	@find . -name "*.a" -delete
	@echo "$(GREEN)Distclean complete!$(NC)"

.DEFAULT_GOAL := help
