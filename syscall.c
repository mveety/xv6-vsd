#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
	if(addr >= proc->sz || addr+4 > proc->sz)
		return -1;
	*ip = *(int*)(addr);
	return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint addr, char **pp)
{
	char *s, *ep;

	if(addr >= proc->sz)
		return -1;
	*pp = (char*)addr;
	ep = (char*)proc->sz;
	for(s = *pp; s < ep; s++)
		if(*s == 0)
			return s - *pp;
	return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
	return fetchint(proc->tf->esp + 4 + 4*n, ip);
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size n bytes.  Check that the pointer
// lies within the process address space.
int
argptr(int n, char **pp, int size)
{
	int i;
	
	if(argint(n, &i) < 0)
		return -1;
	if((uint)i >= proc->sz || (uint)i+size > proc->sz)
		return -1;
	*pp = (char*)i;
	return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
	int addr;
	if(argint(n, &addr) < 0)
		return -1;
	return fetchstr(addr, pp);
}

extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_getpid(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);
extern int sys_chuser(void);
extern int sys_chsystem(void);
extern int sys_disallow(void);
extern int sys_getuid(void);
extern int sys_getsid(void);
extern int sys_getts(void);
extern int sys_chroot(void);
extern int sys_errstr(void);
extern int sys_chperms(void);
extern int sys_seek(void);
extern int sys_tfork(void);
extern int sys_clone(void);

static int (*syscalls[])(void) = {
[SYS_fork]    	sys_fork, // creates a new process
[SYS_exit]    	sys_exit, // kills current process
[SYS_wait]    	sys_wait, // waits for a child process to exit
[SYS_pipe]    	sys_pipe, // makes two interconnected file descriptors
[SYS_read]    	sys_read, // reads bytes from a file descriptor
[SYS_kill]    	sys_kill, // kills a process
[SYS_exec]    	sys_exec, // loads and runs a new process image
[SYS_fstat]   	sys_fstat, // gets info about a file
[SYS_chdir]   	sys_chdir, // changes current directory
[SYS_dup]     	sys_dup, // duplicates a fd
[SYS_getpid]  	sys_getpid, // gets process's current pid
[SYS_sbrk]    	sys_sbrk, // allocate more memory to process
[SYS_sleep]   	sys_sleep, // idle the process for a while
[SYS_uptime]  	sys_uptime, // how many ticks has the machine been up
[SYS_open]    	sys_open, // create a file descriptor from a file
[SYS_write]   	sys_write, // write bytes to a file
[SYS_mknod]   	sys_mknod, // make a special file
[SYS_unlink]  	sys_unlink, // delete a file's name
[SYS_link]    	sys_link, // make a file that points to another file
[SYS_mkdir]   	sys_mkdir, // create a directory
[SYS_close]   	sys_close, // close a fd
[SYS_chuser]  	sys_chuser, // change uid
[SYS_chsystem] 	sys_chsystem, // change system id
[SYS_disallow] 	sys_disallow, // toggle single user mode
[SYS_getuid]  	sys_getuid, // get the current proc's user id
[SYS_getsid]  	sys_getsid, // get the current proc's system id
[SYS_getts]		sys_getts, // returns 0 if in single user mode, 1 if in multiuser
[SYS_chroot] 	sys_chroot, // change the effective root directory of a process
[SYS_errstr] 	sys_errstr, // fiddles with the errstr (you'll never use this directly)
[SYS_chperms] 	sys_chperms, // changes file permissions
[SYS_seek]		sys_seek, // seeks a file descriptor
[SYS_tfork]		sys_tfork, // creates a thread process
[SYS_clone]		sys_clone, // yet another threading mechanism
};

void
syscall(void)
{
	int num;

	num = proc->tf->eax;
	if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
		proc->tf->eax = syscalls[num]();
	} else {
		cprintf("%d %s: unknown sys call %d\n",
						proc->pid, proc->name, num);
		proc->tf->eax = -1;
	}
}

