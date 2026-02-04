cmake_minimum_required(VERSION 3.16)

# For freestanding projects (kernels), we don't have a standard library yet.
# Tell CMake to only test the compiler by creating a static library instead of an executable.
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Force the NASM object format to 32-bit ELF
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
set(CMAKE_ASM_NASM_FLAGS "-f elf32")

project(BasicallyLinux C ASM_NASM)

# Toolchain configuration (can be overridden by command line)
set(CMAKE_C_COMPILER i686-linux-gnu-gcc)
set(CMAKE_ASM_NASM_COMPILER nasm)

# Compilation flags for C files only
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C>:-O2>
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-m32>
    $<$<COMPILE_LANGUAGE:C>:-fno-pie>
    $<$<COMPILE_LANGUAGE:C>:-fno-PIC>
    $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
    $<$<COMPILE_LANGUAGE:C>:-nostdlib>
    $<$<COMPILE_LANGUAGE:C>:-nostdinc>
    $<$<COMPILE_LANGUAGE:C>:-msse>
    $<$<COMPILE_LANGUAGE:C>:-msse2>
)

include_directories(include)

# Source discovery
file(GLOB_RECURSE ASM_SOURCES "src/*.asm")
file(GLOB_RECURSE C_SOURCES "src/*.c")

# Ensure boot.asm is ALWAYS FIRST in the link order to keep Multiboot header at the top
set(BOOT_ASM "${CMAKE_CURRENT_SOURCE_DIR}/src/boot.asm")
list(REMOVE_ITEM ASM_SOURCES "${BOOT_ASM}")
set(ASM_SOURCES "${BOOT_ASM}" ${ASM_SOURCES})

# Exclude specific files if needed
list(FILTER C_SOURCES EXCLUDE REGEX "src/drivers/vga.c")

# Handle AI Model Blob
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(MODEL_BLOB "")

# Check for model candidates
set(CANDIDATES
    "${ASSETS_DIR}/smollm2.gguf"
    "${ASSETS_DIR}/smollm-135m.gguf"
    "${ASSETS_DIR}/SmolLM2-135M-Instruct-Q4_K_M.gguf"
)

foreach(CANDIDATE ${CANDIDATES})
    if(EXISTS "${CANDIDATE}")
        set(MODEL_BLOB "${CANDIDATE}")
        message(STATUS "Found AI model blob: ${MODEL_BLOB}")
        break()
    endif()
endforeach()

if(MODEL_BLOB)
    set(MODEL_OBJ "${CMAKE_CURRENT_BINARY_DIR}/model.o")
    add_custom_command(
        OUTPUT ${MODEL_OBJ}
        COMMAND i686-linux-gnu-objcopy -I binary -O elf32-i386 -B i386 
                --rename-section .data=.ai_model,alloc,load,readonly,data,contents 
                "${MODEL_BLOB}" "${MODEL_OBJ}"
        DEPENDS "${MODEL_BLOB}"
        COMMENT "Converting AI model blob to object file"
    )
    # Add the object file to the sources
    list(APPEND ASM_SOURCES ${MODEL_OBJ})
else()
    message(WARNING "AI model blob not found. Kernel will be built without AI support.")
endif()

# Build Kernel Binary
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")

# We use add_executable but we need to control the linking process
add_executable(kernel.bin ${ASM_SOURCES} ${C_SOURCES})

set_target_properties(kernel.bin PROPERTIES
    LINK_FLAGS "-T \"${LINKER_SCRIPT}\" -nostdlib -m32 -no-pie -Wl,-z,noexecstack"
    SUFFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# Link with libgcc for arithmetic operations
target_link_libraries(kernel.bin gcc)
