# Contributing to AfriOS

Thank you for your interest in contributing to AfriOS! This document provides guidelines and instructions for contributing.

## Code of Conduct

We are committed to providing a welcoming and inspiring community for all. Please read our Code of Conduct before contributing.

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/AfriOS.git`
3. Create a feature branch: `git checkout -b feature/your-feature-name`
4. Make your changes
5. Commit with clear messages: `git commit -m "Add feature: description"`
6. Push to your fork: `git push origin feature/your-feature-name`
7. Create a Pull Request

## Development Setup

### Requirements
- CMake 3.16 or later
- Python 3.8+
- GCC 10+ or Clang 12+
- Vulkan SDK
- Git

### Setup
```bash
./scripts/setup.sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

## Coding Standards

### C/C++ Style
- Use 4 spaces for indentation
- Max line length: 100 characters
- Use `snake_case` for variables and functions
- Use `PascalCase` for classes
- Include guards: `#ifndef CLASS_NAME_H` / `#define CLASS_NAME_H`

### Python Style
- Follow PEP 8
- Use 4 spaces for indentation
- Run `black` and `pylint` before committing

### Commit Messages
```
[Type]: Brief description

Longer explanation if needed.

Type: feat, fix, docs, test, refactor, perf, chore
```

Examples:
- `feat: Add support for ARM64 architecture`
- `fix: Resolve memory leak in GC`
- `docs: Update build instructions`

## Pull Request Process

1. Update documentation for any new features
2. Add tests for new functionality
3. Ensure all tests pass: `ctest --test-dir build`
4. Request review from maintainers
5. Address feedback and iterate
6. Squash commits if requested

## Areas for Contribution

### High Priority
- [ ] Kernel driver implementations
- [ ] Android framework completeness
- [ ] Graphics optimization
- [ ] Performance profiling

### Medium Priority
- [ ] HarmonyOS compatibility expansion
- [ ] iOS/macOS support
- [ ] Windows/Wine integration testing
- [ ] Documentation improvements

### Good for Beginners
- [ ] Documentation fixes
- [ ] Test additions
- [ ] Build system improvements
- [ ] Utility scripts

## Testing

All contributions must include:
- Unit tests for new functions
- Integration tests for components
- Performance benchmarks (if applicable)

Run tests:
```bash
cmake --build build --target test
ctest --test-dir build --output-on-failure
```

## Documentation

- Update `docs/` for architectural changes
- Add code comments for complex logic
- Include examples for new APIs
- Update README for major features

## Questions?

- Open an issue for questions
- Check existing issues first
- Join our Discord community
- Email: team@afrios.dev

Thank you for contributing to AfriOS!
