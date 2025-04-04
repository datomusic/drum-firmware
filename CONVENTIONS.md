# DRUM Development Guidelines (Modern C++ Focus)

These guidelines ensure code is consistent and maintainable while keeping good interaction with the Raspberry Pi Pico SDK.

## 1. Language and Style
- Indentation: 2 spaces
- Primarily use C++17
- Follow consistent naming conventions:
  - `snake_case` for functions, methods, and variables (common in embedded C++)
  - `UPPER_SNAKE_CASE` for macros and compile-time constants (`enum class` members can be `PascalCase` or `UPPER_SNAKE_CASE`)
  - Avoid `_t` suffix for types; use direct class/struct names
- Use namespaces to prevent naming collisions
- Keep lines reasonably short and use braces `{}` for all control structure bodies

## 2. Header Files (.h, .hpp)
- **Include Guards**: Always use `#ifndef...` or `#pragma once` (prefer guards for maximum compatibility)
- **C SDK Interaction**: When including C SDK headers, wrap them in `extern "C" { ... }`
- **C API Exposure**: Declare C-callable functions with `extern "C"`
- **Minimal Includes**: Keep includes minimal
- **Interface Definition**: Define interfaces using `class` and `struct` with proper access specifiers
- **Inline Functions**: Define small member functions in headers for implicit inlining
- **Documentation**: Use Doxygen-style comments *where necessary* to explain complex logic or non-obvious behavior. Avoid verbose comments for self-explanatory code.

## 3. Source Files (.cpp)
- Implement non-trivial functions here
- Use anonymous namespaces for internal linkage (preferred over `static`)
- Include corresponding header first
- Avoid verbose comments for self-explanatory code.

## 4. Platform Abstraction and Portability
- Wrap C SDK hardware in C++ classes using RAII
- Use `#if PICO_RP2040` etc. for target-specific code
- Respect SDK macros like `__not_in_flash`

## 5. Hardware Interaction
- Encapsulate `volatile` register access in methods
- For atomic operations:
  - Use SDK's C functions/macros for hardware operations
  - Use `<atomic>` for software atomicity
- Use `constexpr` for register offsets and bit masks

## 6. Error Handling and Assertions
- Avoid exceptions in performance-critical/ISR code
- Return `pico_error_codes` or custom error enums
- Use `std::optional<T>` (C++17) for optional values
- Use `panic()` for unrecoverable errors
- Use `assert()` and `static_assert`

## 7. Concurrency
- Create RAII wrapper (e.g., `SpinLockGuard`) around `spin_lock_t`
- Use `get_core_num()` for core identification

## 8. Types and Data Structures
- Use fixed-width types from `<cstdint>` (`std::uint32_t`, etc.)
- Use `enum class` for strongly-typed enumerations

## 9. Build System and Configuration
- Interact with C build system defines (`PICO_NO_FPGA_CHECK`, etc.)
- Use weak symbols for behavior overrides

## 10. Embedded Guidelines
- Avoid dynamic memory allocation (no `new`/`malloc`)
- Prefer fixed-size containers
- Be mindful of stack usage
- Use `const` and `static` appropriately