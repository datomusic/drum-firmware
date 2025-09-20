# CODER.md - DRUM Firmware Development Guide

## Project Overview
DRUM is a MIDI drum machine/sequencer running on RP2350 microcontroller. Features 4 tracks with pressure-sensitive pads, step sequencer, real-time effects, MIDI I/O, and A/B firmware partitions for safe updates.

**Architecture:** See README.md for full structure. Key modules:
- `drum/`: Main application 
- `musin/`: Core library (drivers, HAL, audio, UI, USB)
- Hardware abstraction separates business logic from platform code

## Core Principles
**Write code that tells a story.** Optimize for changeability over cleverness. Code must work reliably in real-world conditions with resource constraints and failure modes.

### Size Guidelines (signals, not rules)
- Methods: ≤5 lines
- Classes: ≤100 lines  
- Parameters: ≤4 per method
- Single responsibility per class

## Language Standards

### C++20 with Pico SDK
- **Indentation:** 2 spaces
- **Naming:** `snake_case` functions/variables, `UPPER_SNAKE_CASE` constants, `PascalCase` enum class members
- **Braces:** Always use `{}` for control structures
- **Headers:** Include guards, wrap C SDK in `extern "C"`, minimal includes
- **Sources:** Include header first, use anonymous namespaces

### Types & Memory
- **Fixed-width types:** `std::uint32_t` from `<cstdint>`
- **No dynamic allocation:** Avoid `new`/`malloc`
- **Fixed-size containers** preferred
- **RAII wrappers** for hardware resources (e.g., `SpinLockGuard`)
- **Stack awareness:** Monitor usage

## Embedded Specifics

### Hardware Interface
- **Abstraction layers:** Separate hardware from business logic
- **Volatile access:** Encapsulate in methods
- **Atomics:** SDK functions for hardware, `<atomic>` for software
- **Register constants:** Use `constexpr` for offsets/masks

### Error Handling
- **Return codes:** `pico_error_codes` or custom enums
- **Optional values:** `std::optional<T>` (C++20)
- **Unrecoverable:** Use `panic()`
- **Avoid exceptions:** In performance-critical/ISR code
- **Assertions:** Use `assert()` and `static_assert`

### Real-time Audio Constraints
- **No dynamic allocation** in audio paths
- **Deterministic timing** required
- **Hardware failure handling** essential
- **Power state awareness** needed

## Development Workflow

### Building & Testing
```bash
./build.sh [options]  # See README.md for full options
cd test && ./run_all_tests.sh
```

### Code Quality
- **Pre-commit hooks:** Run `pre-commit` before commits
- **Conventional commits:** `fix:` `feat:` `refactor:` etc.
- **Branch strategy:** Don't commit on `main` - use feature branches

### AI Editor Guidelines
- **Preserve behavior** unless explicitly instructed
- **Remove code** instead of commenting out
- **Mark unused:** `[[maybe_unused]]` for variables or ask whether it can be removed
- **No end-of-line comments** explaining edits
- **Use `gh`** for GitHub operations
- **git rm** for tracked file removal

## Key Dependencies
- **ETL library:** Use `etl/observer.h` (not `etl/observable.h`)
- **Composition over inheritance**
- **Modern C++20** patterns where appropriate
- **SDK integration:** Respect Pico SDK patterns and macros

## Testing Philosophy
Use dependency injection and abstractions to enable comprehensive testing of business logic.
Always test behaviour. Never test implementation

**Reference:** See CONVENTIONS.md for detailed C++/SDK interaction patterns.
**Build details:** See README.md and drum/README.md for comprehensive build/MIDI specs.
