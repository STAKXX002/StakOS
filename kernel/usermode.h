#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

/*
 * Drops CPL to 3 and jumps to `entry`, running on `user_stack`.
 * Never returns to the caller — execution continues at `entry` in
 * user mode. If `entry` ever "returns" (falls off the end of its
 * function), it will execute whatever garbage follows it in memory,
 * so any user-mode test function MUST end by calling a syscall like
 * SYS_EXIT rather than just returning. There is no safety net here.
 */
void enter_usermode(uint32_t entry, uint32_t user_stack);

#endif