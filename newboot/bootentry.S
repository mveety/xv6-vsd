#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"

.globl _start
_start = V2P_WO(bootelf_entry)

.globl bootelf_entry
bootelf_entry:
	mov $0x700000,%esp // just where ever
	mov $bootmain, %eax
	jmp *%eax
ohshit:
	hlt
	jmp ohshit
