#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "users.h"

int
sys_fork(void)
{
	return fork();
}

int
sys_exit(void)
{
//	cprintf("cpu%d: exit call from pid %d (%s)\n",
//				cpu->id, proc->pid, proc->name);
	exit();
	return 0;  // not reached
}

int
sys_wait(void)
{
	return wait();
}

int
sys_kill(void)
{
	int pid;

	if(argint(0, &pid) < 0)
		return -1;
	return kill(pid);
}

int
sys_getpid(void)
{
	return proc->pid;
}

int
sys_sbrk(void)
{
	int addr;
	int n;

	if(argint(0, &n) < 0)
		return -1;
	addr = proc->sz;
	if(growproc(n) < 0)
		return -1;
	return addr;
}


int
sys_sleep(void)
{
	int n;
	uint ticks0;
	
	if(argint(0, &n) < 0)
		return -1;
	if(n == 0){
		yield();
		if(proc->killed)
			return -1;
		return 0;
	}
	acquire(&tickslock);
	ticks0 = ticks;
	n = n > 0 ? n*100 : 5;
	while(ticks - ticks0 < n){
		if(proc->killed){
			release(&tickslock);
			return -1;
		}
		sleep(&ticks, &tickslock);
	}
	release(&tickslock);
	return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
	uint xticks;
	
	acquire(&tickslock);
	xticks = ticks;
	release(&tickslock);
	return xticks;
}

int
sys_chuser(void)
{
	int uid;
	if(argint(0, &uid) < 0)
		return -1;
	return chuser(uid);
}

int
sys_chsystem(void)
{
	int sid;
	if(argint(0, &sid) < 0)
		return -1;
	return chsystem(sid);
}

int
sys_disallow(void)
{
	return disallow();
}

int
sys_getuid(void)
{
//	cprintf("pid%d: run syscall getuid\n", proc->pid);
	return proc->uid;
}

int
sys_getsid(void)
{
	return proc->sid;
}

int
sys_getts(void)
{
	return singleuser;
}

int
sys_errstr(void)
{
	int direc;
	char *buf;
	int nbytes;

	if(argint(0, &direc) < 0 || argint(2, &nbytes) < 0)
		return -1;
	if(nbytes > 0)
		if(argptr(1, &buf, nbytes) < 0)
			return -1;
	if(direc == 0)
		return rerrstr(buf, nbytes);
	else if(direc == 1)
		return werrstr(buf, nbytes);
	else
		return -1;
}

int
sys_tfork(void)
{
	return tfork();
}

int
sys_clone(void)
{
	uint stk;

	if(argint(0, (int*)&stk) < 0)
		return -1;
	return clone(stk);
}

int // int sendmsg(int pid, void *msg, int size)
sys_sendmsg(void)
{
	int pid;
	char *msgdata;
	int size;
	
	Message *msg;
	struct proc *p;

	if(argint(0, &pid) < 0 || argint(2, &size) < 0 || argptr(1, &msgdata, size) < 0)
		return -1;

	// if a pid isn't found return 0. messages are like mail, you don't know if they get there.
	if((p = findpid(pid)) == nil)
		return 0;

	msg = kmalloc(sizeof(Message));
	msg->size = size;
	msg->data = kmallocz(size);
	memcpy(msg->data, msgdata, size);
	psendmsg(p, msg);
	return 0;
}

int // int recvwait(void);
sys_recvwait(void)
{
	int sz;

	sz = (int)precvwait();
	return sz;
}

int // int recvmsg(void *msgbuf, int size)
sys_recvmsg(void)
{
	char *msgbuf;
	int size;

	Message *msg;
	int cpysize;

	if(argint(1, &size) < 0 || argptr(0, &msgbuf, size) < 0)
		return -1;

	msg = precvmsg();
	if(msg->size < size)
		cpysize = msg->size;
	else
		cpysize = size;
	memcpy(msgbuf, msg->data, cpysize);
	return 0;
}
