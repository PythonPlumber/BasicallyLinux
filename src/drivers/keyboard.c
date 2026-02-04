#include "keyboard.h"
#include "idt.h"
#include "ports.h"
#include "types.h"

#define KEYBOARD_BUFFER_SIZE 128

static volatile char key_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t key_head = 0;
static volatile uint32_t key_tail = 0;
static keyboard_callback_t key_callback = 0;
static uint8_t shift_down = 0;
static uint8_t ctrl_down = 0;
static uint8_t extended_down = 0;

static const char scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7',
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.'
};

static const char scancode_map_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7',
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.'
};

uint8_t get_keyboard_scancode(void) {
    return inb(0x60);
}

char scancode_to_ascii(uint8_t scancode, int shift) {
    if (scancode >= 128) {
        return 0;
    }
    return shift ? scancode_map_shift[scancode] : scancode_map[scancode];
}

char keyboard_wait(void) {
    char ch = 0;
    while (!keyboard_read_char(&ch)) { }
    return ch;
}

static void buffer_push(char c) {
    uint32_t next = (key_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next == key_tail) {
        return;
    }
    key_buffer[key_head] = c;
    key_head = next;
}

int keyboard_read_char(char* out) {
    if (key_tail == key_head) {
        return 0;
    }
    *out = key_buffer[key_tail];
    key_tail = (key_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return 1;
}

static registers_t* keyboard_irq(registers_t* regs) {
    uint8_t scancode = inb(0x60);
    if (scancode == 0xE0) {
        extended_down = 1;
        return regs;
    }
    if (extended_down) {
        extended_down = 0;
        if (scancode == 0x48) {
            buffer_push((char)KEY_ARROW_UP);
        } else if (scancode == 0x50) {
            buffer_push((char)KEY_ARROW_DOWN);
        } else if (scancode == 0x4B) {
            buffer_push((char)KEY_ARROW_LEFT);
        } else if (scancode == 0x4D) {
            buffer_push((char)KEY_ARROW_RIGHT);
        } else if (scancode == 0x53) {
            buffer_push((char)KEY_DELETE);
        } else if (scancode == 0x49) {
            buffer_push((char)KEY_PAGE_UP);
        } else if (scancode == 0x51) {
            buffer_push((char)KEY_PAGE_DOWN);
        }
        if (key_callback) {
            key_callback();
        }
        return regs;
    }
    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = 1;
        return regs;
    }
    if (scancode == 0x1D) {
        ctrl_down = 1;
        return regs;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_down = 0;
        return regs;
    }
    if (scancode == 0x9D) {
        ctrl_down = 0;
        return regs;
    }
    if (scancode & 0x80) {
        return regs;
    }
    char c = shift_down ? scancode_map_shift[scancode] : scancode_map[scancode];
    if (ctrl_down && c >= 'A' && c <= 'Z') {
        c = (char)((c - 'A' + 1) & 0x1F);
    } else if (ctrl_down && c >= 'a' && c <= 'z') {
        c = (char)((c - 'a' + 1) & 0x1F);
    }
    if (c) {
        buffer_push(c);
    }
    if (key_callback) {
        key_callback();
    }
    return regs;
}

void keyboard_set_callback(keyboard_callback_t callback) {
    key_callback = callback;
}

void keyboard_init(void) {
    key_head = 0;
    key_tail = 0;
    key_callback = 0;
    register_interrupt_handler(33, keyboard_irq);
    uint8_t mask = inb(0x21);
    outb(0x21, (uint8_t)(mask & 0xFD));
}
