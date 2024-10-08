#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "elf.h"
#include "users.h"
#include "fs.h"
#include "file.h"

// defs for functions in vm.c
extern pte_t* walkpgdir(pde_t*, const void*, int);
extern int mappages(pde_t*, void*, uint, uint, int);

struct {
	struct spinlock lock;
	struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 0;
extern void forkret(void);
extern void trapret(void);
extern int booted;
extern int allowthreads;
extern int useclone;
extern int msgnote;

static int first = 1;

static void wakeup1(void *chan);

void yield1(void);
int wait1(void);
void flush_mailbox(Mailbox*);

void
pinit(void)
{
	initlock(&ptable.lock, "ptable");
}

// imported cpuid and mycpu from xv6-public
int
cpuid(void) {
	return mycpu()-cpus;
}

struct cpu*
mycpu(void)
{
	int apicid, i;

	if(readeflags()&FL_IF)
		panic("mycpu called with interrupts enabled\n");

	apicid = lapicid();
	for(i = 0; i < ncpu; ++i)
		if(cpus[i].id == apicid)
			return &cpus[i];

	panic("unknown apicid\n");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
	struct proc *p;
	char *sp;

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->state == UNUSED)
			goto found;
	}
	release(&ptable.lock);
	return 0;

found:
	p->state = EMBRYO;
	p->pid = nextpid++;
	p->sid = -1;
	p->uid = -2;
	memset(p->errstr, 0, ERRMAX);
	p->errset = 0;
	release(&ptable.lock);

	// Allocate kernel stack.
	if((p->kstack = kalloc()) == 0){
		p->state = UNUSED;
		return 0;
	}
	sp = p->kstack + KSTACKSIZE;
	
	// Leave room for trap frame.
	sp -= sizeof *p->tf;
	p->tf = (struct trapframe*)sp;
	
	// Set up new context to start executing at forkret,
	// which returns to trapret.
	sp -= 4;
	*(uint*)sp = (uint)trapret;

	sp -= sizeof *p->context;
	p->context = (struct context*)sp;
	memset(p->context, 0, sizeof *p->context);
	p->context->eip = (uint)forkret;

	// setup the mailbox
	initmailbox(&p->mbox);

	return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
	struct proc *p;
	extern char _binary_initcode_start[], _binary_initcode_size[];

	cprintf("cpu%d: starting init\n", cpu->id);
	p = allocproc();
	initproc = p;
	if((p->pgdir = setupkvm()) == 0)
		panic("userinit: out of memory?");
	inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
	p->sz = PGSIZE;
	memset(p->tf, 0, sizeof(*p->tf));
	p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
	p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
	p->tf->es = p->tf->ds;
	p->tf->ss = p->tf->ds;
	p->tf->eflags = FL_IF;
	p->tf->esp = PGSIZE;
	p->tf->eip = 0;  // beginning of initcode.S
	p->uid = -1;
	p->sid = -1;
	safestrcpy(p->name, "userinit", sizeof(p->name));
	p->rootdir = nil;
	p->cwd = namei("/");
	p->state = RUNNABLE;
	booted = 1;	// we consider the system booted when initproc is set
}

