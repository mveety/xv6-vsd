// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock;
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf*);

/*
 * the ide devices (/dev/rdisk[n]) are defined by these functions. These
 * allow only *raw* reads and writes to the ide disk. The normal buffered
 * safe disk i/o should be done through /dev/disk[n] devices.
 * use mknod("/dev/disk[n]", 6, n) to create a raw disk device file. You
 * shouldn't use this. I'm serious.
 */

int ide_write(struct inode*, char*, int, int);
int ide_read(struct inode*, char*, int, int);

// Wait for IDE disk to become ready.
static int
idewait(int checkerr)
{
	int r;

	while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY) 
		;
	if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
		return -1;
	return 0;
}

void
ideinit(void)
{
	int i;
	uint diskid;
	
	initlock(&idelock, "ide");
	cprintf("cpu%d: driver: starting ide\n", cpu->id);
	picenable(IRQ_IDE);
	ioapicenable(IRQ_IDE, ncpu - 1);
	idewait(0);
	
	// Check if disk 1 is present
	outb(0x1f6, 0xe0 | (1<<4));
	for(i=0; i<1000; i++){
		if(inb(0x1f7) != 0){
			havedisk1 = 1;
			break;
		}
	}
	
	// Switch back to disk 0.
	outb(0x1f6, 0xe0 | (0<<4));
	devsw[6].read = ide_read;
	devsw[6].write = ide_write;
	diskid = register_disk(0, &iderw, nil);
	cprintf("cpu%d: ide: registered ide disk 0 as disk%d\n", cpu->id, diskid);
}

// Start the request for b.  Caller must hold idelock.
static void
idestart(struct buf *b)
{
	uint realdev;

	if(b == 0)
		panic("idestart");
	if(b->blockno >= FSSIZE){
		cprintf("cpu%d: b->blockno = %d\n", cpu->id, b->blockno);
		panic("incorrect blockno");
	}
	int sector_per_block =  BSIZE/SECTOR_SIZE;
	int sector = b->blockno * sector_per_block;

	if (sector_per_block > 7) panic("idestart");

	realdev = realdisk(b->dev);
	idewait(0);
	outb(0x3f6, 0);  // generate interrupt
	outb(0x1f2, sector_per_block);  // number of sectors
	outb(0x1f3, sector & 0xff);
	outb(0x1f4, (sector >> 8) & 0xff);
	outb(0x1f5, (sector >> 16) & 0xff);
	outb(0x1f6, 0xe0 | ((realdev&1)<<4) | ((sector>>24)&0x0f));
	if(b->flags & B_DIRTY){
		outb(0x1f7, IDE_CMD_WRITE);
		outsl(0x1f0, b->data, BSIZE/4);
	} else {
		outb(0x1f7, IDE_CMD_READ);
	}
}

// Interrupt handler.
void
ideintr(void)
{
	struct buf *b;

	// First queued buffer is the active request.
	acquire(&idelock);
	if((b = idequeue) == 0){
		release(&idelock);
		// cprintf("spurious IDE interrupt\n");
		return;
	}
	idequeue = b->qnext;

	// Read data if needed.
	if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
		insl(0x1f0, b->data, BSIZE/4);
	
	// Wake process waiting for this buf.
	b->flags |= B_VALID;
	b->flags &= ~B_DIRTY;
	wakeup(b);
	
	// Start disk on next buf in queue.
	if(idequeue != 0)
		idestart(idequeue);

	release(&idelock);
}

//PAGEBREAK!
// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
void
iderw(struct buf *b)
{
	struct buf **pp;

	if(!(b->flags & B_BUSY))
		panic("iderw: buf not busy");
	if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
		panic("iderw: nothing to do");

	acquire(&idelock);  //DOC:acquire-lock

	// Append b to idequeue.
	b->qnext = 0;
	for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue
		;
	*pp = b;
	
	// Start disk if necessary.
	if(idequeue == b)
		idestart(b);
	
	// Wait for request to finish.
	while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
		sleep(b, &idelock);
	}

	release(&idelock);
}

// reads and writes directly to the disk is disabled. its a bad idea.
int
ide_write(struct inode *ip, char *bf, int nbytes, int off)
{
	return 0;
}

int
ide_read(struct inode *ip, char *bf, int nbytes, int off)
{
	return 0;
}

