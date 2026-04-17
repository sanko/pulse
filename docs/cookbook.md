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

---

## Chapter 1: Getting Started

### Recipe: Compiling and Running Your First Program

**Problem**: You want to compile and run a pulse program.

**Solution**: Use the compiler API to compile and execute the program.

```c
#include <pulse/pulse.h>
#include <stdio.h>

int main() {
    infix_compiler_t* compiler = infix_compiler_create();
    
    const char* source = "print(\"Hello, World!\");";
    
    if (infix_compile(compiler, source)) {
        vm_object_t* result = infix_run(compiler);
        // Program executed successfully
    }
    
    infix_compiler_destroy(compiler);
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
    infix_compiler_t* compiler = infix_compiler_create();
    
    if (!infix_compile(compiler, script)) {
        fprintf(stderr, "Compilation error: %s\n", compiler->errors);
        infix_compiler_destroy(compiler);
        return 1;
    }
    
    vm_object_t* result = infix_run(compiler);
    
    infix_compiler_destroy(compiler);
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
