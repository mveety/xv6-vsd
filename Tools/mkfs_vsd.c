#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat  // avoid clash with host struct stat
#include "../types.h"
#include "../fs.h"
#include "../stat.h"
#include "../param.h"

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

#define min(a, b) ((a) < (b) ? (a) : (b))

// like make something sensible. have a 10,000 block fs
// maybe make 1000 inodes? I dunno.
#define NINODES (FSSIZE/10)

// user permissions
#define U_READ 0x0001
#define U_WRITE 0x0002
#define U_EXEC 0x0004
#define U_HIDDEN 0x0008
// world permissions
#define W_READ 0x0100
#define W_WRITE 0x0200
#define W_EXEC 0x0400
#define W_HIDDEN 0x0800

#define sysperms U_READ|W_READ
#define binperms U_READ|U_EXEC|W_READ|W_EXEC
#define etcperms U_READ|U_WRITE|W_READ

#define P_READ 0x1000
#define P_WRITE 0x0100
#define P_EXEC 0x0010
#define P_HIDDEN 0x0001

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE/(BSIZE*8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;  
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
uint iallocp(ushort type, ushort perms);
void iappend(uint inum, void *p, int n);
void iappendm(uint inum, void *p, int n, int mode);
uint makedir(uint parent, char *name);

typedef struct {
	uint block;  // which indirect block is it?
	uint ptr; // which ptr in that block
} indir_ptr;

indir_ptr
offset2real(uint off)
{
	indir_ptr buf;

	buf.block = off / NINDIRECT;
	buf.ptr = off % NINDIRECT;
	return buf;
}

// convert to intel byte order
ushort
xshort(ushort x)
{
	ushort y;
	uchar *a = (uchar*)&y;
	a[0] = x;
	a[1] = x >> 8;
	return y;
}

uint
xint(uint x)
{
	uint y;
	uchar *a = (uchar*)&y;
	a[0] = x;
	a[1] = x >> 8;
	a[2] = x >> 16;
	a[3] = x >> 24;
	return y;
}

int
main(int argc, char *argv[])
{
	int i, cc, fd, m;
	uint rootino, binino, etcino, inum, off;
	uint bootino;
	struct dirent de;
	char buf[BSIZE];
	struct dinode din;

	static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");
	if(argc < 2){
		fprintf(stderr, "Usage: mkfs fs.img label files...\n");
		exit(1);
	}
	assert((BSIZE % sizeof(struct dinode)) == 0);
	assert((BSIZE % sizeof(struct dirent)) == 0);
	fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
	if(fsfd < 0){
		perror(argv[1]);
		exit(1);
	}
	// 1 fs block = 1 disk sector
	nmeta = 2 + nlog + ninodeblocks + nbitmap;
	nblocks = FSSIZE - nmeta;
	memset(sb.label, 0, 11);
	if(strlen(argv[2]) > 10)
		strcpy(sb.label, "NO NAME\0");
	else
		memcpy(sb.label, argv[2], strlen(argv[2]));
	sb.version = 0;
	sb.size = xint(FSSIZE);
	sb.nblocks = xint(nblocks);
	sb.ninodes = xint(NINODES);
	sb.nlog = xint(nlog);
	sb.logstart = xint(2);
	sb.inodestart = xint(2+nlog);
	sb.bmapstart = xint(2+nlog+ninodeblocks);
	sb.bootable = 0;
	sb.system = 1;
	sb.bootinode = 0;
	printf("mkfs: vsd new filesystem\n");
	printf("mkfs: label %s, version %d\n", sb.label, sb.version);
	printf("mkfs: meta %d (boot, super, log %u, inode %u, bitmap %u) blocks %d total %d\n",
				 nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);
	freeblock = nmeta;     // the first free block that we can allocate
	printf("mkfs: reaming");
	for(i = 0; i < FSSIZE; i++){
		if(i%100 == 0)
			printf(".");
		wsect(i, zeroes);
	}
	memset(buf, 0, sizeof(buf));
	memmove(buf, &sb, sizeof(sb));
	wsect(1, buf);
// we can't create / with makedir because it needs a parent.
	rootino = ialloc(T_DIR);
	assert(rootino == ROOTINO);
	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, ".");
	iappend(rootino, &de, sizeof(de));
	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, "..");
	iappend(rootino, &de, sizeof(de));
