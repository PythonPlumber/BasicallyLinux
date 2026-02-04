# AI Subsystem Architecture

## Overview
The OS features a native, kernel-level Artificial Intelligence subsystem designed for running Quantized Large Language Models (LLMs) on embedded or resource-constrained x86 hardware. Unlike traditional user-space AI runtimes, this subsystem is integrated directly into the kernel to minimize latency and memory overhead.

## Core Components

### 1. The Model Loader (`src/ai/ai_model.c`)
The loader handles the GGUF (GPT-Generated Unified Format) binary format, which is the industry standard for quantized local models.

- **Parsing**: Validates the `GGUF` magic bytes and parses the header version.
- **Tensor Mapping**: Instead of loading the entire model into heap memory, the kernel maps the model blob (linked as `.ai_model` section) directly into the kernel address space.
- **Quantization Support**: Native support for various quantization types:
    - `Q4_0`, `Q4_1` (4-bit weights)
    - `Q5_0`, `Q5_1` (5-bit)
    - `Q8_0` (8-bit)
    - `F16`, `F32` (Fallback)

### 2. The Neural Engine (`ai_matmul_q16_16`)
The heart of the AI subsystem is the Matrix Multiplication (MatMul) engine, optimized for fixed-point arithmetic.

- **Data Type**: `q16_16_t` (32-bit fixed-point: 16-bit integer, 16-bit fractional). This avoids the overhead and unpredictability of floating-point units (FPU) in interrupt contexts.
- **SIMD Acceleration**:
    - Uses inline assembly for **SSE4.1** instructions (`pmulld`, `psrad`).
    - **Vectorization**: Processes 4 tensor elements in parallel using `xmm` registers.
    - **Optimization**: Packs non-contiguous memory into temporary buffers for aligned SIMD loads.

```c
// Example of the SIMD kernel loop (simplified)
asm volatile(
    "movdqu (%1), %%xmm0\n"    // Load 4 values from A
    "movdqu (%2), %%xmm1\n"    // Load 4 values from B
    "pmulld %%xmm1, %%xmm0\n"  // Multiply packed integers
    "psrad $16, %%xmm0\n"      // Shift right to restore Q16.16 scale
    // ... accumulate result ...
);
```

### 3. Inference Pipeline
The inference process follows a standard Transformer architecture pipeline:

1.  **Tokenizer**: Encodes text prompts into integer tokens.
2.  **Embedding**: Looks up token vectors from the model weights.
3.  **Transformer Blocks**:
    - **RMSNorm**: Root Mean Square Normalization.
    - **QKV Attention**: Query-Key-Value attention mechanism with RoPE (Rotary Positional Embeddings).
    - **Feed Forward**: SwiGLU activation functions (`activation_silu`).
4.  **Sampling**: Top-P (Nucleus) sampling to select the next token.

### 4. KV Cache
To speed up generation, the kernel maintains a Key-Value (KV) Cache.
- **Structure**: Pre-allocated memory buffer storing previous attention contexts.
- **Management**: Ring-buffer style management to handle context windows (e.g., 2048 tokens).

### 5. Neural Dispatch Scheduler
To leverage multicore systems (SMP), the AI subsystem includes a dedicated job scheduler (`ai_scheduler`).
- **Parallelism**: Large matrix multiplications are tiled into smaller chunks and dispatched to worker threads across available CPU cores.
- **Lock-Free Queue**: Uses atomic operations for low-latency job submission.
- **Fork-Join Model**: The main inference thread submits tasks and waits for completion (`ai_scheduler_wait`), ensuring synchronization for layer-by-layer execution.

## Integration with OS
- **Direct Hardware Access**: The AI engine writes tokens directly to the VGA buffer for zero-latency display (`ai_stream_write`).
- **Benchmarking**: Built-in `bench_matmul` command allows profiling the neural engine's TOPS (Trillions of Operations Per Second) equivalent.

## Future Roadmap
- **AVX2 Support**: Extending the SIMD kernel to 256-bit registers (8 elements/cycle).
- **Background Inference**: Moving inference to a low-priority background thread (`SCHED_CLASS_IDLE`) to keep the UI responsive.
