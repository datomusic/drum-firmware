# CLAUDE.md - Coding Guidelines

## Core Philosophy
Write code that tells a story. Optimize for changeability over cleverness. Future developers should understand the intent without deciphering puzzles.

For embedded systems: Write code that works reliably in the real world. Consider resource constraints, failure modes, and the physical environment where your code will run.

## Language & Style Standards

### Formatting & Naming
- **Indentation**: 2 spaces
- **C++20** with Pico SDK integration
- **Naming conventions**:
  - `snake_case` for functions, methods, variables
  - `UPPER_SNAKE_CASE` for macros and constants
  - `PascalCase` for `enum class` members
  - Avoid `_t` suffix for types
- **Braces**: Always use `{}` for control structures
- **Namespaces**: Use to prevent naming collisions

### File Organization
- **Headers**: Use include guards, wrap C SDK in `extern "C"`, minimal includes
- **Sources**: Include corresponding header first, use anonymous namespaces
- **Documentation**: Doxygen-style comments only for complex/non-obvious code

## Design Principles

### Dependencies & Coupling
- **Depend on abstractions, not concretions**
- Wrap C SDK hardware in C++ RAII classes
- Use dependency injection to make relationships explicit
- If changing class A forces changes in class B, examine their coupling

### Size Constraints
- **Methods**: 5 lines or fewer (signals, not rules)
- **Classes**: 100 lines or fewer
- **Method parameters**: 4 or fewer
- Large size often indicates multiple responsibilities

### Single Responsibility & Clarity
- Each class should have one reason to change
- Method names should reveal intent, not implementation
- Code should read like well-written prose
- Extract classes when describing with "and"

### Resource Management (Embedded Focus)
- **No dynamic allocation**: Avoid `new`/`malloc`
- **Fixed-size containers** preferred
- **Stack awareness**: Monitor stack usage
- **RAII wrappers**: For hardware resources and locks (e.g., `SpinLockGuard`)
- **Memory layout**: Use `constexpr` for register offsets and bit masks

### Hardware Interface Design
- **Abstraction layers**: Separate hardware-specific code from business logic
- **Volatile access**: Encapsulate in methods
- **Atomic operations**: Use SDK functions for hardware, `<atomic>` for software
- **Platform abstraction**: Use `#if PICO_RP2040` for target-specific code

### Error Handling & Concurrency
- **Return error codes**: Use `pico_error_codes` or custom enums
- **Optional values**: Use `std::optional<T>` (C++20)
- **Unrecoverable errors**: Use `panic()`
- **Assertions**: Use `assert()` and `static_assert`
- **Avoid exceptions**: In performance-critical/ISR code
- **Concurrency**: RAII wrappers around `spin_lock_t`

### Types & Testing
- **Fixed-width types**: Use `std::uint32_t` etc. from `<cstdint>`
- **Strong typing**: Use `enum class` for enumerations
- **Testability**: Design for testing without target hardware
- **Modularity**: Make components testable on desktop

## Code Review Checklist

### Readability & Design
1. **Can I understand what this does without explanation?**
2. **If requirements change, how many files need modification?**
3. **Does each method do one thing well?**
4. **Are dependencies explicit and minimal?**

### Embedded Systems
5. **What happens if this fails? How will we know?**
6. **Are resource constraints considered (memory, timing, power)?**
7. **Is hardware-specific code isolated from business logic?**
8. **Can this be tested without target hardware?**
9. **How does this behave during startup, shutdown, and error conditions?**

### Platform Integration
10. **Are C SDK headers properly wrapped in `extern "C"`?**
11. **Are volatile register accesses encapsulated?**
12. **Is dynamic allocation avoided in critical paths?**

## Red Flags

### Design Issues
- Classes with many instance variables
- Methods with complex conditionals
- Tight coupling between unrelated concepts
- Names that don't reveal intent
- Code that's difficult to test

### Embedded Concerns
- Dynamic memory allocation in critical paths
- Long-running code in interrupt handlers
- No error handling for hardware failures
- Hardware registers accessed directly in business logic
- Missing consideration for power states or timing constraints
- C SDK headers included without `extern "C"` wrapper

## Remember
Perfect is the enemy of good. Apply these principles pragmatically. The goal is maintainable, understandable code that serves the business needs.

For embedded systems: Real-world constraints matter. Code must work reliably in its physical environment, not just in ideal conditions. Plan for failure modes and resource limitations from the start.

The code runs on an RP2350 microcontroller. It is the successor of the RP2040 and has a Cortex M33 core architecture

**AI Editor Note**: Preserve existing behavior unless explicitly instructed otherwise. Avoid end-of-line comments explaining edits.
- commit messages are prefixed with a "Conventional Commits" prefix like "fix: resolve random crash" or "feat: add MIDI note off support"
- run pre-commit on all touched files before committing
- mark unused variables [[maybe_unused]]
- use git rm to remove files if they were checked into git