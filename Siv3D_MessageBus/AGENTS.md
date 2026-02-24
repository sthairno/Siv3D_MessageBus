# AGENTS.md

## Project overview

- Repository contains the C++ library project `Siv3D_MessageBus` and a test project `Test`.
- The `Test` project uses Google Test and depends on `Siv3D_MessageBus`; building `Test` also builds the library automatically.
- The project targets both Windows (Visual Studio/MSBuild) and Linux (CMake/Ninja); adjust required tools and environment variables per OS.

## Project structure

- Public headers: `include/ThirdParty/MessageBus/`
- Usage: `#include <MessageBus/[...].hpp>`
- External headers are resolved via the above: Siv3D core, fmt (via Siv3D ThirdParty), hiredis (vcpkg)
- hiredis import rule (C interop): Always wrap hiredis headers with `extern "C"` in C++.

```cpp
extern "C" {
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/poll.h>
}
```

## Build commands

- Command line: `python3 scripts/build.py <ProjectName> <Configuration>`
  - ProjectName: `Siv3D_MessageBus` or `Test`
  - Configuration: `Debug` or `Release`

## Requirements

### Common

- Python 3.9+ (for helper scripts), Git, and CMake/vcpkg tooling available on PATH
- Dependencies installed via vcpkg (Siv3D core, fmt, hiredis, etc.)

### Windows (Visual Studio)

- Visual Studio 2022 with C++ Desktop Development workload
- `SIV3D_0_6_16` environment variable configured
- Ability to run the build scripts from MSBuild or Developer PowerShell

### Linux (CMake)

- GCC 11+ or Clang 14+ with the corresponding standard library
- CMake 3.24+, Ninja (or Make), and Python 3.9+ available on PATH
- `VCPKG_ROOT` configured so CMake can locate the vcpkg toolchain file
- Siv3D SDK installed under `/usr/local` (environment variables such as `SIV3D_0_6_16` are not required)
- hiredis is supplied by vcpkg (manifest); do not use system apt-installed hiredis

## Testing instructions

### Project structure

- Test Project: `Test.vcxproj` (Google Test integrated with Siv3D)
- Main Entry: `test/Main.cpp` (DO NOT EDIT)
- Test Files: Add tests to existing `.cpp` files under `test/`, or create new ones as needed
- Dependencies: Google Test (via vcpkg)

### Run tests (CLI)

```bash
# Debug configuration (default)
python3 scripts/test.py

# Run with specific configuration
python3 scripts/test.py Debug
python3 scripts/test.py Release

# Pass GoogleTest arguments
python3 scripts/test.py Debug --gtest_list_tests
python3 scripts/test.py Debug --gtest_filter=TestSuite.*
```

### Build requirements for testing

- Build the `Test` project before running tests
- `Siv3D_MessageBus` builds automatically when building `Test`

#### Build commands (for tests)

```bash
# Build the Test project
python3 scripts/build.py Test Debug    # Debug configuration
python3 scripts/build.py Test Release  # Release configuration
```

#### Build outputs (tests)

- Debug: `build/Test/debug/bin/Test.exe`
- Release: `build/Test/release/bin/Test.exe`

### Test development workflow

1. Check existing files under `test/`
2. Prefer adding test cases to existing `.cpp` files when possible
3. Create a new `.cpp` only if no suitable file exists
4. Ensure the file is included in `Test.vcxproj`
5. Build and run: `scripts/msbuild.bat Test Debug` → `scripts/runtest.bat Debug`

#### Test case template

```cpp
#include <gtest/gtest.h>
#include <Siv3D.hpp>

TEST(TestSuiteName, TestCaseName) {
    EXPECT_EQ(expected, actual);
    EXPECT_TRUE(condition);
    EXPECT_FALSE(condition);
}
```

#### Project file management

- Verify existing files are listed under `<ClCompile>` in `Test.vcxproj`
- Example for adding a new file:

```xml
<ClCompile Include="test\NewTests.cpp" />
```

### Troubleshooting

1. `Test.exe` not found: build with `python3 scripts/build.py Test Debug`
2. No console output: ensure you are running from PowerShell
3. Build failures: confirm dependencies are installed via vcpkg
4. New test not compiling: ensure the new `.cpp` is added to `Test.vcxproj`
5. Link errors: check includes (`<gtest/gtest.h>`, `<Siv3D.hpp>`)

## C++ Development Rules

You are a senior C++ developer with expertise in modern C++ (C++20), STL, and system-level programming.

### Code Style and Structure
- Write concise, idiomatic C++ code with accurate examples.
- Follow modern C++ conventions and best practices.
- Use object-oriented, procedural, or functional programming patterns as appropriate.
- Leverage STL and standard algorithms for collection operations.
- Use descriptive variable and method names (e.g., 'isUserSignedIn', 'calculateTotal').
- Structure files into headers (*.hpp) and implementation files (*.cpp) with logical separation of concerns.

### Naming Conventions
- Use PascalCase for class names.
- Use camelCase for variable names and methods.
- Methods/functions use lowerCamelCase (e.g., `isConnected`, `subscribe`).
- Public API names should be concise and verb-first when applicable.
- Use SCREAMING_SNAKE_CASE for constants and macros.
- Prefix member variables with m_ (e.g., `m_userId`).
- Use namespaces to organize code logically.

### C++ Features Usage
- Prefer modern C++ features (e.g., auto, range-based loops, smart pointers).
- Use `std::unique_ptr` and `std::shared_ptr` for memory management.
- Prefer `std::optional` and `std::variant` for type-safe alternatives.
- Use `constexpr` and `const` to optimize compile-time computations.
- Use `std::string_view` for read-only string operations to avoid unnecessary copies.

### Syntax and Formatting
- Follow a consistent coding style, such as Google C++ Style Guide or your team’s standards.
- Place braces on the next line (Allman style) for control structures, methods, and classes.
- Use clear and consistent commenting practices.

### Error Handling and Validation
- Use exceptions for error handling (e.g., `std::runtime_error`, `std::invalid_argument`).
- Use RAII for resource management to avoid memory leaks.
- Validate inputs at function boundaries.
- Log using Siv3D `Logger`; prefer `U""` string literals for formatting.

### Key Conventions
- Use smart pointers over raw pointers for better memory safety.
- Avoid global variables; use singletons sparingly.
- Use `enum class` for strongly typed enumerations.
- Separate interface from implementation in classes.
- Use templates and metaprogramming judiciously for generic solutions.

### Security
- Use secure coding practices to avoid vulnerabilities (e.g., buffer overflows, dangling pointers).
- Prefer `std::array` or `std::vector` over raw arrays.
- Avoid C-style casts; use `static_cast`, `dynamic_cast`, or `reinterpret_cast` when necessary.
- Enforce const-correctness in functions and member variables.

### Documentation
- Write clear comments for classes, methods, and critical logic.
- Use Doxygen for generating API documentation.
- In public headers, prefer triple-slash `///` Doxygen comments; Japanese descriptions are acceptable.
- Document assumptions, constraints, and expected behavior of code.

Follow the official ISO C++ standards and guidelines for best practices in modern C++ development.
