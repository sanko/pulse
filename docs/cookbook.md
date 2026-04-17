# The pulse Cookbook

This guide provides practical, real-world examples to help you solve common tasks with the `pulse` language and compiler. Where the `README.md` covers concepts, this cookbook provides the code.

## Table of Contents

* [Chapter 1: Getting Started](#chapter-1-getting-started)
   + [Recipe: Compiling and Running Your First Program](#recipe-compiling-and-running-your-first-program)
   + [Recipe: Embedding pulse in Your C Application](#recipe-embedding-pulse-in-your-c-application)
* [Chapter 2: Variables and Types](#chapter-2-variables-and-types)
   + [Recipe: Working with Numbers](#recipe-working-with-numbers)
   + [Recipe: Working with Strings](#recipe-working-with-strings)
   + [Recipe: Working with Arrays](#recipe-working-with-arrays)
   + [Recipe: Working with Objects](#recipe-working-with-objects)
* [Chapter 3: Functions](#chapter-3-functions)
   + [Recipe: Defining and Calling Functions](#recipe-defining-and-calling-functions)
   + [Recipe: Closures](#recipe-closures)
   + [Recipe: Higher-Order Functions](#recipe-higher-order-functions)
* [Chapter 4: Classes and OOP](#chapter-4-classes-and-oop)
   + [Recipe: Defining a Class](#recipe-defining-a-class)
   + [Recipe: Inheritance](#recipe-inheritance)
   + [Recipe: Static Methods](#recipe-static-methods)
* [Chapter 5: Control Flow](#chapter-5-control-flow)
   + [Recipe: If/Else Statements](#recipe-ifelse-statements)
   + [Recipe: While Loops](#recipe-while-loops)
   + [Recipe: For Loops](#recipe-for-loops)
* [Chapter 6: Fibers (Coroutines)](#chapter-6-fibers-coroutines)
   + [Recipe: Creating a Fiber](#recipe-creating-a-fiber)
   + [Recipe: Communicating with Fibers](#recipe-communicating-with-fibers)
* [Chapter 7: Error Handling](#chapter-7-error-handling)
   + [Recipe: Throwing and Catching Exceptions](#recipe-throwing-and-catching-exceptions)
   + [Recipe: The Finally Block](#recipe-the-finally-block)
* [Chapter 8: Namespaces](#chapter-8-namespaces)
   + [Recipe: Organizing Code with Namespaces](#recipe-organizing-code-with-namespaces)
* [Chapter 9: Code Generation (Emit)](#chapter-9-code-generation-emit)
   + [Recipe: Generating Machine Code](#recipe-generating-machine-code)
   + [Recipe: Writing an Executable to Disk](#recipe-writing-an-executable-to-disk)
   + [Recipe: JIT Compilation](#recipe-jit-compilation)

---

## Chapter 1: Getting Started

### Recipe: Compiling and Running Your First Program

**Problem**: You want to compile and run a pulse program.

**Solution**: Use the compiler API to compile and execute the program.

```c
#include <pulse/pulse.h>
#include <stdio.h>

int main() {
    pulse_compiler_t* compiler = pulse_compiler_create();

    const char* source = "print(\"Hello, World!\");";

    if (pulse_compile(compiler, source)) {
        pulse_vm_t* vm = pulse_vm_create(compiler);
        pulse_vm_run(vm);
        // Program executed successfully
    }

    pulse_compiler_destroy(compiler);
    return 0;
}
```

### Recipe: Embedding pulse in Your C Application

**Problem**: You want to embed the pulse interpreter in your C application.

**Solution**: Create a compiler instance and use it to run user-provided scripts.

```c
#include <pulse/pulse.h>
#include <stdio.h>
#include <stdlib.h>

int run_script(const char* script) {
    pulse_compiler_t* compiler = pulse_compiler_create();

    if (!pulse_compile(compiler, script)) {
        fprintf(stderr, "Compilation error: %s\n", pulse_get_error(compiler));
        pulse_compiler_destroy(compiler);
        return 1;
    }

    pulse_vm_t* vm = pulse_vm_create(compiler);
    pulse_vm_run(vm);

    pulse_compiler_destroy(compiler);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script.pulse>\n", argv[0]);
        return 1;
    }
    
    // Read file and run
    FILE* f = fopen(argv[1], "r");
    if (!f) {
        perror("fopen");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(len + 1);
    fread(source, 1, len, f);
    source[len] = '\0';
    fclose(f);
    
    int result = run_script(source);
    free(source);
    return result;
}
```

---

## Chapter 2: Variables and Types

### Recipe: Working with Numbers

**Problem**: You need to perform arithmetic operations.

```pulse
var x = 10;
var y = 3;

print(x + y);   // 13
print(x - y);   // 7
print(x * y);   // 30
print(x / y);   // 3 (integer division)
print(x % y);   // 1 (modulo)

var f = 3.14;
print(f * 2);   // 6.28
```

### Recipe: Working with Strings

**Problem**: You need to manipulate text.

```pulse
var greeting = "Hello";
var name = "World";

print(greeting + ", " + name + "!");  // String concatenation

var s = "pulse";
print(s.len());     // 5
print(s.upper());   // PULSE
print(s.lower());   // pulse

print(s[0]);        // p
print(s[1..3]);     // ul
```

### Recipe: Working with Arrays

**Problem**: You need to store collections of items.

```pulse
var numbers = [1, 2, 3, 4, 5];

print(numbers[0]);      // 1
print(numbers.len());   // 5

numbers.push(6);
print(numbers);         // [1, 2, 3, 4, 5, 6]

var sum = 0;
for (n in numbers) {
    sum = sum + n;
}
print(sum);            // 21
```

### Recipe: Working with Objects

**Problem**: You need key-value storage.

```pulse
var person = {
    "name": "Alice",
    "age": 30,
    "city": "New York"
};

print(person["name"]);     // Alice
person["age"] = 31;        // Update value
person["email"] = "alice@example.com";  // Add new key

for (key, value in person) {
    print(key + ": " + value);
}
```

---

## Chapter 3: Functions

### Recipe: Defining and Calling Functions

**Problem**: You want to define reusable code blocks.

```pulse
fn greet(name) {
    return "Hello, " + name + "!";
}

print(greet("World"));     // Hello, World!
print(greet("Pulse"));     // Hello, Pulse!

fn factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

print(factorial(5));       // 120
```

### Recipe: Closures

**Problem**: You need functions that capture their environment.

```pulse
fn make_counter(start) {
    var count = start;
    fn counter() {
        count = count + 1;
        return count;
    }
    return counter;
}

var counter = make_counter(0);
print(counter());  // 1
print(counter());  // 2
print(counter());  // 3

var counter2 = make_counter(100);
print(counter2()); // 101
print(counter()); // 4 (first counter unaffected)
```

### Recipe: Higher-Order Functions

**Problem**: You need functions that take or return other functions.

```pulse
fn apply_twice(fn, value) {
    return fn(fn(value));
}

fn double(x) {
    return x * 2;
}

print(apply_twice(double, 3));  // 12

fn compose(f, g) {
    fn result(x) {
        return f(g(x));
    }
    return result;
}

var add_one = fn(x) { return x + 1; };
var double = fn(x) { return x * 2; };
var add_one_then_double = compose(double, add_one);

print(add_one_then_double(5));  // 12
```

---

## Chapter 4: Classes and OOP

### Recipe: Defining a Class

**Problem**: You need to model objects with state and behavior.

```pulse
class BankAccount {
    init(owner, balance) {
        this.owner = owner;
        this.balance = balance;
    }
    
    deposit(amount) {
        this.balance = this.balance + amount;
    }
    
    withdraw(amount) {
        if (amount > this.balance) {
            throw "Insufficient funds";
        }
        this.balance = this.balance - amount;
    }
    
    to_string() {
        return this.owner + ": $" + this.balance;
    }
}

var account = BankAccount("Alice", 1000);
account.deposit(500);
account.withdraw(200);
print(account.to_string());  // Alice: $1300
```

### Recipe: Inheritance

**Problem**: You need to extend existing classes.

```pulse
class Animal {
    init(name) {
        this.name = name;
    }
    
    speak() {
        return "...";
    }
}

class Dog < Animal {
    init(name, breed) {
        super.init(name);
        this.breed = breed;
    }
    
    speak() {
        return "Woof!";
    }
    
    fetch(item) {
        return this.name + " fetches the " + item;
    }
}

var dog = Dog("Rex", "Labrador");
print(dog.speak());              // Woof!
print(dog.fetch("ball"));        // Rex fetches the ball
```

### Recipe: Static Methods

**Problem**: You need methods that belong to the class, not instances.

```pulse
class Math {
    static abs(x) {
        if (x < 0) {
            return -x;
        }
        return x;
    }
    
    static max(a, b) {
        if (a > b) {
            return a;
        }
        return b;
    }
    
    static min(a, b) {
        if (a < b) {
            return a;
        }
        return b;
    }
}

print(Math.abs(-5));   // 5
print(Math.max(3, 7)); // 7
print(Math.min(3, 7)); // 3
```

---

## Chapter 5: Control Flow

### Recipe: If/Else Statements

**Problem**: You need conditional execution.

```pulse
fn classify_number(n) {
    if (n > 0) {
        return "positive";
    } elif (n < 0) {
        return "negative";
    } else {
        return "zero";
    }
}

print(classify_number(5));   // positive
print(classify_number(-3));  // negative
print(classify_number(0));   // zero
```

### Recipe: While Loops

**Problem**: You need to repeat code while a condition is true.

```pulse
var i = 0;
while (i < 5) {
    print(i);
    i = i + 1;
}
// Prints: 0, 1, 2, 3, 4

var sum = 0;
var n = 1;
while (n <= 100) {
    sum = sum + n;
    n = n + 1;
}
print(sum);  // 5050
```

### Recipe: For Loops

**Problem**: You need to iterate over sequences.

```pulse
// Iterate over array
var numbers = [1, 2, 3, 4, 5];
for (n in numbers) {
    print(n);
}

// Iterate with range
for (i in 0..5) {
    print(i);
}
// Prints: 0, 1, 2, 3, 4

// Iterate over object
var obj = {"a": 1, "b": 2};
for (key, value in obj) {
    print(key + " = " + value);
}
```

---

## Chapter 6: Fibers (Coroutines)

### Recipe: Creating a Fiber

**Problem**: You need cooperative multitasking.

```pulse
fn counter(max) {
    var i = 0;
    while (i < max) {
        yield i;
        i = i + 1;
    }
    return "done";
}

var fiber = Fiber(counter);
while (fiber.status == "suspended") {
    print("Got: " + fiber.call());
}
print("Final: " + fiber.result);
// Prints:
// Got: 0
// Got: 1
// Got: 2
// ...
// Got: 9
// Final: done
```

### Recipe: Communicating with Fibers

**Problem**: You need two-way communication with fibers.

```pulse
fn echo_fiber() {
    while (true) {
        var msg = yield;
        if (msg == "quit") {
            return "Goodbye!";
        }
        yield "Echo: " + msg;
    }
}

var fiber = Fiber(echo_fiber);
fiber.call("Hello");   // Prime the fiber
print(fiber.call("World"));   // Echo: World
print(fiber.call("!"));       // Echo: !
print(fiber.call("quit"));    // Goodbye!
```

---

## Chapter 7: Error Handling

### Recipe: Throwing and Catching Exceptions

**Problem**: You need to handle error conditions.

```pulse
fn divide(a, b) {
    if (b == 0) {
        throw "Division by zero!";
    }
    return a / b;
}

try {
    var result = divide(10, 0);
    print(result);
} catch err {
    print("Error caught: " + err);
}
// Output: Error caught: Division by zero!

try {
    var result = divide(10, 2);
    print(result);  // 5
} catch err {
    print("Error: " + err);
}
```

### Recipe: The Finally Block

**Problem**: You need to ensure cleanup code runs.

```pulse
var file = open_file("data.txt");

try {
    var content = file.read();
    print(content);
} catch err {
    print("Error reading file: " + err);
} finally {
    file.close();
    print("Cleanup complete");
}
// The file is always closed, whether an exception occurred or not
```

---

## Chapter 8: Namespaces

### Recipe: Organizing Code with Namespaces

**Problem**: You need to organize code into logical units.

```pulse
namespace Math {
    fn add(a, b) { return a + b; }
    fn sub(a, b) { return a - b; }
    fn mul(a, b) { return a * b; }
    fn div(a, b) { return a / b; }
}

namespace String {
    fn reverse(s) {
        var result = "";
        for (i in 0..s.len()) {
            result = s[i] + result;
        }
        return result;
    }
    
    fn is_palindrome(s) {
        return s == reverse(s);
    }
}

print(Math.add(3, 4));                    // 7
print(String.reverse("hello"));           // olleh
print(String.is_palindrome("radar"));     // true
```

---

## Chapter 9: Code Generation (Emit)

The `emit` library provides low-level JIT code generation for x86-64 and ARM64 architectures. You can generate machine code at runtime, write it to disk as an executable, or execute it directly via JIT.

### Recipe: Generating Machine Code

**Problem**: You need to generate x86-64 machine code programmatically.

**Solution**: Use the emit API to create sections, emit instructions, and retrieve the binary.

```c
#include <pulse/emit/emit.h>
#include <pulse/emit/emit_math.h>
#include <stdio.h>

int main() {
    emit_context_t* ctx = NULL;

    // Create context for x86-64, binary format
    pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY);
    if (status != PULSE_SUCCESS) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    // Add a code section
    emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    emit_begin_section(ctx, ".text");

    // Define and emit a function that returns 42
    emit_define_symbol(ctx, "get_answer", EMIT_VISIBILITY_DEFAULT, true);
    emit_emit_label(ctx, "get_answer");

    emit_math_mov_imm(ctx, EMIT_REG_RAX, 42);  // mov rax, 42
    emit_math_ret(ctx);                          // ret

    // Get the generated binary
    const uint8_t* binary = NULL;
    size_t size = 0;
    emit_get_binary(ctx, &binary, &size);

    printf("Generated %zu bytes of machine code\n", size);

    // The binary contains: B8 2A 00 00 00 C3 (mov rax, 42; ret)

    emit_destroy(ctx);
    return 0;
}
```

### Recipe: Writing an Executable to Disk

**Problem**: You want to generate an actual PE/ELF executable file that can be run.

**Solution**: Use `EMIT_FORMAT_PE` on Windows or `EMIT_FORMAT_ELF` on Linux, then write to disk.

```c
#include <pulse/emit/emit.h>
#include <pulse/emit/emit_math.h>
#include <stdio.h>

int main() {
    emit_context_t* ctx = NULL;

    // Create context for PE format (Windows executable)
    pulse_status status = emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_PE);
    if (status != PULSE_SUCCESS) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    // Create code section
    emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    emit_begin_section(ctx, ".text");

    // Function: int add(int a, int b) { return a + b; }
    emit_define_symbol(ctx, "add", EMIT_VISIBILITY_DEFAULT, true);
    emit_emit_label(ctx, "add");

    // Prologue: push rbp; mov rbp, rsp
    emit_math_prologue(ctx);

    // Result in RAX = RCX + RDX (first two args in Windows x64 ABI)
    emit_math_add(ctx, EMIT_REG_RAX, EMIT_REG_RDX);

    // Epilogue: leave; ret
    emit_math_epilogue(ctx);

    // Write directly to file
    status = emit_write_file(ctx, "add.exe");
    if (status == PULSE_SUCCESS) {
        printf("Written executable: add.exe\n");
    }

    emit_destroy(ctx);
    return 0;
}
```

On Windows, this generates a valid PE32+ executable. On Linux, use `EMIT_FORMAT_ELF` to generate an ELF binary.

### Recipe: JIT Compilation

**Problem**: You want to compile and immediately execute code at runtime.

**Solution**: Generate machine code, copy it to executable memory, and call it as a function pointer.

```c
#include <pulse/emit/emit.h>
#include <pulse/emit/emit_math.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

typedef uint64_t (*fn_ptr)(void);

void* alloc_executable(size_t size) {
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
    return mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}

void free_executable(void* mem, size_t size) {
#ifdef _WIN32
    VirtualFree(mem, 0, MEM_RELEASE);
#else
    munmap(mem, size);
#endif
}

int main() {
    emit_context_t* ctx = NULL;
    emit_create(&ctx, EMIT_ARCH_X86_64, EMIT_FORMAT_BINARY);

    emit_add_section(ctx, ".text", EMIT_SECTION_FLAG_ALLOC | EMIT_SECTION_FLAG_EXECUTE);
    emit_begin_section(ctx, ".text");

    // Generate: int factorial(int n) { return n <= 1 ? 1 : n * factorial(n-1); }
    emit_define_symbol(ctx, "factorial", EMIT_VISIBILITY_DEFAULT, true);
    emit_emit_label(ctx, "factorial");

    // RAX = n (argument)
    // Compare n <= 1
    emit_math_cmp_imm(ctx, EMIT_REG_RAX, 1);
    emit_math_jmp_cc(ctx, EMIT_CC_LE, "base_case");  // jle base_case

    // Recursive case: n * factorial(n - 1)
    // Save n
    emit_math_push(ctx, EMIT_REG_RAX);

    // n - 1
    emit_math_sub_imm(ctx, EMIT_REG_RAX, 1);

    // Recursive call (simplified - just returns 1 for demo)
    emit_math_mov_imm(ctx, EMIT_REG_RAX, 1);

    emit_math_pop(ctx, EMIT_REG_RCX);  // restore n
    emit_math_mul(ctx, EMIT_REG_RCX);  // n * result
    emit_math_ret(ctx);

    // Base case: return 1
    emit_emit_label(ctx, "base_case");
    emit_math_mov_imm(ctx, EMIT_REG_RAX, 1);
    emit_math_ret(ctx);

    // Get binary
    const uint8_t* binary = NULL;
    size_t size = 0;
    emit_get_binary(ctx, &binary, &size);

    // Copy to executable memory
    void* exec = alloc_executable(size);
    memcpy(exec, binary, size);

    // Execute!
    fn_ptr factorial = (fn_ptr)exec;
    printf("factorial(5) = %llu\n", (unsigned long long)factorial(5));

    // Cleanup
    free_executable(exec, size);
    emit_destroy(ctx);
    return 0;
}
```

### Architecture Support

The emit library supports multiple architectures:

| Architecture | Constant | Notes |
|-------------|----------|-------|
| x86-64 | `EMIT_ARCH_X86_64` | Windows x64, Linux x86-64 |
| ARM64 | `EMIT_ARCH_AARCH64` | ARMv8-A (Raspberry Pi 4, Apple Silicon) |

### Output Formats

| Format | Constant | Output |
|--------|----------|--------|
| Binary | `EMIT_FORMAT_BINARY` | Raw machine code only |
| PE | `EMIT_FORMAT_PE` | Windows PE32/PE32+ executable |
| ELF | `EMIT_FORMAT_ELF` | Linux ELF64 relocatable |
