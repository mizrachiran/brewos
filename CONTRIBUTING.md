# Contributing to BrewOS

Thank you for your interest in contributing to BrewOS! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [How to Contribute](#how-to-contribute)
- [Development Setup](#development-setup)
- [Code Style](#code-style)
- [Testing](#testing)
- [Pull Request Process](#pull-request-process)
- [Safety Guidelines](#safety-guidelines)

---

## Code of Conduct

This project adheres to a [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to the maintainers.

---

## Getting Started

1. **Read the documentation** - Familiarize yourself with the [Architecture](docs/pico/Architecture.md) and [Requirements](docs/pico/Requirements.md)
2. **Set up your environment** - Follow the [Setup Guide](SETUP.md)
3. **Find something to work on** - Check [open issues](https://github.com/mizrachiran/brewos/issues) or propose a new feature
4. **Discuss first** - For major changes, open an issue to discuss before coding

---

## How to Contribute

### Reporting Bugs

Before reporting a bug:
1. Check if the issue already exists
2. Collect relevant information (firmware version, machine type, logs)
3. Create a minimal reproducible example if possible

When reporting:
- Use the bug report template
- Include clear steps to reproduce
- Attach relevant logs or screenshots
- Specify your hardware configuration

### Suggesting Features

We welcome feature suggestions! Please:
1. Check existing issues and discussions
2. Describe the use case clearly
3. Consider how it fits with existing architecture
4. Be open to feedback and iteration

### Contributing Code

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Test thoroughly
5. Commit with clear messages
6. Push to your fork
7. Open a Pull Request

### Contributing Documentation

Documentation improvements are highly valued! This includes:
- Fixing typos or unclear explanations
- Adding examples
- Improving diagrams
- Translating documentation
- Writing tutorials

---

## Development Setup

### Prerequisites

```bash
# Pico development
brew install cmake  # macOS
# Install ARM toolchain from https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
export PICO_SDK_PATH=~/pico-sdk

# ESP32 development
pip install platformio
```

### Building

```bash
# Pico firmware
cd src/pico
mkdir build && cd build
cmake -DBUILD_ALL_MACHINES=ON ..
make -j4

# ESP32 firmware
cd src/esp32
pio run
```

### Running Tests

```bash
# Run unit tests (when available)
cd src/pico/build
ctest

# Run linting
# (Add your linting commands here)
```

---

## Code Style

### C Code (Pico)

- Use 4-space indentation (no tabs)
- Opening braces on same line
- Descriptive variable names (snake_case)
- Function names: `module_action_object()` (e.g., `safety_check_temperature()`)
- Constants: `UPPER_SNAKE_CASE`
- Document public functions with Doxygen-style comments

```c
/**
 * Check if temperature is within safe limits
 * 
 * @param temp_c Temperature in Celsius
 * @return true if safe, false if over limit
 */
bool safety_check_temperature(float temp_c) {
    if (temp_c > SAFETY_MAX_TEMP_C) {
        safety_trigger_shutdown(FAULT_OVER_TEMP);
        return false;
    }
    return true;
}
```

### C++ Code (ESP32)

- Follow Arduino/ESP32 conventions
- Use `camelCase` for functions and variables
- Use `PascalCase` for classes
- Prefer `const` and references where appropriate

### JavaScript/HTML/CSS (Web UI)

- Use 2-space indentation
- Use modern ES6+ syntax
- Keep functions small and focused
- Comment complex logic

---

## Testing

### Before Submitting

1. **Build all targets** - Ensure all machine types compile
   ```bash
   cmake -DBUILD_ALL_MACHINES=ON ..
   make
   ```

2. **Test on hardware** - If you have access to hardware, test there

3. **Simulation mode** - Test with `HW_SIMULATION_MODE=1` for basic validation

4. **Check for warnings** - Code should compile without warnings

### Safety-Critical Code

Changes to safety-critical modules require extra scrutiny:

| Module | Criticality | Review Requirements |
|--------|-------------|---------------------|
| `safety.c` | 🔴 Critical | Two approvals required |
| `control_*.c` | 🟠 High | Hardware testing required |
| `sensors.c` | 🟠 High | Calibration verification |
| `protocol.c` | 🟡 Medium | Protocol compatibility |
| `web_server.cpp` | 🟢 Normal | Standard review |

---

## Pull Request Process

### Before Opening a PR

- [ ] Code compiles without errors or warnings
- [ ] All machine types build successfully
- [ ] Changes are tested (hardware or simulation)
- [ ] Documentation is updated
- [ ] Commit messages are clear and descriptive
- [ ] Branch is up to date with `main`

### PR Title Format

Use conventional commit format:

- `feat: Add flow meter support`
- `fix: Correct temperature offset calculation`
- `docs: Update wiring diagram for ESP32`
- `refactor: Simplify PID implementation`
- `chore: Update dependencies`

### Review Process

1. Automated checks run on PR
2. Maintainer reviews code
3. Address feedback (push new commits)
4. Maintainer approves and merges

### After Merge

- Delete your feature branch
- Update your fork's main branch
- Celebrate! 🎉

---

## Safety Guidelines

### General Principles

1. **Fail safe** - When in doubt, turn off heaters
2. **Defense in depth** - Multiple layers of protection
3. **Clear error handling** - Never silently ignore errors
4. **Explicit over implicit** - Make safety checks obvious

### Mandatory Safety Features

These must never be disabled or bypassed:

- Watchdog timer (2-second timeout)
- Over-temperature shutdown (165°C)
- Water level interlocks
- Communication timeout handling

### Code Review for Safety

Safety-related changes require:
1. Clear explanation of safety implications
2. Analysis of failure modes
3. Testing under fault conditions
4. Approval from a maintainer familiar with safety code

---

## Questions?

- Open a [Discussion](https://github.com/mizrachiran/brewos/discussions)
- Check existing [Issues](https://github.com/mizrachiran/brewos/issues)
- Read the [Documentation](docs/README.md)

---

Thank you for contributing to BrewOS! ☕

