#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

/* Every command has this signature: argc + argv like standard C */
typedef void (*cmd_fn_t)(int argc, char** argv);

/* Command table entry */
typedef struct {
    const char* name;
    const char* help;
    cmd_fn_t    fn;
} command_t;

/* The command table (null-terminated) */
extern const command_t commands[];

/* Individual command implementations */
void cmd_help     (int argc, char** argv);
void cmd_clear    (int argc, char** argv);
void cmd_echo     (int argc, char** argv);
void cmd_meminfo  (int argc, char** argv);
void cmd_memtest  (int argc, char** argv);
void cmd_hexdump  (int argc, char** argv);
void cmd_heapinfo (int argc, char** argv);
void cmd_heaptest (int argc, char** argv);

#endif