void
kernelinit(void)
{
	struct proc *p;
	extern char _binary_initkern_start[], _binary_initkern_size[];

	cprintf("cpu%d: starting kernel proc\n", cpu->id);
	p = allocproc();
	initproc = p;
	if((p->pgdir = setupkvm()) == 0)
		panic("kernelinit: out of memory?");
	inituvm(p->pgdir, _binary_initkern_start, (int)_binary_initkern_size);
	p->sz = PGSIZE;
	memset(p->tf, 0, sizeof(*p->tf));
	p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
	p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
	p->tf->es = p->tf->ds;
	p->tf->ss = p->tf->ds;
	p->tf->eflags = FL_IF;
	p->tf->esp = PGSIZE;
	p->tf->eip = 0;  // beginning of initcode.S
	p->uid = -1;
	p->sid = -1;
	safestrcpy(p->name, "kernel", sizeof(p->name));
	p->rootdir = nil;
	p->cwd = nil;
	p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
	uint sz;
	int i;
	
	sz = proc->sz;
	if(n > 0){
		if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
			return -1;
	} else if(n < 0){
		if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
			return -1;
	}
	proc->sz = sz;

	// update thread sizes
	if(proc->threadi > 0){
		for(i = 0; i < THREADMAX; i++)
			if(proc->threads[i]->state != UNUSED)
				proc->threads[i]->sz = sz;
	}
	switchuvm(proc);
	return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
	int i, pid;
	struct proc *np;

	// Allocate process.
	if((np = allocproc()) == 0){
		seterr(EPFORKNP);
		return -1;
	}
	// Copy process state from p.
	if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
		kfree(np->kstack);
		np->kstack = 0;
		np->state = UNUSED;
		seterr(EPFORKNS);
		return -1;
	}
	np->type = PROCESS;
	np->uid = proc->uid;
	np->sid = proc->sid;
	np->sz = proc->sz;
	np->parent = proc;
	*np->tf = *proc->tf;
	// Clear %eax so that fork returns 0 in the child.
	np->tf->eax = 0;

	for(i = 0; i < NOFILE; i++)
		if(proc->ofile[i])
			np->ofile[i] = filedup(proc->ofile[i]);

	np->rootdir = idup(proc->rootdir);
	np->cwd = idup(proc->cwd);
	np->threadi = 0;

	safestrcpy(np->name, proc->name, sizeof(proc->name));
 
	pid = np->pid;

	// lock to force the compiler to emit the np->state write last.
	acquire(&ptable.lock);
	np->state = RUNNABLE;
	release(&ptable.lock);
	
	return pid;
}

int
addchild(struct proc *par, struct proc *child)
{
	int i;

	for(i = 0; i < THREADMAX; i++){
		if(par->threads[i] == nil){
			par->threads[i] = child;
			par->threadi++;
			return 0;
		}
	}
	return 1;
}

int
removechild(struct proc *par, struct proc *child)
{
	int i;

	for(i = 0; i < THREADMAX; i++){
		if(par->threads[i] == child){
			par->threads[i] = nil;
			par->threadi--;
			return 0;
		}
	}
	return 1;
}

// tfork creates vsd 'kernel threads'. vsd kernel threads are the same as
// processes but share all the resources of the parent.
int
tfork(void)
{
	int pid;
	struct proc *np;
	pte_t *pstack, *cstack;
	pte_t *pte;
	uint pa, flags, i;
	uint esp_start;
	char *mem;
	int rval;
	uint saddr;

	// tfork is hereby disabled.
	fail(1, EKDISABLED, -1);
	fail(!allowthreads, EPNOTHREADS, -1);
	fail(useclone, EKDISABLED, -1);
	fail(proc->threadi >= THREADMAX, EP2MANYT, -1);
	if((np = allocproc()) == 0){
		seterr(EPFORKNP);
		return -1;
	}
	*np->tf = *proc->tf;
	// now tfork needs to alloc a new page for this threads stack. While it is
	// true that a thread shares its parent's memory it cannot share the parent's
	// stack.
	if(!(pstack = walkpgdir(proc->pgdir, (void*)proc->tf->esp, 0)))
		panic("tfork: could not find parent stack page");
	if(!(np->pgdir = setupkvm()))
		goto fail;

	for(i = 0; i < proc->sz; i += PGSIZE){
		if(!(pte = walkpgdir(proc->pgdir, (void*)i, 0)))
			panic("tfork: pte should exist");
		if(!(*pte & PTE_P))
			panic("tfork: page not present");
		pa = PTE_ADDR(*pte);
		flags = PTE_FLAGS(*pte);
		if(pa == PTE_ADDR(*pstack)){
			if(!(mem = kalloc()))
				goto bad;
			memmove(mem, (char*)p2v(pa), PGSIZE);
		} else
			mem = (char*)p2v(pa);
		if(mappages(np->pgdir, (void*)i, PGSIZE, v2p(mem), flags) < 0)
			goto bad;
	}

	np->type = THREAD;
	np->uid = proc->uid;
	np->sid = proc->sid;
	np->sz = proc->sz;
	np->parent = proc;
	np->tf->eax = 0;
	np->stackpte = walkpgdir(np->pgdir, (void*)np->tf->esp, 0);
	for(i = 0; i < NOFILE; i++)
		if(proc->ofile[i])
			np->ofile[i] = filedup(proc->ofile[i]);
	np->rootdir = idup(proc->rootdir);
	np->cwd = idup(proc->cwd);
	safestrcpy(np->name, proc->name, sizeof(proc->name));
	pid = np->pid;
	acquire(&ptable.lock);
	np->state = RUNNABLE;
	release(&ptable.lock);
	addchild(proc, np);
	return pid;
bad:
	freevm(np->pgdir);
fail:
	kfree(np->kstack);
	np->kstack = 0;
	np->state = UNUSED;
	werrstr(EPFORKNS, strlen(EPFORKNS));
	return -1;
}