// this makes /bin, /etc, and /dev
	binino = makedir(rootino, "bin");
	etcino = makedir(rootino, "etc");
	bootino = makedir(rootino, "boot");
	makedir(rootino, "dev");
	printf("adding files");
	for(i = 3; i < argc; i++){
		assert(index(argv[i], '/') == 0);

		if((fd = open(argv[i], 0)) < 0){
			perror(argv[i]);
			exit(1);
		}
		printf(".");
		// Skip leading _ in name when writing to file system.
		// The binaries are named _rm, _cat, etc. to keep the
		// build operating system from trying to execute them
		// in place of system binaries like rm and cat.
		if(argv[i][0] == '_'){
			// if it has a leading _ it goes in /bin
			++argv[i];
			inum = ialloc(T_FILE);
//			printf("mkfs: adding file %s (inode %d)\n", argv[i], inum);
			bzero(&de, sizeof(de));
			de.inum = xshort(inum);
			strncpy(de.name, argv[i], DIRSIZ);
			iappendm(binino, &de, sizeof(de), binperms);
			while((cc = read(fd, buf, sizeof(buf))) > 0)
				iappendm(inum, buf, cc, binperms);
		} else if(argv[i][0] == '@'){
			// if it has a leading @ it goes in /etc
			++argv[i];
			m = etcperms;
			inum = ialloc(T_FILE);
//			printf("mkfs: adding file %s (inode %d)\n", argv[i], inum);
			bzero(&de, sizeof(de));
			de.inum = xshort(inum);
			strncpy(de.name, argv[i], DIRSIZ);
			iappendm(etcino, &de, sizeof(de), m);
			while((cc = read(fd, buf, sizeof(buf))) > 0)
				iappendm(inum, buf, cc, m);
		} else if((strcmp(argv[i], "kernel.bin") == 0)
					|| (strcmp(argv[i], "kernel.elf") == 0)
					|| (strcmp(argv[i], "bootbin") == 0)
					|| (strcmp(argv[i], "bootelf") == 0)){
			// this is a kernel image. doesn't even really need to show up
			// but we put it in /boot/kernel
			// the new bootloader goes in bootelf
			inum = ialloc(T_FILE);
//			printf("mkfs: adding file %s (inode %d)\n", argv[i], inum);
			bzero(&de, sizeof(de));
			de.inum = xshort(inum);
			if(strcmp(argv[i], "bootelf") == 0){
				sb.bootable = 1;
				sb.bootinode = xshort(inum);
			}
			if(strcmp(argv[i], "kernel.elf") == 0)
				sb.kerninode = xshort(inum);
			strncpy(de.name, argv[i], DIRSIZ);
			iappendm(bootino, &de, sizeof(de), sysperms);
			while((cc = read(fd, buf, sizeof(buf))) > 0)
				iappendm(inum, buf, cc, sysperms);
		} else {
			// everything else goes in /
			inum = ialloc(T_FILE);
//			printf("mkfs: adding file %s (inode %d)\n", argv[i], inum);
			bzero(&de, sizeof(de));
			de.inum = xshort(inum);
			strncpy(de.name, argv[i], DIRSIZ);
			iappend(rootino, &de, sizeof(de));
			while((cc = read(fd, buf, sizeof(buf))) > 0)
				iappend(inum, buf, cc);
		}
		close(fd);
	}
	printf("done\n");
	if(sb.bootable){
		printf("mkfs: fs bootable, bootloader is inode %u\n", sb.bootinode);
		memset(buf, 0, sizeof(buf));
		memmove(buf, &sb, sizeof(sb));
		wsect(1, buf);
	}
	if(sb.kerninode > 0) {
		printf("mkfs: kernel is inode %u\n", sb.kerninode);
		memset(buf, 0, sizeof(buf));
		memmove(buf, &sb, sizeof(sb));
		wsect(1, buf);
	}
	// fix size of root inode dir
	rinode(rootino, &din);
	off = xint(din.size);
	off = ((off/BSIZE) + 1) * BSIZE;
	din.size = xint(off);
//	printf("mkfs: fixing inode %d (%u bytes)\n", rootino, din.size);
	winode(rootino, &din);
	// fix size of bin inode dir
	rinode(binino, &din);
	off = xint(din.size);
	off = ((off/BSIZE) + 1) * BSIZE;
	din.size = xint(off);
//	printf("mkfs: fixing inode %d (%u bytes)\n", binino, din.size);
	winode(binino, &din);
	rinode(etcino, &din);
	off = xint(din.size);
	off = ((off/BSIZE) + 1) * BSIZE;
	din.size = xint(off);
//	printf("mkfs: fixing inode %d (%u bytes)\n", etcino, din.size);
	winode(etcino, &din);
	rinode(bootino, &din);
	off = xint(din.size);
	off = ((off/BSIZE) + 1) * BSIZE;
	din.size = xint(off);
	winode(bootino, &din);
	balloc(freeblock);
	exit(0);
}

