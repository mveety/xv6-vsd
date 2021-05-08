#include "errstr.h"  // NO! BAD VEETY! NO NO NO NO
// Segments in proc->gdt.
#define NSEGS     7

// Per-CPU state
struct cpu {
	uchar id;                    // Local APIC ID; index into cpus[] below
	struct context *scheduler;   // swtch() here to enter scheduler
	struct taskstate ts;         // Used by x86 to find stack for interrupt
	struct segdesc gdt[NSEGS];   // x86 global descriptor table
	volatile uint started;       // Has the CPU started?
	int ncli;                    // Depth of pushcli nesting.
	int intena;                  // Were interrupts enabled before pushcli?
	
	// Cpu-local storage variables; see below
	struct cpu *cpu;
	struct proc *proc;           // The currently-running process.
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Per-CPU variables, holding pointers to the
// current cpu and to the current process.
// The asm suffix tells gcc to use "%gs:0" to refer to cpu
// and "%gs:4" to refer to proc.  seginit sets up the
// %gs segment register so that %gs refers to the memory
// holding those two variables in the local cpu's struct cpu.
// This is similar to how thread-local variables are implemented
// in thread libraries such as Linux pthreads.
extern struct cpu *cpu asm("%gs:0");       // &cpus[cpunum()]
extern struct proc *proc asm("%gs:4");     // cpus[cpunum()].proc

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
	uint edi;
	uint esi;
	uint ebx;
	uint ebp;
	uint eip;
};

enum procstate { 
	UNUSED,		// proc not in use
	EMBRYO,		// proc not totally initialized
	SLEEPING,	// zzzzzzz
	RUNNABLE,	// can run next scheduler round
	RUNNING,	// currently running
	ZOMBIE,		// waiting for parent to wait()
	SUMODE,		// user process that is runnable in sumode
	SULOCK,		// user process that is sleeping in sumode
	SURUN,		// user process that is running in sumode
};
enum proctype { PROCESS, THREAD };

// the maximum number of child threads
#define THREADMAX 32

// Per-process state
struct proc {
	uint sz;                     // Size of process memory (bytes)
	pde_t* pgdir;                // Page table
	char *kstack;                // Bottom of kernel stack for this process
	enum procstate state;        // Process state
	enum proctype type;			 // Process type
	int pid;                     // Process ID
	int uid;                     // User id
	int sid;                     // System id (-1 = global)
	pte_t *stackpte;			 // the stack pte.
	struct proc *parent;         // Parent process
	struct trapframe *tf;        // Trap frame for current syscall
	struct context *context;     // swtch() here to run process
	void *chan;                  // If non-zero, sleeping on chan
	int killed;                  // If non-zero, have been killed
	struct file *ofile[NOFILE];  // Open files
	struct inode *rootdir;       // the root directory inode
// TODO: this is total shit. write inode2path
	char rdirpath[128];          // path of the root dir (to make things easy)
	char injail;                 // path is jailed.
	struct inode *cwd;           // Current directory
	char errstr[ERRMAX];         // error strings make more sense than error codes.
	uchar errset;                // has errstr been set recently?
	char name[128];              // Process name (debugging)
	uint ssp;
	uint threadi;
	struct proc *threads[THREADMAX];	 // all child threads
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

int checksysperms(struct proc*, struct proc*);