// clone is like tfork except the generated process shared the same pagedir.
int
clone(uint stack)
{
	int pid;
	struct proc *np;
	char *mem;
	pte_t *pstack;
	uint spoffset;
	uint a, i, sz;

	fail(!allowthreads, EPNOTHREADS, -1);
	fail(!useclone, EKDISABLED, -1);
	fail(proc->threadi >= THREADMAX, EP2MANYT, -1);
	if((np = allocproc()) == 0){
		seterr(EPCLONENP);
		return -1;
	}
	*np->tf = *proc->tf;
//	cprintf("cpu%d: proc->ssp = %p, proc->tf->esp = %p, stack = %x\n", cpu->id, proc->ssp,
//			proc->tf->esp, stack);

	mem = (char*)stack;
	if((pstack = walkpgdir(proc->pgdir, (void*)proc->tf->esp, 0)) == 0)
		panic("clone: parent has no stack page");
	spoffset = proc->ssp - proc->tf->esp;
	memmove(mem, (char*)p2v(PTE_ADDR(*pstack)), PGSIZE);
	np->tf->esp = stack + PGSIZE;
	np->ssp = np->tf->esp;
	np->tf->esp -= spoffset;

//	cprintf("cpu%d: np->ssp = %p, proc->ssp = %p, np->tf->esp = %p,\n  proc->tf->esp = %p\n",
//			cpu->id, np->ssp, proc->ssp, np->tf->esp, proc->tf->esp);

	// clone parent's state
	np->pgdir = proc->pgdir;
	np->sz = proc->sz;
	np->type = THREAD;
	np->uid = proc->uid;
	np->sid = proc->sid;
	if(proc->type == PROCESS)
		np->parent = proc;
	else
		np->parent = proc->parent;
	np->tf->eax = 0;
	for(i = 0; i < NOFILE; i++)
		if(proc->ofile[i])
			np->ofile[i] = filedup(proc->ofile[i]);
	np->rootdir = idup(proc->rootdir);
	np->cwd = idup(proc->cwd);
	safestrcpy(np->name, proc->name, sizeof(proc->name));
	pid = np->pid;
	acquire(&ptable.lock);
	np->state = RUNNABLE;
	release(&ptable.lock);
	if(proc->type == THREAD)
		addchild(proc->parent, np);
	else
		addchild(proc, np);
	return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
	struct proc *p;
	int fd, i;
	struct inode *cwdir;

	if(proc == initproc)
		panic("init exiting");
		// Close all open files.
	for(fd = 0; fd < NOFILE; fd++){
		if(proc->ofile[fd]){
			fileclose(proc->ofile[fd]);
			proc->ofile[fd] = 0;
		}
	}

	begin_op(proc->cwd->dev);
	iput(proc->cwd);
	end_op(proc->cwd->dev);
	proc->cwd = 0;

	if(proc->type == PROCESS){
		// if we have threads kill them
		if(proc->threadi > 0){
			acquire(&ptable.lock);
			for(i = 0; i < THREADMAX; i++){
				if(proc->threads[i] != nil)
					proc->threads[i]->killed = 1;
			}
			if(proc->threadi > 0)
				while(wait1() != -1) ;
			else
				release(&ptable.lock);
		}
	}

	// empty the mailbox
	flush_mailbox(&proc->mbox);
	kmfree(proc->mbox.lock);
	
	acquire(&ptable.lock);
	// Parent might be sleeping in wait().
	wakeup1(proc->parent);
	// Pass abandoned children to init.
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->parent == proc){
			p->parent = initproc;
			if(p->state == ZOMBIE)
				wakeup1(initproc);
		}
	}

	// Jump into the scheduler, never to return.
	proc->state = ZOMBIE;
	sched();
	panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
	acquire(&ptable.lock);
	return wait1();
}

