// bootbin -- new bootloader for vsd
struct spinlock { // fake spinlock for fs.h
	int x;
};

#include "param.h"
#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"
#include "kbd.h"
#include "fs.h"
#include "newboot/smalloc.h"

int cur_line, cur_col, stiter;
extern void elfmain(void);
void putch(char);
void readsect(void*, uint);
void readfile(uint*, uchar*, uint, uint);
void panic(void);
void printst(void);
void memcpy(void*, void*, uint);

void
bootmain(void)
{
	// from bootmain.c
	void *kernel;
	void (*entry)(void);
	// from mbrboot.c
	struct superblock *sb;
	struct dinode *dn;
	struct dinode *kinode;
	uint *addrs;
	uint i = 0,j = 0;
	uint ksize;
	uint kblks;
	char *xptr;

// stage 0: initial set up
	cur_col = 0;
	cur_line = 0;
	stiter = 0;
	smalloc_init((void*)0x800000, 0x100000);
	for(i = 0; i < (80*25); i++)
		putch(' ');
	cur_line = 0;
	cur_col = 0;
	putch(' ');putch(' ');
	putch('v');putch('s');putch('d');putch('.');putch('b');putch('i');putch('n');
	putch(' ');
	putch('0');
	sb = smalloc(sizeof(struct superblock));
	dn = smalloc(sizeof(struct dinode));
	addrs = smalloc(512);
	kernel = (void*)0x100000;
	xptr = (void*)kernel;
	for(i = 0; i < 0x1000; i++)
		xptr[i] = '\0';

// stage 1: get disk metadata (test for system and get kernel inode)
	putch('\b');
	putch('1');
	readsect(sb, 1);
	if(!sb->system)
		panic();
	readsect(dn, ((sb->kerninode)/IPB + sb->inodestart));

// stage 2: read the kernel inode
	putch('\b');
	putch('2');
	kinode = &dn[sb->kerninode%IPB];
	ksize = kinode->size;
	kblks = ksize/65536;
	if(ksize%65536)
		kblks += 1;
	addrs = smalloc(512*kblks);
	xptr = (void*)addrs;
	for(i = 0; kinode->addrs[i] != 0; i++){
		printst();
		readsect(xptr, kinode->addrs[i]);
		xptr += BSIZE;
	}

// stage 3: read the kernel
	putch('\b');
	putch('3');
	// have the kernel inode!
	// now read the kernel
	readfile(addrs, kernel, ksize, 0);

// stage 5: jump to the kernel
	putch('\b');
	putch('5');
	entry = (void(*)(void))(kernel);
	entry();
	panic();
	// there is no return
}

void
putch(char c)
{
	int linpos;
	int print = 1;
	u16int *cur;
	u16int pdat;
	char colour = '\x07';
	static char *vga = (char*)0x000b8000;

	switch(c){
	case '\n':	// newline
		cur_line++;
		cur_col = 0;
		print = 0;
		break;
	case '\b':	// backspace
		print = 0;
		if(cur_col == 0){
			if(cur_line != 0){
				cur_col = 24;
				cur_line--;
			}
		} else {
			cur_col--;
		}
		break;
	}
	if(cur_col == 25){
		cur_line++;
		cur_col = 0;
	}
	if(cur_line == 80){
		cur_line++;
		cur_col = 0;
	}
	if(print){
		// calculate linear position 
		linpos = (cur_line*25)+cur_col;
		cur = (void*)vga+(linpos*2);
		pdat = (short)colour<<8;
		pdat = pdat|(short)c;
		*cur = pdat;
		cur_col++;
	}
}

void
waitdisk(void)
{
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

void
readfile(uint *addrs, uchar *pa, uint count, uint offset)
{
	int i = 0;
	int tblks;
	int offb;
	int offsb;

	tblks = count / BSIZE;  // tblks = 8
	pa -= offset % BSIZE;   // pa-0
	offb = offset / BSIZE;  // offb = 0
	
	for(i = 0; i < tblks; i++, pa += BSIZE, printst())
		readsect(pa, addrs[offb+i]);
}

void
panic(void)
{
	putch('!');
	for(;;)
		hlt();
}

void
printst(void)
{
	int colsave, linesave;

	if(stiter > 3)
		stiter = 0;
	colsave = cur_col;
	linesave = cur_line;
	cur_col = 0;
	cur_line = 0;
	switch(stiter){
	case 0:
		putch('|');
		break;
	case 1:
		putch('/');
		break;
	case 2:
		putch('-');
		break;
	case 3:
		putch('\\');
		break;
	}
	stiter++;
	cur_col = colsave;
	cur_line = linesave;
}

void
memcpy(void *dst, void *src, uint len)
{
	int i;
	char *dptr, *sptr;

	dptr = dst;
	sptr = src;
	for(i = 0; i < len; i++)
		dptr[i] = sptr[i];
}
