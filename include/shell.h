#ifndef SHELL_H
#define SHELL_H

typedef struct {
    char* name;
    void (*func)(int argc, char** argv);
} command_t;

void shell_init(void);
void shell_on_input(void);

#endif
