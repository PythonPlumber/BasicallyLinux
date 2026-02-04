ENTRY(_start)

SECTIONS
{
    /* The kernel starts at 1MB */
    . = 1M;

    .multiboot BLOCK(4K) : ALIGN(4K)
    {
        KEEP(*(.multiboot))
    }

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    .ai_model BLOCK(4K) : ALIGN(4K)
    {
        _ai_model_start = .;
        KEEP(*(.ai_model*))
        _ai_model_end = .;
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    kernel_end = .;
}
