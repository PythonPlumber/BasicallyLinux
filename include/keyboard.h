typedef void (*keyboard_callback_t)(void);

#define KEY_ARROW_UP 0x80
#define KEY_ARROW_DOWN 0x81
#define KEY_ARROW_LEFT 0x82
#define KEY_ARROW_RIGHT 0x83
#define KEY_DELETE 0x84
#define KEY_PAGE_UP 0x85
#define KEY_PAGE_DOWN 0x86

void keyboard_init(void);
void keyboard_set_callback(keyboard_callback_t callback);
int keyboard_read_char(char* out);
uint8_t get_keyboard_scancode(void);
char scancode_to_ascii(uint8_t scancode, int shift);
char keyboard_wait(void);
