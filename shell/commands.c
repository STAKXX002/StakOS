#include "commands.h"
#include <stddef.h>

/*
 * The command table. Implementations live in:
 *   commands_core.c — help, clear, echo
 *   commands_mem.c  — meminfo, memtest, hexdump, heapinfo, heaptest
 *   commands_proc.c — ps, sleep, uptime, vminfo
 *   commands_user.c — synctest, usertest, elftest
 * sh_util.c holds sh_strtoul, shared by several of the above.
 */
const command_t commands[] = {
    { "help",     "list available commands",                    cmd_help     },
    { "clear",    "clear the screen",                           cmd_clear    },
    { "echo",     "print arguments to screen",                  cmd_echo     },
    { "meminfo",  "show PMM memory statistics",                 cmd_meminfo  },
    { "memtest",  "alloc/free N frames (default 8)",            cmd_memtest  },
    { "hexdump",  "dump memory: hexdump <addr> <len>",          cmd_hexdump  },
    { "heapinfo", "show kmalloc heap statistics",               cmd_heapinfo },
    { "heaptest", "alloc/free/read-back heap chunks",           cmd_heaptest },
    { "ps",       "list processes",                             cmd_ps       },
    { "sleep",    "sleep <ticks> (100 ticks = 1 second)",       cmd_sleep    },
    { "uptime",   "show ticks and seconds since boot",          cmd_uptime   },
    { "vminfo",   "show virtual memory info",                   cmd_vminfo   },
    { "synctest", "exercise int 0x80 syscall gate",             cmd_synctest },
    { "usertest", "spawn a process and run it at ring 3",       cmd_usertest },
    { "elftest",  "load and run the embedded test ELF binary",  cmd_elftest  },
    { NULL, NULL, NULL }   /* sentinel */
};