// wait1 is called when you need to wait but also need to hold
// ptable.lock. wait1 released the lock when it returns.
int
wait1(void)
{
	struct proc *p;
	int havekids, pid;

	for(;;){
		// Scan through table looking for zombie children.
		havekids = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->parent != proc)
				continue;
			havekids = 1;
			if(p->state == ZOMBIE && p->type == PROCESS && !p->crashed){
				// Found one.
				pid = p->pid;
				kfree(p->kstack);
				p->kstack = 0;
				if(p->type == PROCESS)
					freevm(p->pgdir);
				else if(p->type == THREAD && !useclone){
					kfree((char*)PTE_ADDR(p->stackpte));
					kfree((char*)p->pgdir);
				}
				if(p->type == THREAD)
					removechild(p->parent, p);
				p->state = UNUSED;
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				release(&ptable.lock);
				return pid;
			}
			if(p->state == ZOMBIE && p->type == PROCESS && p->crashed){
				// uh oh
				pid = p->pid;
				p->state = CRASHED;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if(!havekids || proc->killed){
			werrstr(EPWAITNOCH, strlen(EPWAITNOCH));
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		// sleep(proc, &ptable.lock);  //DOC: wait-sleep

		// use WAIT state instead. sleeping on the ptable.lock is fine, but it
		// could be the case that your threads are exiting on their own
		// while you are also exiting. If the thread beats you, you call
		// sleep, but have no children to wake you.
		proc->state = WAIT;
		proc->waitrounds = WAITROUNDS;
		sched();
	}
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
	struct proc *p;

	if(halted){  // the system's halted. stop all procs.
		cli();
		for(;;)
			hlt();
	}

	for(;;){
		// Enable interrupts on this processor.
		sti();
		// Loop over process table looking for process to run.
		acquire(&ptable.lock);
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

			// cull zombie threads. parents don't get notified about their
			// dead threads using wait, so might as well nuke them instead of
			// keeping them around.
			if(p->type == THREAD && p->state == ZOMBIE){
				kfree(p->kstack);
				p->kstack = 0;
				if(!useclone){
					kfree((char*)PTE_ADDR(p->stackpte));
					kfree((char*)p->pgdir);
				}
				removechild(p->parent, p);
				p->state = UNUSED;
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
			}

			// only let the kernel process run until things settle down
			if(first && p->pid != 0)
				continue;
			
			// so the scheduler runs as normal when not in singleuser.
			// when the scheduler is in singleuser, non-system processes
			// won't get executed and, if running, will get stopped. This
			// is to possibly help the system admin. If the machine is
			// put into singleuser when user processes are running,
			// generally something bad has happened. By preventing user
			// processes from running, the system processes get higher
			// priority and the other processes will not do anything
			// unpredictable to the machine.

			if(p->uid != -1 && singleuser == 1)
				continue;
			
			if(p->state != RUNNABLE){
				// this handles the new wait state. when processes
				// wait on children they get put in the wait state
				// which will transition to RUNNABLE when waitrounds
				// is <= 0. This is to handle the race where a process
				// might sleep waiting to be awoken by its non-existant
				// children. this is pretty hacky and wasteful and a
				// better solution needs to be found.
				if(p->state == WAIT){
					if(p->waitrounds > 0)
						p->waitrounds--;
					else
						p->state = RUNNABLE;
				}
				continue;
			}

			// Switch to chosen process.  It is the process's job
			// to release ptable.lock and then reacquire it
			// before jumping back to us.
			proc = p;
			switchuvm(p);
			p->state = RUNNING;
			swtch(&cpu->scheduler, proc->context);
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			proc = 0;
		}
		release(&ptable.lock);
	}
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
	int intena;

	if(!holding(&ptable.lock))
		panic("sched ptable.lock");
	if(cpu->ncli != 1)
		panic("sched locks");
	if(proc->state == RUNNING)
		panic("sched running");
	if(readeflags()&FL_IF)
		panic("sched interruptible");
	intena = cpu->intena;
	swtch(&proc->context, cpu->scheduler);
	cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield1(void)
{
	proc->state = RUNNABLE;
	sched();
}

void
yield(void)
{
	acquire(&ptable.lock);  //DOC: yieldlock
	yield1();
	release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
	// Still holding ptable.lock from scheduler.
	release(&ptable.lock);

	if (first && proc->pid == 0) {
		// Some initialization functions must be run in the context
		// of a regular process (e.g., they call sleep), and thus cannot 
		// be run from main().
		// fire up the filesystem
		fsinit(ROOTDEV);
		first = 0;
	}
	
	// Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
	if(proc == 0)
		panic("sleep");

	if(lk == 0)
		panic("sleep without lk");

	// Must acquire ptable.lock in order to
	// change p->state and then call sched.
	// Once we hold ptable.lock, we can be
	// guaranteed that we won't miss any wakeup
	// (wakeup runs with ptable.lock locked),
	// so it's okay to release lk.
	if(lk != &ptable.lock){  //DOC: sleeplock0
		acquire(&ptable.lock);  //DOC: sleeplock1
		release(lk);
	}

	// Go to sleep.
	proc->chan = chan;
	proc->state = SLEEPING;
	sched();

	// Tidy up.
	proc->chan = 0;

	// Reacquire original lock.
	if(lk != &ptable.lock){  //DOC: sleeplock2
		release(&ptable.lock);
		acquire(lk);
	}
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
	struct proc *p;

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if(p->state == SLEEPING && p->chan == chan)
			p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
	acquire(&ptable.lock);
	wakeup1(chan);
	release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
	struct proc *p;

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->pid == pid && checksysperms(p, proc) == 0){
			p->killed = 1;
			// Wake process from sleep if necessary.
			if(p->state == SLEEPING || p->state == MSGWAIT)
				p->state = RUNNABLE;
			release(&ptable.lock);
			return 0;
		}
	}
	release(&ptable.lock);
	return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
	static char *states[] = {
	[UNUSED]    "unused\0",
	[EMBRYO]    "embryo\0",
	[SLEEPING]  "sleep\0",
	[RUNNABLE]  "runable",
	[RUNNING]   "run\0",
	[ZOMBIE]    "zombie\0",
	[MSGWAIT]   "msgwait\0",
	[WAIT]		"wait\0",
	[CRASHED]	"crashed\0",
	};
	int i;
	struct proc *p;
	char *state;
	u32int totalmem = 0;

	cprintf("\nPROCESS DUMP\npid: type, state, name, sz, user, parent, threadi, waitrounds\n");
	if(halted)
		cprintf("system halted, no processes running\n");
	else {
		for(i = 0; i < NPROC; i++){
			p = &ptable.proc[i];
			if(p->state == UNUSED)
				continue;
			if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
				state = states[p->state];
			else
				state = "???";
			if(p->type == PROCESS)
				totalmem += p->sz/PGSIZE;
			cprintf("%d: %s, %s, %s, %u, %d, %d, %d, %d\n", p->pid,
					p->type == THREAD ? "thread" : "process", state, 
					p->name, p->sz/PGSIZE, p->uid, p->parent != nil ? p->parent->pid : 0,
					p->threadi, p->waitrounds);
		}
	}
	cprintf("MEMORY USED: %u pages\n", totalmem);
}

