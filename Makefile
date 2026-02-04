CC := i686-linux-gnu-gcc
AS := nasm
LD := i686-linux-gnu-gcc
OBJCOPY := i686-linux-gnu-objcopy

CFLAGS := -ffreestanding -O2 -Wall -Wextra -m32 -fno-pie -fno-stack-protector -nostdlib -nostdinc -Iinclude -msse -msse2
ASFLAGS := -f elf32
LDFLAGS := -T linker.ld -nostdlib -m32 -Wl,-z,noexecstack

SRC_DIR := src
BUILD_DIR := build
MODEL_BLOB := $(firstword $(wildcard assets/smollm-135m.gguf) $(wildcard assets/SmolLM2-135M-Instruct-Q4_K_M.gguf))
MODEL_OBJ := $(BUILD_DIR)/model.o

ASM_SRCS := $(shell find $(SRC_DIR) -name "*.s")
C_SRCS := $(filter-out $(SRC_DIR)/drivers/vga.c, $(shell find $(SRC_DIR) -name "*.c"))

ASM_OBJS := $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(ASM_SRCS))
C_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))

BOOT_OBJ := $(BUILD_DIR)/boot.o
OTHER_OBJS := $(sort $(filter-out $(BOOT_OBJ),$(ASM_OBJS) $(C_OBJS)))
OBJS := $(BOOT_OBJ) $(OTHER_OBJS)

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
	$(LD) $(LDFLAGS) $(OBJS) -lgcc

$(MODEL_OBJ): $(MODEL_BLOB)
	mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 --rename-section .data=.ai_model,alloc,load,readonly,data,contents $< $@

clean:
	rm -rf $(BUILD_DIR) kernel.bin

.PHONY: all clean
