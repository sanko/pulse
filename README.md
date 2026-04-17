# pulse: A Modern Scripting Language Compiler

`pulse` is a modern, lightweight compiled scripting language that targets x86-64 via a custom emit library. It's designed as a clean, educational implementation featuring OOP, fibers, exceptions, closures, and namespaces.

It's designed to be the simplest way to add a dynamic scripting language to your project, whether you're building a game engine, a plugin system, or just want a lightweight embedded language.

[![CI](https://github.com/sanko/pulse/actions/workflows/ci.yml/badge.svg)](#platform-support)

## Key Features

*   **Modern Language Design:** Features you'd expect from a modern scripting language: OOP with classes and inheritance, fibers (coroutines) for cooperative multitasking, exceptions with try/catch/finally, closures, and namespaces.
*   **Clean Compilation Pipeline:** Lexer -> Parser -> AST -> Bytecode -> VM, with an emit library for native code generation.
*   **Simple Integration:** Add a single C file and a header directory to your project to get started. No complex dependencies.
*   **Type System:** Strong static typing with inference, arrays, objects, functions, fibers, and class types.
*   **High Performance:** Emit library generates native x86-64 machine code for maximum performance.
*   **Security-First Design:** Hardened against vulnerabilities with fuzz testing.

## Full Documentation

*   [Installation Guide](docs/INSTALL.md): How to build and integrate `pulse`.
*   [API Quick Reference](docs/API.md): A complete reference for the public API.
*   [The Cookbook](docs/cookbook.md): Practical, real-world examples and recipes.
*   [Internals Documentation](docs/internals.md): Deep dive into the compiler architecture.
*   [Porting Guide](docs/porting.md): Instructions for adding support for a new CPU architecture.
*   [Contributing Guide](CONTRIBUTING.md): How to contribute to the project.

---

## How It Works: A Quick Example

```c
#include <pulse/pulse.h>

int main() {
    pulse_compiler_t* compiler = pulse_compiler_create();

    const char* source = "fn main() { print(\"Hello, World!\"); }";

    if (pulse_compile(compiler, source)) {
        pulse_vm_t* vm = pulse_vm_create(compiler);
        pulse_vm_run(vm);
        // Handle result
    } else {
        printf("Compilation error: %s\n", pulse_get_error(compiler));
    }

    pulse_compiler_destroy(compiler);
    return 0;
}
```

## Language Features

### Variables and Functions

```javascript
var x = 10;
const y = 20;

fn add(a, b) {
    return a + b;
}

fn factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

### Classes and OOP

```javascript
class Point {
    init(x, y) {
        this.x = x;
        this.y = y;
    }
    
    distance_to(other) {
        var dx = this.x - other.x;
        var dy = this.y - other.y;
        return sqrt(dx * dx + dy * dy);
    }
}

var p1 = Point(1.0, 2.0);
var p2 = Point(4.0, 6.0);
print(p1.distance_to(p2));
```

### Fibers (Coroutines)

```javascript
fn counter() {
    var i = 0;
    while (i < 10) {
        yield i;
        i = i + 1;
    }
}

var fiber = Fiber(counter);
while (fiber.status == "suspended") {
    print(fiber.call());
}
```

### Exceptions

```javascript
fn risky() {
    throw "Something went wrong!";
}

try {
    risky();
} catch err {
    print("Caught: " + err);
} finally {
    print("Cleanup");
}
```

## Getting Started

The easiest way to use `pulse` is to add its source directly to your project.

1.  Copy the `src/` and `include/` directories into your project.
2.  Add `src/pulse.c` to your build system's list of source files.
3.  Add the `include/` directory to your include paths.
4.  `#include <pulse/pulse.h>` in your code.

### Building

```bash
# Using Perl build script (recommended)
perl build.pl build

# Using CMake
cmake -B build && cmake --build build

# Using GNU Make
make

# Using XMake
xmake
```

### Running Tests

```bash
# Using Perl build script
perl build.pl test

# Using CMake
ctest --test-dir build

# Using GNU Make
make test

# Using XMake
xmake test
```

## Project Philosophy

`pulse` is built on three core principles:

1.  **Educational Clarity:** The codebase should be readable and understandable, serving as a reference implementation for language compilation.
2.  **Modern Features:** Include features from modern languages (classes, fibers, exceptions) while keeping the implementation clean.
3.  **Performance:** The emit library generates optimized native code for fast execution.

## Platform Support

`pulse` is designed to work across multiple platforms.

| OS           | Version     | Architecture | Compiler  | Status |
| :----------- | :---------- | :----------- | :-------- | :----- |
| Windows      | 10+         | x86-64       | GCC/Clang | Planned |
| Linux        | Any         | x86-64       | GCC/Clang | Planned |
| macOS        | Any         | x86-64/AArch64 | Clang   | Planned |

## Architecture

The `pulse` compiler follows a classic pipeline:

1.  **Lexer** - Tokenizes source code into tokens
2.  **Parser** - Builds an Abstract Syntax Tree (AST) from tokens
3.  **Semantic Analyzer** - Performs type checking and symbol resolution
4.  **Code Generator** - Generates bytecode instructions
5.  **Virtual Machine** - Executes bytecode with a stack-based VM

The emit library provides native code generation for x86-64 platforms:

- **PE (Windows)** - Generates Windows PE executables
- **ELF (Unix)** - Generates ELF executables for Linux/macOS

## License

`pulse` is dual-licensed under the [Artistic License 2.0](LICENSE-A2) and the [MIT License](LICENSE-MIT). You may choose to use the code under the terms of either license.

All standalone documentation (`.md`), explanatory text, and code examples contained within this repository may be used, modified, and distributed under the terms of the [Creative Commons Attribution 4.0 International License (CC BY 4.0)](LICENSE-CC).
