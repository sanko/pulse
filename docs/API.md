# API Quick Reference

This document provides a concise reference for the public API of the `pulse` compiler library. It's designed to be a quick lookup for developers who are already familiar with compiler design concepts.

For practical examples, please see the [Cookbook](cookbook.md).

## Table of Contents

*   [1. Compiler API](#1-compiler-api)
*   [2. Lexer API](#2-lexer-api)
*   [3. Parser API](#3-parser-api)
*   [4. AST API](#4-ast-api)
*   [5. Semantic Analysis API](#5-semantic-analysis-api)
*   [6. Code Generation API](#6-code-generation-api)
*   [7. Virtual Machine API](#7-virtual-machine-api)

---

## 1. Compiler API

The main entry point for using pulse as a library.

### `infix_compiler_create`

Creates a new compiler instance with all necessary components (lexer, parser, semantic analyzer, code generator, VM).

```c
infix_compiler_t* infix_compiler_create(void);
```

### `infix_compiler_destroy`

Destroys a compiler instance and frees all associated memory.

```c
void infix_compiler_destroy(infix_compiler_t* compiler);
```

### `infix_compile`

Compiles a source string into bytecode.

```c
bool infix_compile(infix_compiler_t* compiler, const char* source);
```

**Returns:** `true` on success, `false` on error.

### `infix_run`

Executes the compiled bytecode.

```c
vm_object_t* infix_run(infix_compiler_t* compiler);
```

**Returns:** The result of the program execution as a VM object.

### `infix_load_file`

Compiles and runs a source file.

```c
bool infix_load_file(infix_compiler_t* compiler, const char* filename);
```

### `infix_set_verbose`

Enables verbose output during compilation.

```c
void infix_set_verbose(infix_compiler_t* compiler, bool verbose);
```

### `infix_set_dump_ast`

Enables AST dumping for debugging.

```c
void infix_set_dump_ast(infix_compiler_t* compiler, bool dump);
```

### `infix_set_dump_bytecode`

Enables bytecode dumping for debugging.

```c
void infix_set_dump_bytecode(infix_compiler_t* compiler, bool dump);
```

---

## 2. Lexer API

The lexer (tokenizer) converts source code into a stream of tokens.

### `lexer_create`

Creates a new lexer for the given source.

```c
lexer_t* lexer_create(const char* source);
```

### `lexer_destroy`

Destroys a lexer instance.

```c
void lexer_destroy(lexer_t* lexer);
```

### `lexer_next_token`

Returns the next token from the source.

```c
token_t lexer_next_token(lexer_t* lexer);
```

### `lexer_peek`

Returns the next token without consuming it.

```c
token_t lexer_peek(lexer_t* lexer);
```

### `lexer_match`

Consumes a token if it matches the given type.

```c
bool lexer_match(lexer_t* lexer, token_type_t type);
```

### `lexer_expect`

Consumes a token and verifies it has the expected type.

```c
bool lexer_expect(lexer_t* lexer, token_type_t type, const char* error_msg);
```

### Token Types

```c
typedef enum {
    /* Literals */
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,

    /* Keywords */
    TOKEN_NAMESPACE,
    TOKEN_CLASS,
    TOKEN_FN,
    TOKEN_VAR,
    TOKEN_CONST,
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_DO,
    TOKEN_RETURN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_THROW,
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_FINALLY,
    TOKEN_YIELD,
    TOKEN_AWAIT,
    TOKEN_IMPORT,
    TOKEN_EXPORT,
    TOKEN_PUBLIC,
    TOKEN_PRIVATE,
    TOKEN_STATIC,
    TOKEN_FIBER,
    TOKEN_NOT,
    TOKEN_AND,
    TOKEN_OR,

    /* Operators */
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    /* ... more operators */

    /* Delimiters */
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,

    /* Special */
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_COMMENT,
    TOKEN_NEWLINE
} token_type_t;
```

### Token Structure

```c
typedef struct {
    token_type_t type;
    char* lexeme;
    int line;
    int column;
    union {
        int64_t int_val;
        double float_val;
        char* string_val;
    };
} token_t;
```

---

## 3. Parser API

The parser converts tokens into an Abstract Syntax Tree (AST).

### `parser_create`

Creates a new parser for the given lexer.

```c
parser_t* parser_create(lexer_t* lexer);
```

### `parser_destroy`

Destroys a parser instance.

```c
void parser_destroy(parser_t* parser);
```

### `parser_parse`

Parses the tokens and returns the AST root node.

```c
ast_node_t* parser_parse(parser_t* parser);
```

### `parser_had_error`

Checks if the parser encountered an error.

```c
bool parser_had_error(parser_t* parser);
```

### `parser_get_error`

Returns the parser's error message.

```c
const char* parser_get_error(parser_t* parser);
```

---

## 4. AST API

The AST represents the syntactic structure of the program.

### `ast_create_node`

Creates a new AST node.

```c
ast_node_t* ast_create_node(ast_node_type_t type);
```

### `ast_free_node`

Frees a single AST node.

```c
void ast_free_node(ast_node_t* node);
```

### `ast_free_tree`

Frees an entire AST subtree.

```c
void ast_free_tree(ast_node_t* root);
```

### AST Node Types

```c
typedef enum {
    AST_PROGRAM,
    AST_NAMESPACE_DECL,
    AST_CLASS_DECL,
    AST_FUNCTION_DECL,
    AST_PARAM_LIST,
    AST_VAR_DECL,
    AST_CONST_DECL,
    AST_BLOCK,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_DO_WHILE_STMT,
    AST_RETURN_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_THROW_STMT,
    AST_TRY_STMT,
    AST_EXPR_STMT,
    AST_YIELD_STMT,

    /* Expressions */
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_INDEX_EXPR,
    AST_MEMBER_EXPR,
    AST_ASSIGNMENT,
    AST_TERNARY_EXPR,
    AST_IDENTIFIER,
    AST_INTEGER_LITERAL,
    AST_FLOAT_LITERAL,
    AST_STRING_LITERAL,
    AST_BOOL_LITERAL,
    AST_NULL_LITERAL,
    AST_ARRAY_LITERAL,
    AST_OBJECT_LITERAL,
    AST_LAMBDA_EXPR,
    AST_NEW_EXPR,
    AST_THIS_EXPR,
    AST_SUPER_EXPR,
    AST_FIBER_EXPR,
    AST_AWAIT_EXPR,
    AST_METHOD_CALL
} ast_node_type_t;
```

---

## 5. Semantic Analysis API

The semantic analyzer performs type checking and symbol resolution.

### `sema_create`

Creates a new semantic analyzer instance.

```c
sema_t* sema_create(void);
```

### `sema_destroy`

Destroys a semantic analyzer instance.

```c
void sema_destroy(sema_t* sema);
```

### `sema_push_scope`

Enters a new scope.

```c
scope_t* sema_push_scope(sema_t* sema, const char* name);
```

### `sema_pop_scope`

Exits the current scope.

```c
void sema_pop_scope(sema_t* sema);
```

### `sema_define`

Defines a new symbol in the current scope.

```c
symbol_t* sema_define(sema_t* sema, const char* name, symbol_kind_t kind, type_t* type);
```

### `sema_lookup`

Looks up a symbol by name.

```c
symbol_t* sema_lookup(sema_t* sema, const char* name);
```

### `sema_analyze`

Performs semantic analysis on the AST.

```c
bool sema_analyze(sema_t* sema, ast_node_t* root);
```

### `sema_get_type`

Gets the type of an AST node.

```c
type_t* sema_get_type(sema_t* sema, ast_node_t* node);
```

### `sema_get_error`

Returns the semantic analyzer's error message.

```c
const char* sema_get_error(sema_t* sema);
```

---

## 6. Code Generation API

The code generator produces bytecode from the AST.

### `codegen_create`

Creates a new code generator instance.

```c
codegen_t* codegen_create(sema_t* sema);
```

### `codegen_destroy`

Destroys a code generator instance.

```c
void codegen_destroy(codegen_t* cg);
```

### `codegen_compile`

Compiles the AST into bytecode.

```c
function_t* codegen_compile(codegen_t* cg, ast_node_t* root);
```

### `codegen_had_error`

Checks if code generation encountered an error.

```c
bool codegen_had_error(codegen_t* cg);
```

### `codegen_get_error`

Returns the code generator's error message.

```c
const char* codegen_get_error(codegen_t* cg);
```

### Bytecode Opcodes

```c
typedef enum {
    OP_HALT,
    OP_LOAD_NULL,
    OP_LOAD_TRUE,
    OP_LOAD_FALSE,
    OP_LOAD_INT,
    OP_LOAD_FLOAT,
    OP_LOAD_STRING,
    OP_LOAD_LOCAL,
    OP_STORE_LOCAL,
    OP_LOAD_GLOBAL,
    OP_STORE_GLOBAL,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEG,
    OP_NOT,
    /* ... more opcodes */
    OP_CALL,
    OP_RETURN,
    OP_NEW_OBJECT,
    OP_NEW_ARRAY,
    /* ... more opcodes */
} opcode_t;
```

---

## 7. Virtual Machine API

The VM executes bytecode instructions.

### `vm_create`

Creates a new VM instance.

```c
vm_t* vm_create(size_t heap_size);
```

### `vm_destroy`

Destroys a VM instance.

```c
void vm_destroy(vm_t* vm);
```

### `vm_execute`

Executes a compiled function.

```c
vm_object_t* vm_execute(vm_t* vm, function_t* entry);
```

### `vm_gc`

Triggers garbage collection.

```c
vm_object_t* vm_gc(vm_t* vm);
```

### VM Constants

```c
#define VM_STACK_SIZE 65536
#define VM_CALL_DEPTH 1024
#define VM_STRING_LEN_MAX 65536
```

### VM Object Types

Objects on the heap can be:
- **Strings**: Null-terminated character arrays
- **Arrays**: Dynamic arrays of VM objects
- **Objects**: Hash maps with string keys
- **Instances**: Class instances with fields
- **Fibers**: Coroutine state

---

## Example: Embedding pulse

```c
#include <pulse/pulse.h>
#include <stdio.h>

int main(int argc, char** argv) {
    infix_compiler_t* compiler = infix_compiler_create();
    infix_set_verbose(compiler, false);
    infix_set_dump_ast(compiler, false);
    infix_set_dump_bytecode(compiler, false);

    const char* source =
        "fn fib(n) {\n"
        "    if (n <= 1) return n;\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "\n"
        "print(fib(10));\n";

    if (!infix_compile(compiler, source)) {
        printf("Compilation failed: %s\n", parser_get_error(compiler->parser));
        infix_compiler_destroy(compiler);
        return 1;
    }

    vm_object_t* result = infix_run(compiler);
    printf("Result type: %d\n", result->type.kind);

    infix_compiler_destroy(compiler);
    return 0;
}
```
