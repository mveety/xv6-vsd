struct spinlock { // fake spinlock for fs.h
	int x;
};

#include "param.h"
#include "types.h"
#include "x86.h"
#include "fs.h"

void readsect(void*, uint);

void
bootmain(void)
{
	struct superblock *sb;
	struct dinode *dn;
	struct dinode *bf;
	void *fsbuf;
	void *bootelf;
	void (*go)(void);
	uint *addrs;
	int i = 0;

	// malloc is for bitches
	fsbuf = (void*)0x8000;
	dn = (void*)0x8300;
	addrs = (void*)0x8600;
	bootelf = (void*)0x9000;
	readsect(fsbuf, 1);
	sb = (struct superblock*)fsbuf;
	readsect(dn, ((sb->bootinode) / IPB + sb->inodestart));
	bf = &dn[sb->bootinode%IPB];
	readsect(addrs, bf->addrs[0]);
	for(;;){
		if(addrs[i] == 0)
			break;
		readsect(bootelf+(i*BSIZE), addrs[i]);
		i++;
	}
	go = (void(*)(void))(bootelf);
	go();
}

void
waitdisk(void)
{
  // Wait for disk ready.
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// Read a single sector at offset into dst.
void
readsect(void *dst, uint offset)
{
  // Issue command.
  waitdisk();
  outb(0x1F2, 1);   // count = 1
  outb(0x1F3, offset);
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
  outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

  // Read data.
  waitdisk();
  insl(0x1F0, dst, BSIZE/4);
}

