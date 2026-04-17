# Porting pulse to a New Architecture

This guide outlines the steps required to add support for a new CPU architecture to the `pulse` compiler. The emit library is designed to be highly portable, with a clean separation between platform-agnostic logic and architecture-specific implementations.

We will use **RISC-V 64-bit (RV64GC)** with the standard **LP64D ABI** as a practical example throughout this guide.

## Step 0: Research and Preparation

This is the most critical step. Before writing any code, you must have a solid understanding of the target architecture. For RISC-V, this means studying the official instruction set architecture (ISA) documentation.

You need to answer these key questions:

*   **Instruction Encoding:** What is the instruction size? (Fixed 32-bit for RISC-V, variable for x86)
*   **Register Layout:** How many general-purpose registers? Floating-point registers?
*   **Calling Convention:** Which registers are used for argument passing? Return values?
*   **Memory Model:** Endianness? Alignment requirements?

## Step 1: Platform Detection (`src/common/pulse_config.h`)

The first code change is to teach the library how to recognize the new platform at compile time.

1.  **Add Architecture Macro**: Add a new `#define PULSE_ARCH_*` macro for your architecture.

    ```c
    // In src/common/pulse_config.h
    #elif defined(__riscv) && __riscv_xlen == 64
    #define PULSE_ARCH_RISCV64
    #else
    #error "Unsupported architecture."
    #endif
    ```

2.  **Select Emit Backend**: Choose which emit backend to use based on the architecture.

    ```c
    // In src/common/pulse_config.h
    #if defined(PULSE_ARCH_X64)
      #define PULSE_EMIT_PE_OR_ELF  // Can use PE (Windows) or ELF (Unix)
    #elif defined(PULSE_ARCH_RISCV64)
      #define PULSE_EMIT_ELF        // RISC-V typically uses ELF
    #endif
    ```

## Step 2: Implement the Emit Backend

1.  **Create New Files**: Create a new directory for your architecture in `src/emit/`.

    ```
    src/emit/riscv64/
        emit_riscv64.c
        emit_riscv64.h
        emit_riscv64_instructions.c
    ```

2.  **Implement Instruction Emitters**: Write functions to emit machine code instructions.

    ```c
    // In src/emit/riscv64/emit_riscv64.h
    typedef struct {
        uint8_t* buffer;
        size_t position;
        size_t capacity;
    } code_buffer;

    void emit_riscv64_addi(code_buffer* buf, uint8_t rd, uint8_t rs1, int16_t imm);
    void emit_riscv64_ld(code_buffer* buf, uint8_t rd, uint8_t rs1, int16_t imm);
    void emit_riscv64_sd(code_buffer* buf, uint8_t rs1, uint8_t rs2, int16_t imm);
    // ... etc for all instructions needed by the compiler
    ```

## Step 3: Integrate the New Backend

1.  **Update `emit/emit.c`**: Add conditional compilation to include your new backend.

    ```c
    // In src/emit/emit.c
    #if defined(PULSE_ARCH_X64)
    #include "x64/emit_x64.c"
    #elif defined(PULSE_ARCH_RISCV64)
    #include "riscv64/emit_riscv64.c"
    #endif
    ```

2.  **Update `pulse.c`**: Ensure the emit library is included in the unity build.

## Step 4: Testing

-   Run the compiler test suite on the new architecture
-   Verify generated code executes correctly
-   Test with real pulse programs

## Step 5: Platform-Specific VM Optimizations (Optional)

For maximum performance, you may want to implement architecture-specific VM instruction handlers.

*   This is optional and can be added incrementally
*   The portable C implementations in the VM will work on any architecture