// werrstr copies buf to proc->errstr, sets p->errset, and returns
// the amount of bytes copied.
int
werrstr(char *buf, int nbytes)
{
	memset(proc->errstr, 0, ERRMAX);
	if(nbytes >= ERRMAX)
		nbytes = ERRMAX-1;
	memmove(proc->errstr, buf, nbytes);
	proc->errstr[nbytes] = '\0';
	proc->errset = 1;
	return nbytes;
}

// rerrstr copies proc->errstr to buf up to nbytes-1, clears proc->errset,
// and returns 0 if nbytes is greater than 0. if nbytes is zero (and
// conventionally buf is nil) then rerrstr will return 1 if proc->errset
// is not zero.
int
rerrstr(char *buf, int nbytes)
{
	if(nbytes == 0){
		if(proc->errset)
			return 1;
		else
			return 0;
	}
	memset(buf, 0, nbytes);
	memmove(buf, proc->errstr, nbytes-1);
	proc->errset = 0;
	return 0;
}
	
struct proc*
findpid(int pid)
{
	struct proc *rproc = nil;
	struct proc *p;

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if(p->pid == pid && p->state != UNUSED){
				rproc = p;
				break;
		}
	release(&ptable.lock);
	return rproc;
}

void
initmailbox(Mailbox *mbox)
{
	mbox->lock = kmalloc(sizeof(struct spinlock));
	initlock(mbox->lock, "mailbox");
}

