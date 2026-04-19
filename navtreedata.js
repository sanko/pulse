/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "infix", "index.html", [
    [ "pulse: A Modern Scripting Language Compiler", "index.html", "index" ],
    [ "API Quick Reference", "md_docs_2API.html", [
      [ "Table of Contents", "md_docs_2API.html#autotoc_md18", null ],
      [ "1. Compiler API", "md_docs_2API.html#autotoc_md20", [
        [ "<tt>infix_compiler_create</tt>", "md_docs_2API.html#autotoc_md21", null ],
        [ "<tt>infix_compiler_destroy</tt>", "md_docs_2API.html#autotoc_md22", null ],
        [ "<tt>infix_compile</tt>", "md_docs_2API.html#autotoc_md23", null ],
        [ "<tt>infix_run</tt>", "md_docs_2API.html#autotoc_md24", null ],
        [ "<tt>infix_load_file</tt>", "md_docs_2API.html#autotoc_md25", null ],
        [ "<tt>infix_set_verbose</tt>", "md_docs_2API.html#autotoc_md26", null ],
        [ "<tt>infix_set_dump_ast</tt>", "md_docs_2API.html#autotoc_md27", null ],
        [ "<tt>infix_set_dump_bytecode</tt>", "md_docs_2API.html#autotoc_md28", null ]
      ] ],
      [ "2. Lexer API", "md_docs_2API.html#autotoc_md30", [
        [ "<tt>lexer_create</tt>", "md_docs_2API.html#autotoc_md31", null ],
        [ "<tt>lexer_destroy</tt>", "md_docs_2API.html#autotoc_md32", null ],
        [ "<tt>lexer_next_token</tt>", "md_docs_2API.html#autotoc_md33", null ],
        [ "<tt>lexer_peek</tt>", "md_docs_2API.html#autotoc_md34", null ],
        [ "<tt>lexer_match</tt>", "md_docs_2API.html#autotoc_md35", null ],
        [ "<tt>lexer_expect</tt>", "md_docs_2API.html#autotoc_md36", null ],
        [ "Token Types", "md_docs_2API.html#autotoc_md37", null ],
        [ "Token Structure", "md_docs_2API.html#autotoc_md38", null ]
      ] ],
      [ "3. Parser API", "md_docs_2API.html#autotoc_md40", [
        [ "<tt>parser_create</tt>", "md_docs_2API.html#autotoc_md41", null ],
        [ "<tt>parser_destroy</tt>", "md_docs_2API.html#autotoc_md42", null ],
        [ "<tt>parser_parse</tt>", "md_docs_2API.html#autotoc_md43", null ],
        [ "<tt>parser_had_error</tt>", "md_docs_2API.html#autotoc_md44", null ],
        [ "<tt>parser_get_error</tt>", "md_docs_2API.html#autotoc_md45", null ]
      ] ],
      [ "4. AST API", "md_docs_2API.html#autotoc_md47", [
        [ "<tt>ast_create_node</tt>", "md_docs_2API.html#autotoc_md48", null ],
        [ "<tt>ast_free_node</tt>", "md_docs_2API.html#autotoc_md49", null ],
        [ "<tt>ast_free_tree</tt>", "md_docs_2API.html#autotoc_md50", null ],
        [ "AST Node Types", "md_docs_2API.html#autotoc_md51", null ]
      ] ],
      [ "5. Semantic Analysis API", "md_docs_2API.html#autotoc_md53", [
        [ "<tt>sema_create</tt>", "md_docs_2API.html#autotoc_md54", null ],
        [ "<tt>sema_destroy</tt>", "md_docs_2API.html#autotoc_md55", null ],
        [ "<tt>sema_push_scope</tt>", "md_docs_2API.html#autotoc_md56", null ],
        [ "<tt>sema_pop_scope</tt>", "md_docs_2API.html#autotoc_md57", null ],
        [ "<tt>sema_define</tt>", "md_docs_2API.html#autotoc_md58", null ],
        [ "<tt>sema_lookup</tt>", "md_docs_2API.html#autotoc_md59", null ],
        [ "<tt>sema_analyze</tt>", "md_docs_2API.html#autotoc_md60", null ],
        [ "<tt>sema_get_type</tt>", "md_docs_2API.html#autotoc_md61", null ],
        [ "<tt>sema_get_error</tt>", "md_docs_2API.html#autotoc_md62", null ]
      ] ],
      [ "6. Code Generation API", "md_docs_2API.html#autotoc_md64", [
        [ "<tt>codegen_create</tt>", "md_docs_2API.html#autotoc_md65", null ],
        [ "<tt>codegen_destroy</tt>", "md_docs_2API.html#autotoc_md66", null ],
        [ "<tt>codegen_compile</tt>", "md_docs_2API.html#autotoc_md67", null ],
        [ "<tt>codegen_had_error</tt>", "md_docs_2API.html#autotoc_md68", null ],
        [ "<tt>codegen_get_error</tt>", "md_docs_2API.html#autotoc_md69", null ],
        [ "Bytecode Opcodes", "md_docs_2API.html#autotoc_md70", null ]
      ] ],
      [ "7. Virtual Machine API", "md_docs_2API.html#autotoc_md72", [
        [ "<tt>vm_create</tt>", "md_docs_2API.html#autotoc_md73", null ],
        [ "<tt>vm_destroy</tt>", "md_docs_2API.html#autotoc_md74", null ],
        [ "<tt>vm_execute</tt>", "md_docs_2API.html#autotoc_md75", null ],
        [ "<tt>vm_gc</tt>", "md_docs_2API.html#autotoc_md76", null ],
        [ "VM Constants", "md_docs_2API.html#autotoc_md77", null ],
        [ "VM Object Types", "md_docs_2API.html#autotoc_md78", null ]
      ] ],
      [ "Example: Embedding pulse", "md_docs_2API.html#autotoc_md80", null ]
    ] ],
    [ "The pulse Cookbook", "md_docs_2cookbook.html", [
      [ "Table of Contents", "md_docs_2cookbook.html#autotoc_md82", null ],
      [ "Chapter 1: Getting Started", "md_docs_2cookbook.html#autotoc_md84", [
        [ "Recipe: Compiling and Running Your First Program", "md_docs_2cookbook.html#autotoc_md85", null ],
        [ "Recipe: Embedding pulse in Your C Application", "md_docs_2cookbook.html#autotoc_md86", null ]
      ] ],
      [ "Chapter 2: Variables and Types", "md_docs_2cookbook.html#autotoc_md88", [
        [ "Recipe: Working with Numbers", "md_docs_2cookbook.html#autotoc_md89", null ],
        [ "Recipe: Working with Strings", "md_docs_2cookbook.html#autotoc_md90", null ],
        [ "Recipe: Working with Arrays", "md_docs_2cookbook.html#autotoc_md91", null ],
        [ "Recipe: Working with Objects", "md_docs_2cookbook.html#autotoc_md92", null ]
      ] ],
      [ "Chapter 3: Functions", "md_docs_2cookbook.html#autotoc_md94", [
        [ "Recipe: Defining and Calling Functions", "md_docs_2cookbook.html#autotoc_md95", null ],
        [ "Recipe: Closures", "md_docs_2cookbook.html#autotoc_md96", null ],
        [ "Recipe: Higher-Order Functions", "md_docs_2cookbook.html#autotoc_md97", null ]
      ] ],
      [ "Chapter 4: Classes and OOP", "md_docs_2cookbook.html#autotoc_md99", [
        [ "Recipe: Defining a Class", "md_docs_2cookbook.html#autotoc_md100", null ],
        [ "Recipe: Inheritance", "md_docs_2cookbook.html#autotoc_md101", null ],
        [ "Recipe: Static Methods", "md_docs_2cookbook.html#autotoc_md102", null ]
      ] ],
      [ "Chapter 5: Control Flow", "md_docs_2cookbook.html#autotoc_md104", [
        [ "Recipe: If/Else Statements", "md_docs_2cookbook.html#autotoc_md105", null ],
        [ "Recipe: While Loops", "md_docs_2cookbook.html#autotoc_md106", null ],
        [ "Recipe: For Loops", "md_docs_2cookbook.html#autotoc_md107", null ]
      ] ],
      [ "Chapter 6: Fibers (Coroutines)", "md_docs_2cookbook.html#autotoc_md109", [
        [ "Recipe: Creating a Fiber", "md_docs_2cookbook.html#autotoc_md110", null ],
        [ "Recipe: Communicating with Fibers", "md_docs_2cookbook.html#autotoc_md111", null ]
      ] ],
      [ "Chapter 7: Error Handling", "md_docs_2cookbook.html#autotoc_md113", [
        [ "Recipe: Throwing and Catching Exceptions", "md_docs_2cookbook.html#autotoc_md114", null ],
        [ "Recipe: The Finally Block", "md_docs_2cookbook.html#autotoc_md115", null ]
      ] ],
      [ "Chapter 8: Namespaces", "md_docs_2cookbook.html#autotoc_md117", [
        [ "Recipe: Organizing Code with Namespaces", "md_docs_2cookbook.html#autotoc_md118", null ]
      ] ],
      [ "Chapter 9: Code Generation (Emit)", "md_docs_2cookbook.html#autotoc_md120", [
        [ "Recipe: Generating Machine Code", "md_docs_2cookbook.html#autotoc_md121", null ],
        [ "Recipe: Writing an Executable to Disk", "md_docs_2cookbook.html#autotoc_md122", null ],
        [ "Recipe: JIT Compilation", "md_docs_2cookbook.html#autotoc_md123", null ],
        [ "Architecture Support", "md_docs_2cookbook.html#autotoc_md124", null ],
        [ "Output Formats", "md_docs_2cookbook.html#autotoc_md125", null ]
      ] ]
    ] ],
    [ "Building and Integrating pulse", "md_docs_2INSTALL.html", [
      [ "Prerequisites", "md_docs_2INSTALL.html#autotoc_md127", null ],
      [ "1. The Easiest Way: Add <tt>infix</tt> Directly to Your Project", "md_docs_2INSTALL.html#autotoc_md129", null ],
      [ "</blockquote>", "md_docs_2INSTALL.html#autotoc_md130", null ],
      [ "2. Building <tt>infix</tt> as a Standalone Library (Optional)", "md_docs_2INSTALL.html#autotoc_md131", [
        [ "Using perl (Recommended)", "md_docs_2INSTALL.html#autotoc_md132", null ],
        [ "Using xmake", "md_docs_2INSTALL.html#autotoc_md133", null ],
        [ "Using CMake", "md_docs_2INSTALL.html#autotoc_md134", null ],
        [ "Using Makefiles", "md_docs_2INSTALL.html#autotoc_md135", null ],
        [ "Symbol Visibility", "md_docs_2INSTALL.html#autotoc_md136", null ],
        [ "Advanced Methods", "md_docs_2INSTALL.html#autotoc_md137", null ]
      ] ],
      [ "3. Linking Against a Pre-Built Library", "md_docs_2INSTALL.html#autotoc_md139", [
        [ "Using CMake with <tt>find_package</tt>", "md_docs_2INSTALL.html#autotoc_md140", null ],
        [ "Using pkg-config", "md_docs_2INSTALL.html#autotoc_md141", null ],
        [ "Using xmake", "md_docs_2INSTALL.html#autotoc_md142", null ],
        [ "Example: Visual Studio Code Configuration", "md_docs_2INSTALL.html#autotoc_md143", null ]
      ] ]
    ] ],
    [ "Architectural Notes", "md_docs_2internals.html", [
      [ "1. Core Design Philosophy", "md_docs_2internals.html#autotoc_md145", [
        [ "1.1 Guiding Principles", "md_docs_2internals.html#autotoc_md146", null ],
        [ "1.2 Key Architectural Decisions", "md_docs_2internals.html#autotoc_md147", [
          [ "The Unity Build", "md_docs_2internals.html#autotoc_md148", null ],
          [ "The Emit Library", "md_docs_2internals.html#autotoc_md149", null ]
        ] ]
      ] ],
      [ "2. Compilation Pipeline", "md_docs_2internals.html#autotoc_md151", [
        [ "2.1 Lexer (Tokenization)", "md_docs_2internals.html#autotoc_md152", null ],
        [ "2.2 Parser", "md_docs_2internals.html#autotoc_md153", null ],
        [ "2.3 Semantic Analyzer", "md_docs_2internals.html#autotoc_md154", null ],
        [ "2.4 Code Generator", "md_docs_2internals.html#autotoc_md155", null ],
        [ "2.5 Virtual Machine", "md_docs_2internals.html#autotoc_md156", null ]
      ] ],
      [ "3. Language Features", "md_docs_2internals.html#autotoc_md158", [
        [ "3.1 Variables and Constants", "md_docs_2internals.html#autotoc_md159", null ],
        [ "3.2 Functions", "md_docs_2internals.html#autotoc_md160", null ],
        [ "3.3 Classes", "md_docs_2internals.html#autotoc_md161", null ],
        [ "3.4 Fibers (Coroutines)", "md_docs_2internals.html#autotoc_md162", null ],
        [ "3.5 Exceptions", "md_docs_2internals.html#autotoc_md163", null ]
      ] ],
      [ "4. The Emit Library", "md_docs_2internals.html#autotoc_md165", [
        [ "4.1 Supported Backends", "md_docs_2internals.html#autotoc_md166", null ],
        [ "4.2 Code Buffer", "md_docs_2internals.html#autotoc_md167", null ],
        [ "4.3 Instruction Emission", "md_docs_2internals.html#autotoc_md168", null ],
        [ "4.4 PE Format (Windows)", "md_docs_2internals.html#autotoc_md169", null ],
        [ "4.5 ELF Format (Unix)", "md_docs_2internals.html#autotoc_md170", null ]
      ] ],
      [ "5. Maintainer's Debugging Guide", "md_docs_2internals.html#autotoc_md172", [
        [ "Method 1: AST and Bytecode Dumping", "md_docs_2internals.html#autotoc_md173", null ],
        [ "Method 2: Verbose Mode", "md_docs_2internals.html#autotoc_md174", null ],
        [ "Method 3: Direct VM Inspection", "md_docs_2internals.html#autotoc_md175", null ],
        [ "Method 4: Test Suite", "md_docs_2internals.html#autotoc_md176", null ],
        [ "Useful Tools", "md_docs_2internals.html#autotoc_md177", null ]
      ] ]
    ] ],
    [ "Porting pulse to a New Architecture", "md_docs_2porting.html", [
      [ "Step 0: Research and Preparation", "md_docs_2porting.html#autotoc_md179", null ],
      [ "Step 1: Platform Detection (<tt>src/common/pulse_config.h</tt>)", "md_docs_2porting.html#autotoc_md180", null ],
      [ "Step 2: Implement the Emit Backend", "md_docs_2porting.html#autotoc_md181", null ],
      [ "Step 3: Integrate the New Backend", "md_docs_2porting.html#autotoc_md182", null ],
      [ "Step 4: Testing", "md_docs_2porting.html#autotoc_md183", null ],
      [ "Step 5: Platform-Specific VM Optimizations (Optional)", "md_docs_2porting.html#autotoc_md184", null ]
    ] ],
    [ "The infix Signature and Type System", "md_docs_2signatures.html", [
      [ "Part 2: The Signature Language Reference", "md_docs_2signatures.html#autotoc_md190", [
        [ "Part 1: Introduction", "md_docs_2signatures.html#autotoc_md186", [
          [ "1.1 The Challenge of Interoperability", "md_docs_2signatures.html#autotoc_md187", null ],
          [ "1.2 The Limitations of C Declarations", "md_docs_2signatures.html#autotoc_md188", null ],
          [ "1.3 Our Solution: A Human-First Signature System", "md_docs_2signatures.html#autotoc_md189", null ]
        ] ],
        [ "2.1 Primitives", "md_docs_2signatures.html#autotoc_md191", [
          [ "Tier 1: Abstract C Types", "md_docs_2signatures.html#autotoc_md192", null ],
          [ "Tier 2: Explicit Fixed-Width Types (Recommended)", "md_docs_2signatures.html#autotoc_md193", null ],
          [ "Tier 3: SIMD Vector Aliases", "md_docs_2signatures.html#autotoc_md194", null ]
        ] ],
        [ "2.2 Type Constructors and Composite Structures", "md_docs_2signatures.html#autotoc_md195", null ],
        [ "2.3 Syntax Showcase", "md_docs_2signatures.html#autotoc_md196", [
          [ "2.4 Bitfields and Flexible Arrays", "md_docs_2signatures.html#autotoc_md197", null ]
        ] ],
        [ "2.5 Scope and Namespaces", "md_docs_2signatures.html#autotoc_md198", null ],
        [ "2.6 Built-in Aliases vs. Registry Aliases", "md_docs_2signatures.html#autotoc_md199", [
          [ "Why this matters for Strings", "md_docs_2signatures.html#autotoc_md200", null ]
        ] ]
      ] ],
      [ "Part 3: The Named Type Registry", "md_docs_2signatures.html#autotoc_md202", [
        [ "Defining Types (<tt>infix_register_types</tt>)", "md_docs_2signatures.html#autotoc_md203", null ],
        [ "Using Named Types", "md_docs_2signatures.html#autotoc_md204", null ]
      ] ],
      [ "Part 4: Technical Specification", "md_docs_2signatures.html#autotoc_md206", null ]
    ] ],
    [ "Changelog", "md_CHANGELOG.html", [
      [ "Unreleased", "md_CHANGELOG.html#autotoc_md212", [
        [ "Changed", "md_CHANGELOG.html#autotoc_md213", null ]
      ] ]
    ] ],
    [ "Project Roadmap: infix FFI", "md_TODO.html", [
      [ "High Priority: Foundation & Stability", "md_TODO.html#autotoc_md215", null ],
      [ "Medium Priority: Expansion & Optimization", "md_TODO.html#autotoc_md216", null ],
      [ "Low Priority: Advanced Features & Polish", "md_TODO.html#autotoc_md217", null ],
      [ "High Priority: Foundation & Stability", "md_TODO.html#autotoc_md218", null ]
    ] ],
    [ "Security Policy", "md_SECURITY.html", [
      [ "Supported Versions", "md_SECURITY.html#autotoc_md220", null ],
      [ "Reporting a Vulnerability", "md_SECURITY.html#autotoc_md221", null ],
      [ "Security Model", "md_SECURITY.html#autotoc_md222", [
        [ "Mitigations", "md_SECURITY.html#autotoc_md223", [
          [ "1. W^X (Write XOR Execute) Memory Policy", "md_SECURITY.html#autotoc_md224", null ],
          [ "2. Use-After-Free Prevention (Guard Pages)", "md_SECURITY.html#autotoc_md225", null ],
          [ "3. Read-Only Context Hardening", "md_SECURITY.html#autotoc_md226", null ],
          [ "4. API Hardening Against Integer Overflows", "md_SECURITY.html#autotoc_md227", null ],
          [ "5. Continuous Security Validation (Fuzzing)", "md_SECURITY.html#autotoc_md228", null ]
        ] ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Variables", "functions_vars.html", "functions_vars" ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", "globals_eval" ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"180__emit_8c.html",
"emit_8h.html#ae39bf32cbb3e51537889bb3b2c97a842af151ce6ab946d17a55eb8d417db29ec9",
"emit__math_8c.html#a6b08bfc0ba7bf4e7f916e21471d90366",
"emit__x64_8h.html#a2cd522814329a91dafb922c4eb4a007e",
"include_2common_2infix__internals_8h_source.html",
"pulse_8h.html#a566a5349114079c0db992c80aadb58abae3c05d01e5713b8e32e46fb33c279e62",
"pulse__common_8h.html#ad83ed32ebc11e9c89f2ea9651a99f54d",
"structast__node.html#afe4c26297c4293e8998d46eb8771997c",
"structinfix__executable__t.html#af9a2a93cf175feed6882ba7bcf78a566"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';