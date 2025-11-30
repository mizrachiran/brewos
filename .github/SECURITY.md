# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in BrewOS, please report it responsibly.

### For Software Security Issues

Please **do not** open a public GitHub issue for security vulnerabilities. Instead:

1. Email the maintainers directly (contact@brewos.io)
2. Or use GitHub's private vulnerability reporting feature

Include:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

We will acknowledge receipt within 48 hours and provide a detailed response within 7 days.

### For Safety-Critical Issues

**IMPORTANT:** If you discover a bug that could cause:
- Overheating or fire risk
- Electrical shock hazard
- Water damage
- Any physical harm

Please report immediately with **[SAFETY]** in the subject line. These issues receive highest priority.

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |

## Safety Design Principles

BrewOS implements multiple layers of safety:

1. **Hardware watchdog** - 2-second timeout, automatic heater shutoff
2. **Temperature limits** - Hard-coded 165°C maximum
3. **Water level interlocks** - Heaters disabled without water
4. **Fail-safe defaults** - All outputs off on any error

Any changes to safety-critical code require:
- Explicit review by maintainers
- Testing on hardware under fault conditions
- Documentation of safety implications

