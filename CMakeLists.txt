cmake_minimum_required(VERSION 3.16)

# For freestanding projects (kernels), we don't have a standard library yet.
# Tell CMake to only test the compiler by creating a static library instead of an executable.
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Force the NASM object format to 32-bit ELF
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
set(CMAKE_ASM_NASM_FLAGS "-f elf32")

project(BasicallyLinux C ASM_NASM)

# Toolchain configuration (can be overridden by command line)
if(NOT CMAKE_C_COMPILER)
    find_program(C_COMPILER_I686 i686-linux-gnu-gcc)
    find_program(C_COMPILER_ELF i686-elf-gcc)
    if(C_COMPILER_I686)
        set(CMAKE_C_COMPILER ${C_COMPILER_I686})
    elseif(C_COMPILER_ELF)
        set(CMAKE_C_COMPILER ${C_COMPILER_ELF})
    else()
        set(CMAKE_C_COMPILER gcc) # Fallback to system gcc
    endif()
endif()

if(NOT CMAKE_ASM_NASM_COMPILER)
    set(CMAKE_ASM_NASM_COMPILER nasm)
endif()

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
message(STATUS "Searching for sources in ${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.asm")
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Debugging: print found sources if they are empty
if(NOT ASM_SOURCES AND NOT C_SOURCES)
    message(WARNING "No source files found in ${CMAKE_CURRENT_SOURCE_DIR}/src. Trying relative path...")
    file(GLOB_RECURSE ASM_SOURCES "src/*.asm")
    file(GLOB_RECURSE C_SOURCES "src/*.c")
endif()

if(NOT ASM_SOURCES AND NOT C_SOURCES)
    message(FATAL_ERROR "No source files found in any search path!")
endif()

message(STATUS "Found ${ASM_SOURCES} ASM files")
message(STATUS "Found ${C_SOURCES} C files")

# AI model blob handling
set(MODEL_BLOB "${CMAKE_CURRENT_SOURCE_DIR}/model.bin")
if(EXISTS "${MODEL_BLOB}")
    set(MODEL_OBJ "model.o")
    add_custom_command(
        OUTPUT ${MODEL_OBJ}
        COMMAND i686-linux-gnu-objcopy -I binary -O elf32-i386 -B i386 
                --rename-section .data=.ai_model,alloc,load,readonly,data,contents 
                "${MODEL_BLOB}" "${MODEL_OBJ}"
        DEPENDS "${MODEL_BLOB}"
        COMMENT "Converting AI model blob to object file"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    # Use the generated object file in the link step
    set(MODEL_OBJ_PATH "${CMAKE_CURRENT_BINARY_DIR}/${MODEL_OBJ}")
else()
    message(WARNING "AI model blob not found. Kernel will be built without AI support.")
    set(MODEL_OBJ_PATH "")
    add_compile_definitions(NO_AI_MODEL)
endif()

# Ensure boot.asm is ALWAYS FIRST in the link order to keep Multiboot header at the top
set(BOOT_ASM "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86/boot.asm")
if(NOT EXISTS "${BOOT_ASM}")
    message(FATAL_ERROR "Could not find boot.asm at ${BOOT_ASM}")
endif()
list(REMOVE_ITEM ASM_SOURCES "${BOOT_ASM}")
set(ASM_SOURCES "${BOOT_ASM}" ${ASM_SOURCES})

# Exclude specific files if needed
# list(FILTER C_SOURCES EXCLUDE REGEX "src/drivers/vga.c")

# Build Kernel Binary
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")

add_executable(kernel.bin ${ASM_SOURCES} ${C_SOURCES} ${MODEL_OBJ_PATH})

target_link_options(kernel.bin PRIVATE
    "-T${LINKER_SCRIPT}"
    "-nostdlib"
    "-m32"
    "-no-pie"
    "-Wl,-z,noexecstack"
)

# Find libgcc
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -m32 -print-libgcc-file-name
    OUTPUT_VARIABLE LIBGCC_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_link_libraries(kernel.bin PRIVATE "${LIBGCC_PATH}")

set_target_properties(kernel.bin PROPERTIES
    SUFFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
