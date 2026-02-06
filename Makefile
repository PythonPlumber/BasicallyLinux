CC ?= i686-linux-gnu-gcc
AS ?= nasm
LD ?= i686-linux-gnu-gcc
OBJCOPY ?= i686-linux-gnu-objcopy

CFLAGS := -ffreestanding -O2 -Wall -Wextra -m32 -fno-pie -fno-PIC -fno-stack-protector -nostdlib -nostdinc -Iinclude -msse -msse2
ASFLAGS := -f elf32
LDFLAGS := -T linker.ld -nostdlib -m32 -no-pie -Wl,-z,noexecstack

SRC_DIR := src
BUILD_DIR := build
MODEL_BLOB := $(firstword $(wildcard assets/smollm2.gguf) $(wildcard assets/smollm-135m.gguf) $(wildcard assets/SmolLM2-135M-Instruct-Q4_K_M.gguf))
MODEL_OBJ := $(BUILD_DIR)/model.o
ISO_DIR := $(BUILD_DIR)/isodir
ISO_IMAGE := $(BUILD_DIR)/basicallylinux.iso
GRUB_CFG := $(ISO_DIR)/boot/grub/grub.cfg

ASM_SRCS := $(shell find $(SRC_DIR) -name "*.asm")
C_SRCS := $(shell find $(SRC_DIR) -name "*.c")

ASM_OBJS := $(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/%.asm.o,$(ASM_SRCS))
C_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.c.o,$(C_SRCS))

BOOT_OBJ := $(BUILD_DIR)/boot.asm.o
OTHER_OBJS := $(sort $(filter-out $(BOOT_OBJ),$(ASM_OBJS) $(C_OBJS)))
OBJS := $(BOOT_OBJ) $(OTHER_OBJS)

ifneq ($(MODEL_BLOB),)
OBJS += $(MODEL_OBJ)
else
CFLAGS += -DNO_AI_MODEL
endif

all: $(BUILD_DIR) kernel.bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.asm.o: $(SRC_DIR)/%.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc

$(MODEL_OBJ): $(MODEL_BLOB)
	mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 --rename-section .data=.ai_model,alloc,load,readonly,data,contents $< $@

$(ISO_DIR)/boot/grub:
	mkdir -p $@

$(GRUB_CFG): $(ISO_DIR)/boot/grub
	printf "set timeout=5\nset default=0\nmenuentry \"BasicallyLinux\" {\n    multiboot /boot/kernel.bin\n" > $@
	if [ -n "$(MODEL_BLOB)" ]; then printf "    module /boot/$(notdir $(MODEL_BLOB)) \"ai_model\"\n" >> $@; fi
	printf "    boot\n}\n" >> $@

iso: kernel.bin $(GRUB_CFG)
	mkdir -p $(ISO_DIR)/boot
	cp kernel.bin $(ISO_DIR)/boot/kernel.bin
	if [ -n "$(MODEL_BLOB)" ]; then cp $(MODEL_BLOB) $(ISO_DIR)/boot/; fi
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)

clean:
	rm -rf $(BUILD_DIR) kernel.bin

.PHONY: all clean
