CC := i686-elf-gcc
AS := nasm
LD := i686-elf-gcc
OBJCOPY := i686-elf-objcopy

CFLAGS := -ffreestanding -O2 -Wall -Wextra -m32 -fno-pie -fno-stack-protector -nostdlib -nostdinc -Iinclude
ASFLAGS := -f elf32
LDFLAGS := -T linker.ld -ffreestanding -O2 -nostdlib -Wl,--build-id=none

SRC_DIR := src
BUILD_DIR := build
MODEL_BLOB := $(firstword $(wildcard assets/smollm-135m.gguf) $(wildcard assets/SmolLM2-135M-Instruct-Q4_K_M.gguf))
MODEL_OBJ := $(BUILD_DIR)/model.o

ASM_SRCS := $(wildcard $(SRC_DIR)/*.s $(SRC_DIR)/*/*.s $(SRC_DIR)/*/*/*.s)
C_SRCS := $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c $(SRC_DIR)/*/*/*.c)

ASM_OBJS := $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(ASM_SRCS))
C_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))

OBJS := $(ASM_OBJS) $(C_OBJS)

ifneq ($(MODEL_BLOB),)
OBJS += $(MODEL_OBJ)
endif

all: $(BUILD_DIR) kernel.bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(MODEL_OBJ): $(MODEL_BLOB)
	mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 --rename-section .data=.ai_model,alloc,load,readonly,data,contents $< $@

clean:
	rm -rf $(BUILD_DIR) kernel.bin

.PHONY: all clean
