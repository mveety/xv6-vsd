#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "users.h"

int singleuser;
int booted;
u32int lowmem;
u32int highmem;
u32int totalmem;
static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
	char *p;
	u16int *mem1;
	u16int *mem2;
	u32int allocmem1;
	u32int curend;

	mem1 = (void*)0x500;
	mem2 = (void*)0x600;
	allocmem1 = lowmem = *mem1;
	highmem = (*mem2)*64;
	totalmem = lowmem+highmem;

	singleuser = 1; // boot into single user mode initally
	booted = 0;
	syscons = 1; // cga/ega/vga is the system console
	sysuart = 1; // make the uart initally print messages

	cgamove(1, 0);
	cgaprintstr("xv6...\n");
	earlyuartprintstr("\nxv6...\n");
	consoleinit1();   // I/O devices
	kinit1(end, P2V(4*1024*1024)); // phys page allocator
	allocmem1 -= 4*1024*1024;
	curend = 4*1024*1024;
	kvmalloc();      // kernel page table
	cprintf("memory = %u kb (low = %u, high = %u)\n", totalmem, lowmem, highmem);
	cprintf("starting vsd release 1\n");
	mpinit();        // collect info about this machine
	lapicinit();
	if(!ismp)
		ncpu = 1;
	cprintf("mpinit: found %d processors\n", ncpu);
	seginit();       // set up segments
	picinit();       // interrupt controller
	ioapicinit();    // another interrupt controller
	consoleinit2();   // I/O device's interrupts
	uartinit();      // serial port
	pinit();         // process table
	tvinit();        // trap vectors
	if(!ismp){
		cprintf("cpu%d: not using mp kernel\n", cpu->id);
		timerinit();   // uniprocessor timer
	}
	startothers();   // start other processors
	// free the memory so the kernel knows about it
	if(allocmem1 > 0)
		kinit2(P2V(curend), P2V(lowmem));
	if(highmem > 0)
		kinit2(P2V(ISAHOLEEND), P2V(highmem));
	ideinit();       // disk
	binit();         // buffer cache and /dev/disk*
	fileinit();      // file tables
	bitbucket_init(); // /dev/null, /dev/zero, /dev/robpike devices
	sysctl_init();  // /dev/sysctl
	sysname_init(); // /dev/sysname
	muxinit();		// /dev/mux*
	kernelinit();	// kernel process
	userinit();      // first user process
	// Finish setting up this processor in mpmain.
	cprintf("cpu%d: starting timesharing\n", cpu->id);
	mpmain();
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
	switchkvm(); 
	seginit();
	lapicinit();
	mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
	idtinit();       // load idt register
	xchg(&cpu->started, 1); // tell startothers() we're up
	scheduler();     // start running processes
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
	extern uchar _binary_entryother_start[], _binary_entryother_size[];
	uchar *code;
	struct cpu *c;
	char *stack;

	// Write entry code to unused memory at 0x7000.
	// The linker has placed the image of entryother.S in
	// _binary_entryother_start.
	code = p2v(0x7000);
	memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

	for(c = cpus; c < cpus+ncpu; c++){
		if(c == cpus+cpunum())  // We've started already.
			continue;

		// Tell entryother.S what stack to use, where to enter, and what 
		// pgdir to use. We cannot use kpgdir yet, because the AP processor
		// is running in low  memory, so we use entrypgdir for the APs too.
		stack = kalloc();
		*(void**)(code-4) = stack + KSTACKSIZE;
		*(void**)(code-8) = mpenter;
		*(int**)(code-12) = (void *) v2p(entrypgdir);

		lapicstartap(c->id, v2p(code));

		// wait for cpu to finish mpmain()
		while(c->started == 0)
			;
	}
}

// Boot page table used in entry.S and entryother.S.
// Page directories (and page tables), must start on a page boundary,
// hence the "__aligned__" attribute.  
// Use PTE_PS in page directory entry to enable 4Mbyte pages.
__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
	// Map VA's [0, 4MB) to PA's [0, 4MB)
	[0] = (0) | PTE_P | PTE_W | PTE_PS,
	// Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
	[KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
