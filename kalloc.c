// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file

struct run {
	struct run *next;
};

struct {
	struct spinlock lock;
	int use_lock;
	struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
	initlock(&kmem.lock, "kmem");
	kmem.use_lock = 0;
	freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
	freerange(vstart, vend);
	kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
	char *p;
	p = (char*)PGROUNDUP((uint)vstart);
	for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
		kfree(p);
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
	struct run *r;

	if((uint)v % PGSIZE || v < end || v2p(v) >= PHYSTOP)
		panic("kfree");

	// Fill with junk to catch dangling refs.
	memset(v, 1, PGSIZE);

	if(kmem.use_lock)
		acquire(&kmem.lock);
	r = (struct run*)v;
	r->next = kmem.freelist;
	kmem.freelist = r;
	if(kmem.use_lock)
		release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
	struct run *r;

	if(kmem.use_lock)
		acquire(&kmem.lock);
	r = kmem.freelist;
	if(r)
		kmem.freelist = r->next;
	if(kmem.use_lock)
		release(&kmem.lock);
	return (char*)r;
}

// internal malloc/free
// nicked from K&R (more or less)

union header {
	struct {
		union header *ptr;
		uint size;
	};
	long x;
};

typedef union header Header;

static Header base;
static Header *freep;

#define NALLOC (4096/sizeof(Header))

void
kmfree(void *ap)
{
	Header *bp, *p;

	bp =(Header*)ap - 1;
	for(p = freep; !(bp > p && bp < p->ptr); p = p->ptr)
		if(p >= p->ptr && (bp > p || bp < p->ptr))
			break;
	if(bp+bp->size == p->ptr){
		bp->size += p->ptr->size;
		bp->ptr = p->ptr->ptr;
	} else
		bp->ptr = p->ptr;
	if(p+p->size == bp){
		p->size += bp->size;
		p->ptr = bp->ptr;
	} else
		p->ptr = bp;
	freep = p;
}

static Header*
mehrader(uint n)
{
	char *p;
	Header *hp;

	if(n < NALLOC)
		n = NALLOC;
	// issues with using kalloc:
	// 	kalloc doesn't let you allocate weird sizes, only pages. Therefore we
	// 	must precompute the amount of units instead of doing it the way K&R do
	// 	it.
	p = kalloc();
	if(!p)
		return 0;
	hp = (Header*)p;
	hp->size = n;
	kmfree((void*)(hp+1));
	return freep;
}

void*
kmalloc(uint nbytes)
{
	Header *p, *prev;
	uint nunits;

	nunits = (nbytes+sizeof(Header)-1)/sizeof(Header)+1;
	if((prev = freep) == 0){
		base.ptr = freep = prev = &base;
		base.size = 0;
	}
	for(p = prev->ptr; ; prev = p, p = p->ptr){
		if(p->size >= nunits){
			if(p->size == nunits)
				prev->ptr = p->ptr;
			else {
				p->size -= nunits;
				p += p->size;
				p->size = nunits;
			}
			freep = prev;
			return (void*)(p+1);
		}
		if(p == freep)
			if((p = mehrader(nunits)) == 0)
				return 0;
	}
}

void*
kmallocz(uint nbytes)
{
	void *p;

	p = kmalloc(nbytes);
	memset(p, 0, nbytes);
	return p;
}