int
add_message(Mailbox *mbox, Message *m)
{
	
	acquire(mbox->lock);
	m->next = nil;
	if(mbox->head == nil && mbox->tail == nil){
		mbox->tail = mbox->head = m;
		release(mbox->lock);
		return 0;
	}
	mbox->tail->next = m;
	mbox->tail = m;
	release(mbox->lock);
	return 0;
}

Message*
rem_message(Mailbox *mbox)
{
	Message *m;
	
	acquire(mbox->lock);
	if(mbox->head == nil){
		release(mbox->lock);
		return nil;
	}
	m = mbox->head;
	if(mbox->head == mbox->tail)
		mbox->head = mbox->tail = nil;
	else
		mbox->head = m->next;
	m->next = nil;
	release(mbox->lock);
	return m;
}

Message*
free_message(Message *m)
{
	Message *next;

	next = m->next;
	kmfree(m->data);
	kmfree(m);
	return next;
}

void
flush_mailbox(Mailbox *mbox)
{
	Message *cur;

	acquire(mbox->lock);
	cur = mbox->head;
	while(cur != nil)
		cur = free_message(cur);
	release(mbox->lock);
}

uint
precvwait(void)
{
	Mailbox *mbox;
	int waited = 0;
	uint hdsize = 0;
	
	mbox = &proc->mbox;
	acquire(mbox->lock);
	if(mbox->head == nil){
		release(mbox->lock);
		proc->state = MSGWAIT;
		if(msgnote)
			cprintf("cpu%d: recvwait %d: msgwait\n", cpu->id, proc->pid);
		waited = 1;
		acquire(&ptable.lock);
		sched();
		release(&ptable.lock);
	}
	if(waited)
		acquire(mbox->lock);
	hdsize = mbox->head->size;
	if(msgnote)
		cprintf("cpu%d: recvwait %d: new msg size = %d\n", cpu->id, proc->pid, hdsize);
	release(mbox->lock);
	return hdsize;
}

Message*
precvmsg(void)
{
	Mailbox *mbox;
	Message *msg;
	int waited = 0;

	mbox = &proc->mbox;

	acquire(mbox->lock);
	if(mbox->head == nil){
		release(mbox->lock);
		proc->state = MSGWAIT;
		if(msgnote)
			cprintf("cpu%d: recvmsg %d: msgwait\n", cpu->id, proc->pid);
		waited = 1;
		acquire(&ptable.lock);
		sched();
		release(&ptable.lock);
	}
	// if we get here there is a message in the box
	if(waited)
		acquire(mbox->lock);
	msg = mbox->head;
	mbox->head = mbox->head->next;
	if(mbox->head == nil)
		mbox->tail = nil;
	msg->next = nil;
	release(mbox->lock);
	if(msgnote)
		cprintf("cpu%d: recvmsg %d: new msg size = %d\n", cpu->id, proc->pid, msg->size);
	return msg;
}

int
psendmsg(struct proc *p, Message *msg)
{
	Mailbox *mbox;

	mbox = &p->mbox;
	if(msgnote)
		cprintf("cpu%d: sendmsg %d -> %d\n", cpu->id, proc->pid, p->pid);
	add_message(mbox, msg);
	if(p->state == MSGWAIT)
		p->state = RUNNABLE;
	return 0;
}
