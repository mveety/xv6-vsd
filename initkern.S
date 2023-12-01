# kernel process

#include "syscall.h"
#include "traps.h"

# for(;;) sleep(5);
.globl start
start:
	pushl $5  // wake every five seconds
	pushl $0  // where caller pc would be
	movl $SYS_sleep, %eax
	int $T_SYSCALL
	jmp start
