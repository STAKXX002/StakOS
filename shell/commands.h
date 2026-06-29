#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

typedef void (*cmd_fn_t)(int argc, char** argv);

typedef struct {
    const char* name;
    const char* help;
    cmd_fn_t    fn;
} command_t;

extern const command_t commands[];

void cmd_help     (int argc, char** argv);
void cmd_clear    (int argc, char** argv);
void cmd_echo     (int argc, char** argv);
void cmd_meminfo  (int argc, char** argv);
void cmd_memtest  (int argc, char** argv);
void cmd_hexdump  (int argc, char** argv);
void cmd_heapinfo (int argc, char** argv);
void cmd_heaptest (int argc, char** argv);
void cmd_ps       (int argc, char** argv);
void cmd_sleep    (int argc, char** argv);
void cmd_uptime   (int argc, char** argv);
void cmd_vminfo   (int argc, char** argv);
void cmd_synctest (int argc, char** argv);
void cmd_elftest  (int argc, char** argv);
void cmd_fstest   (int argc, char** argv);
void cmd_ls       (int argc, char** argv);
void cmd_run      (int argc, char** argv);

#endif