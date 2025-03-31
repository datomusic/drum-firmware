# DRUM Development Guidelines (Modern C++ Focus)
These guidelines make sure the code is consistent and maintainable while keeping good interaction
with the raspberry pi pico-sdk

 1 Language and Style:
    - Indentation: 2 spaces
    - Primarily use C++17
    - Follow a consistent naming convention:
       - snake_case for functions, methods, and variables remains acceptable (common in embedded C++).
       - UPPER_SNAKE_CASE for macros and compile-time constants (enum class members can be PascalCase or
         UPPER_SNAKE_CASE).
       - Avoid the _t suffix for types; use direct class/struct names.
    - Use namespaces to prevent naming collisions, especially for project-specific code.
    - Keep lines reasonably short and use braces {} for all control structure bodies.
 2 Header Files (.h, .hpp):
    - Include Guards: Always use include guards (#ifndef...) or #pragma once (prefer guards for maximum
      compatibility if unsure).
    - C SDK Interaction: When including C SDK headers, wrap them in extern "C" { ... } if the C++ header
      might be included by other C++ files.
    - C API Exposure: If a C++ header defines functions intended to be called from C code, declare those
      functions with extern "C".
    - Minimal Includes: Still crucial.
    - Interface Definition: Define interfaces using class and struct. Use public, private, protected
      access specifiers. Prefer defining member functions (methods) within the class/struct for behavior.
    - Inline Functions/Methods: Define small, non-virtual member functions within the class definition in
      the header for implicit inlining. Use the inline keyword sparingly otherwise. Templates are
      inherently header-based.
    - Documentation: Use Doxygen-style comments.
 3 Source Files (.cpp):
    - Implementation: Implement non-trivial class member functions and free functions here.
    - Internal Linkage: Use anonymous namespaces for functions and variables intended only for internal
      use within that source file (preferred over static for non-class members). static is still used for
      class static members.
    - Include Corresponding Header First: Still good practice.
 4 Platform Abstraction and Portability:
    - Hardware Abstraction: Wrap C SDK hardware abstractions in C++ classes. Use RAII (Resource
      Acquisition Is Initialization) principles for resources like peripherals or pins. Consider template
      classes for generic hardware interfaces.
    - Target-Specific Code: Conditional compilation (#if PICO_RP2040, etc.) remains the primary tool. if
      constexpr can be useful within C++ templates/functions for compile-time conditional logic.
    - Common Code: Still relevant.
    - Platform Macros: Respect SDK macros (__not_in_flash, etc.) when necessary, potentially wrapping
      their usage within C++ abstractions.
 5 Hardware Interaction:
    - Volatile Access: Still required. Encapsulate volatile register access within methods of peripheral
      classes. Use volatile with std::uint32_t etc.
    - Atomic Operations:
       - For hardware atomic register operations (like Pico's SET/CLR/XOR aliases), continue using the
         SDK's C functions/macros, wrapped in safe C++ helper methods.
       - For software atomicity (if needed between threads/cores without specific hardware support), use
         C++ <atomic>.
    - Register Definitions: Use constexpr or static constexpr members within peripheral classes for
      register offsets and bit masks instead of #define where possible.
 6 Error Handling and Assertions:
    - Error Reporting:
       - Avoid exceptions in performance-critical or ISR code.
       - Return pico_error_codes (or custom error enums/codes) from functions that can fail.
       - Use std::optional<T> (C++17) to return values that might optionally be absent (e.g., instead of
         PICO_ERROR_NO_DATA).
       - Consider std::expected<T, E> (C++23) if available and appropriate.
    - Panics: Still use panic(...) for unrecoverable errors.
    - Assertions: Use C assert() or create a C++ assertion mechanism. Use C++ static_assert for
      compile-time checks.
 7 Concurrency:
    - Spinlocks: Strongly recommended: Create an RAII wrapper class (e.g., SpinLockGuard) around the SDK's
      spin_lock_t and functions (spin_lock_blocking, spin_unlock). This automatically acquires the lock on
       lock on construction and releases it on destruction (even during stack unwinding if exceptions were
       enabled), significantly improving safety.
     - Core Identification: Still use get_core_num().
  8 Types and Data Structures:
     - Use fixed-width integer types from <cstdint> (e.g., std::uint32_t, std::int8_t).
     - Use C++ bool.
     - Use class or struct for data aggregation. Provide constructors for initialization and member
       functions for operations. Use enum class for strongly-typed enumerations.
  9 Build System and Configuration:
     - Configuration: Still need to interact with the C build system's defines (PICO_NO_FPGA_CHECK, etc.).
     - Weak Symbols: Still relevant for allowing overrides of default behavior, especially when linking C
       and C++ code. C++ virtual functions provide an alternative within pure C++ object hierarchies.
       
## Embedded Guidelines
- Avoid dynamic memory allocation (no new/malloc)
- Prefer fixed-size containers over dynamic ones
- Be mindful of stack usage
- Use const and static appropriately to optimize memory usage