void
wsect(uint sec, void *buf)
{
	if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
		perror("lseek");
		exit(1);
	}
	if(write(fsfd, buf, BSIZE) != BSIZE){
		perror("write");
		exit(1);
	}
}

void
winode(uint inum, struct dinode *ip)
{
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode*)buf) + (inum % IPB);
	*dip = *ip;
	wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode*)buf) + (inum % IPB);
	*ip = *dip;
}

void
rsect(uint sec, void *buf)
{
	if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
		perror("lseek");
		exit(1);
	}
	if(read(fsfd, buf, BSIZE) != BSIZE){
		perror("read");
		exit(1);
	}
}

uint
ialloc(ushort type)
{
	return iallocp(type, binperms); // r,w,x,h
}

uint
iallocp(ushort type, ushort perms)
{
	uint inum = freeinode++;
	struct dinode din;

	bzero(&din, sizeof(din));
	din.type = xshort(type);
	din.nlink = xshort(1);
	din.size = xint(0);
	din.owner = xshort(-1);
	din.perms = xshort(perms);
	winode(inum, &din);
	return inum;
}

void
balloc(int used)
{
	uchar buf[BSIZE];
	int i;

	printf("mkfs: first %d blocks have been allocated\n", used);
	assert(used < BSIZE*8);
	bzero(buf, BSIZE);
	for(i = 0; i < used; i++){
		buf[i/8] = buf[i/8] | (0x1 << (i%8));
	}
	printf("mkfs: write bitmap block at sector %d\n", sb.bmapstart);
	wsect(sb.bmapstart, buf);
}

/*
This new iappend reflects the changes to the fs, in that all addresses are
indirect now as opposed to most being direct and one being indirect.
Also note: I'm trying to make this look and feel as similar as possible to
the old iappend. Don't know how that will turn out.
*/

void
iappend(uint inum, void *xp, int n)
{
	iappendm(inum, xp, n, U_READ|W_READ|U_WRITE);
}

void
iappendm(uint inum, void *xp, int n, int mode)
{
	char *p = (char*)xp;
	struct dinode din;
	char buf[BSIZE];
	indir_ptr bptr;
	uint indirect[NINDIRECT];
	uint fbn, raddr, off, sboff, wrlen;

	rinode(inum, &din);
	off = xint(din.size);
	// you can just use the size as the offset because you want the next block
	// and the size *is* the next block if your array is indexed from zero. If
	// you had some stupid programming language that indexed by 1, this would
	// be off = xint(din.size)-1; or so.
	din.owner = -1;
	din.perms = mode;
	while(n > 0){
		// magic shit here
		sboff = fbn = 0;
		if(off%BSIZE){
			fbn = off/BSIZE;
			sboff = (off%BSIZE);
			wrlen = (n < (BSIZE-sboff) ? n : BSIZE - sboff);
		} else if(off == 0){
			fbn = 0;
			sboff = 0;
			wrlen = (n < (BSIZE-sboff) ? n : BSIZE - sboff);
		} else {
			fbn = off/BSIZE;
			sboff = 0;
			wrlen = ((n < BSIZE) ? n : BSIZE);
		}
		bptr = offset2real(fbn);
		if(xint(din.addrs[bptr.block]) == 0)
			din.addrs[bptr.block] = xint(freeblock++);
		rsect(xint(din.addrs[bptr.block]), (char*)indirect);
		if(indirect[bptr.ptr] == 0){
			indirect[bptr.ptr] = xint(freeblock++);
			wsect(xint(din.addrs[bptr.block]), (char*)indirect);
		}
		raddr = xint(indirect[bptr.ptr]);
//		printf("mkfs: writing at block %u+%u (addr %u) %u bytes\n", fbn, sboff, raddr, wrlen);
		rsect(raddr, buf);
		memcpy(buf+sboff, p, wrlen);
		wsect(raddr, buf);
		p += wrlen;
		n -= wrlen;
		off += wrlen;
	}
	din.size = xint(off);
	winode(inum, &din);
}

uint
makedir(uint parent, char *name)
{
	struct dirent de;
	uint meino = ialloc(T_DIR);

	bzero(&de, sizeof(de));
	de.inum = xshort(meino);
	strcpy(de.name, name);
	iappend(parent, &de, sizeof(de));
	bzero(&de, sizeof(de));
	de.inum = xshort(meino);
	strcpy(de.name, ".");
	iappend(meino, &de, sizeof(de));
	bzero(&de, sizeof(de));
	de.inum = xshort(parent);
	strcpy(de.name, "..");
	iappend(meino, &de, sizeof(de));
	return meino;
}
