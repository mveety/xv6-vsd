// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation 
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);
static int bmap_g(struct inode*, uint, int);
void imount0(struct inode*, struct inode*, struct inode*);
// struct superblock sb;   // there should be one per dev, but we run with one dev
int havesysfs = 0;
struct inode *rootdir;

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
	struct buf *bp;
	
	bp = bread(dev, 1);
	memmove(sb, bp->data, sizeof(struct superblock));
	brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
	struct buf *bp;
	
	bp = bread(dev, bno);
	memset(bp->data, 0, BSIZE);
	log_write(bp);
	brelse(bp);
}

// Blocks. 

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
	int b, bi, m;
	struct buf *bp;
	struct superblock *sb;

	sb = getsuperblock(dev);
	bp = 0;
	for(b = 0; b < sb->size; b += BPB){
		bp = bread(dev, BBLOCK(b, sb));
		for(bi = 0; bi < BPB && b + bi < sb->size; bi++){
			m = 1 << (bi % 8);
			if((bp->data[bi/8] & m) == 0){  // Is block free?
				bp->data[bi/8] |= m;  // Mark block in use.
				log_write(bp);
				brelse(bp);
				bzero(dev, b + bi);
				return b + bi;
			}
		}
		brelse(bp);
	}
	cprintf("error: fs: filesystem full\n");
	return 0;
}

// Free a disk block.
static void
bfree(int dev, uint b)
{
	struct buf *bp;
	int bi, m;
	struct superblock *sb;

	sb = getsuperblock(dev);
	bp = bread(dev, BBLOCK(b, sb));
	bi = b % BPB;
	m = 1 << (bi % 8);
	if((bp->data[bi/8] & m) == 0)
		cprintf("error: fs: freeing free block");
	bp->data[bi/8] &= ~m;
	log_write(bp);
	brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->flags.
//
// An inode and its in-memory represtative go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, iput() frees if
//   the link count has fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() to find or
//   create a cache entry and increment its ref, iput()
//   to decrement ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when the I_VALID bit
//   is set in ip->flags. ilock() reads the inode from
//   the disk and sets I_VALID, while iput() clears
//   I_VALID if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode. The I_BUSY flag indicates
//   that the inode is locked. ilock() sets I_BUSY,
//   while iunlock clears it.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.

struct {
	struct spinlock lock;
	struct inode inode[NINODE];
} icache;

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

void
fsinit(int dev)
{
	struct superblock sb;

	readsb(dev, &sb);
	diskmount0(dev, &sb);
	iinit(dev);
	initlog(dev);
	rootdir = iget(dev, ROOTINO);
	imount0(rootdir, rootdir, rootdir);
	diskmount1(dev, rootdir);
}

void
iinit(int dev)
{
	struct inode *rootinode;
	struct superblock sb;

	initlock(&icache.lock, "icache");
	readsb(dev, &sb);
	cprintf("fs: disk%d: using vsd filesystem version %d\n", dev, sb.version);
	cprintf("fs: disk%d: sb: size %d nblocks %d ninodes %d nlog %d logstart %d\nfs: disk%d: sb: inodestart %d bmap start %d", dev, sb.size, sb.nblocks, sb.ninodes, sb.nlog, sb.logstart, dev, sb.inodestart, sb.bmapstart);
	if(sb.bootable && sb.bootinode > 0)
		cprintf(" boot %u", sb.bootinode);
	if(sb.kerninode > 0)
		cprintf(" kernel %u", sb.kerninode);
	if(sb.system)
		cprintf(" system");
	cprintf("\n");

	cprintf("fs: disk%d: mounting volume %s, version %u\n", dev, sb.label, sb.version);
	if(!sb.system && !havesysfs)
		panic("given disk is not a system disk");
	havesysfs = 1;
}

//PAGEBREAK!
// Allocate a new inode with the given type on device dev.
// A free inode has a type of zero.
struct inode*
ialloc(uint dev, short type)
{
	int inum;
	struct buf *bp;
	struct dinode *dip;
	struct superblock *sb;

	sb = getsuperblock(dev);
	for(inum = 1; inum < sb->ninodes; inum++){
		bp = bread(dev, IBLOCK(inum, sb));
		dip = (struct dinode*)bp->data + inum%IPB;
		if(dip->type == 0){  // a free inode
			memset(dip, 0, sizeof(*dip));
			dip->type = type;
			log_write(bp);   // mark it allocated on the disk
			brelse(bp);
			return iget(dev, inum);
		}
		brelse(bp);
	}
	cprintf("error: fs: no more inodes\n");
	return nil;
}

// Copy a modified in-memory inode to disk.
void
iupdate1(struct inode *ip, int direct)
{
	struct buf *bp;
	struct dinode *dip;
	struct superblock *sb;

	sb = getsuperblock(ip->dev);
	bp = bread(ip->dev, IBLOCK(ip->inum, sb));
	dip = (struct dinode*)bp->data + ip->inum%IPB;
	dip->type = ip->type;
	dip->major = ip->major;
	dip->minor = ip->minor;
	dip->nlink = ip->nlink;
	dip->owner = ip->owner;
	dip->perms = ip->perms;
	dip->size = ip->size;
	memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
	if(direct)
		bwrite(bp);
	else
		log_write(bp);
	brelse(bp);
}

void
iupdate(struct inode *ip)
{
	iupdate1(ip, 0);
}

void
iupdate_direct(struct inode *ip)
{
	iupdate1(ip, 1);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
struct inode*
iget(uint dev, uint inum)
{
	struct inode *ip, *empty;

	acquire(&icache.lock);

	// Is the inode already cached?
	empty = 0;
	for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
		if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
			if(ip->flags & I_MOUNTPOINT){
//				cprintf("fs: dev%d(%d) is a mount point. returning dev%d(%d)\n",
//						ip->dev, ip->inum, ip->mountroot->dev, ip->mountroot->inum);
				ip = ip->mountroot;
			}
			ip->ref++;
			release(&icache.lock);
			return ip;
		}
		if(empty == 0 && ip->ref == 0)    // Remember empty slot.
			empty = ip;
	}

	// Recycle an inode cache entry.
	if(empty == 0)
		panic("iget: no inodes");

	ip = empty;
	ip->dev = dev;
	ip->inum = inum;
	ip->ref = 1;
	ip->flags = 0;
	release(&icache.lock);

	return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
	if(ip != nil){
		acquire(&icache.lock);
		ip->ref++;
		release(&icache.lock);
	}
	return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
	struct buf *bp;
	struct dinode *dip;
	struct superblock *sb;

	if(ip == 0 || ip->ref < 1){
		cprintf("error: fs: inode &%p has negative refs or does not exist.\n", ip);
		return;
	}

	sb = getsuperblock(ip->dev);
	acquire(&icache.lock);
	while(ip->flags & I_BUSY)
		sleep(ip, &icache.lock);
	ip->flags |= I_BUSY;
	release(&icache.lock);

	if(!(ip->flags & I_VALID)){
		bp = bread(ip->dev, IBLOCK(ip->inum, sb));
		dip = (struct dinode*)bp->data + ip->inum%IPB;
		ip->type = dip->type;
		ip->major = dip->major;
		ip->minor = dip->minor;
		ip->owner = dip->owner;
		ip->perms = dip->perms;
		ip->nlink = dip->nlink;
		ip->size = dip->size;
		memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
		brelse(bp);
		ip->flags |= I_VALID;
		if(ip->type == 0)
			cprintf("error: fs: cannot lock empty inode.\n");
	}
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
	if(ip == 0 || ip->ref < 1){
		cprintf("error: fs: inode &%p has negative refs or does not exist. run fsck.\n", ip);
		return;
	}
	if(!(ip->flags & I_BUSY)){
		cprintf("error: fs: inode dev%d(%d): trying to unlock unlocked inode\n", ip->dev, ip->inum);
		return;
	}
	acquire(&icache.lock);
	ip->flags &= ~I_BUSY;
	wakeup(ip);
	release(&icache.lock);
}

void
iref(struct inode *ip)
{
	if(ip == nil){
		cprintf("error: fs: unable to increase refcount on nil inode\n");
		return;
	}
	acquire(&icache.lock);
	ip->ref++;
	release(&icache.lock);
}

void
iunref(struct inode *ip)
{
	if(ip == 0){
		cprintf("error: fs: unable to increase refcount on nil inode\n", ip);
		return;
	}
	if(ip->ref < 1){
		cprintf("error: fs: inode dev%d(%d) has negative refs\n", ip->dev, ip->inum);
		return;
	}
	if(ip->ref == 0){
		cprintf("error: fs: inode dev%d(%d) is already not referenced\n", ip->dev, ip->inum);
		return;
	}
	acquire(&icache.lock);
	ip->ref--;
	release(&icache.lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
iput1(struct inode *ip, int direct)
{
	acquire(&icache.lock);
	if(ip->ref == 1 && (ip->flags & I_VALID) && ip->nlink == 0){
		// inode has no links and no other references: truncate and free.
		if(ip->flags & I_BUSY)
			panic("iput busy");
		ip->flags |= I_BUSY;
		release(&icache.lock);
		itrunc(ip);
		ip->type = 0;
		iupdate1(ip, direct);
		acquire(&icache.lock);
		ip->flags = 0;
		wakeup(ip);
	}
	ip->ref--;
	release(&icache.lock);
}

void
iput(struct inode *ip)
{
	iput1(ip, 0);
}

void
iput_direct(struct inode *ip)
{
	iput1(ip, 1);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
	iunlock(ip);
	iput(ip);
}

void
iunlockput_direct(struct inode *ip)
{
	iunlock(ip);
	iput_direct(ip);
}

void
imount0(struct inode *mroot, struct inode *mpoint, struct inode *parent)
{
	ilock(mroot);
	if(parent != mroot)
		ilock(parent);
	if(mpoint != mroot)
		ilock(mpoint);

	// increment the reference count on these inodes so they don't get freed from
	// the cache
	iref(parent);
	iref(mpoint);
	iref(mroot);

	mpoint->flags |= I_MOUNTPOINT;
	mroot->flags |= I_MOUNTROOT;

	mpoint->mountroot = mroot;
	mroot->mountparent = parent;

	iunlock(mroot);
	if(parent != mroot)
		iunlock(parent);
	if(mpoint != mroot)
		iunlock(mpoint);
}

int
imount(struct inode *mroot, struct inode *mpoint)
{
	struct inode *parent;
	uint poff; // unused

	if(mpoint->type != T_DIR){
		cprintf("error: fs: tried to mount disk%d(%d) on non-direct disk%d(%d)\n",
				mroot->dev, mroot->inum, mpoint->dev, mpoint->inum);
		return -1;
	}
	parent = dirlookup(mpoint, "..", &poff);
	if(!parent){
		cprintf("error: fs: dev%d(%d) has no parent!\n", mpoint->dev, mpoint->inum);
		return -1;
	}
	cprintf("cpu%d: fs: mount dev%d(%d) to dev%d(%d)\n", cpu->id, mroot->dev, mroot->inum,
			mpoint->dev, mpoint->inum);
	imount0(mroot, mpoint, parent);
	return 0;
}

void
iunmount(struct inode *mroot)
{
	struct inode *parent;
	struct inode *mpoint;

	parent = mroot->mountparent;
	mpoint = mroot->mountpoint;

	ilock(mroot);
	ilock(mpoint);
	ilock(parent);

	iunref(mroot);
	iunref(mpoint);
	iunref(parent);

	mpoint->flags &= ~I_MOUNTPOINT;
	mroot->flags &= ~I_MOUNTROOT;

	iunlock(mroot);
	iunlock(mpoint);
	iunlock(parent);
}

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.

static int
bmap(struct inode *ip, uint bn)
{
	return bmap_g(ip, bn, 1);
}

static int
bmap_g(struct inode *ip, uint bn, int create)
{
	indir_ptr bptr;
	uint addr;
	uint *id_block;
	uint newblock;
	struct buf *bp;

	if(bn >= MAXFILE)
		panic("newbmap: block out of range");
	bptr = offset2real(bn);
	if(ip->addrs[bptr.block] == 0 && create){
		if((newblock = balloc(ip->dev)) == 0)
			return -1;
		ip->addrs[bptr.block] = newblock;
	}else if(!create)
		return -1;
	bp = bread(ip->dev, ip->addrs[bptr.block]);
	id_block = (uint*)bp->data;
	if(id_block[bptr.ptr] == 0 && create){
		if((newblock = balloc(ip->dev)) == 0)
			return -1;
		id_block[bptr.ptr] = newblock;
		log_write(bp);
	} else if(!create)
		return -1;
	addr = id_block[bptr.ptr];
	brelse(bp);
	return addr;
}

static void
bfreeref(struct inode *ip, uint addr)
{
	indir_ptr bptr;
	uint *id_block;
	struct buf *bp;

	bptr = offset2real(addr);
	if(ip->addrs[bptr.block] != 0){
		bp = bread(ip->dev, ip->addrs[bptr.block]);
		id_block = (uint *)bp->data;
		id_block[bptr.ptr] = 0;
		log_write(bp);
		brelse(bp);
	}
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).

static void
itrunc(struct inode *ip)
{
	int i;
	struct buf *bp;
	uint addr;
	uint tblks;

	tblks = ip->size / BSIZE;
	for(i = 0; i < tblks; i++){
		addr = bmap_g(ip, i, 0);
		if(addr == -1)
			continue;
		bfree(ip->dev, addr);
		bfreeref(ip, addr);
	}
	ip->size = 0;
	iupdate(ip);
}

// Copy stat information from inode.
// TODO: update stat struct
void
stati(struct inode *ip, struct stat *st)
{
	st->dev = ip->dev;
	st->ino = ip->inum;
	st->type = ip->type;
	st->nlink = ip->nlink;
	st->size = ip->size;
	st->owner = ip->owner;
	st->perms = ip->perms;
}

//PAGEBREAK!
// Read data from inode.
int
readi(struct inode *ip, char *dst, uint off, uint n)
{
	uint tot, m, addr;
	struct buf *bp;

	if(ip->type == T_DEV){
		if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
			return -1;
		return devsw[ip->major].read(ip, dst, n, off);
	}

	if(off > ip->size || off + n < off)
		return -1;
	if(off + n > ip->size)
		n = ip->size - off;
//	cprintf("fs: inode %u is %u bytes in size\n", ip->inum, ip->size);
//	cprintf("fs: reading %u bytes at offset %u from inode %d\n", n, off, ip->inum);
// TODO: make disks use the devsw shit
	for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
		addr = bmap(ip, off/BSIZE);
//		cprintf("fs: getting block %u from inode %u\n", addr, ip->inum);
		bp = bread(ip->dev, addr);
		m = min(n - tot, BSIZE - off%BSIZE);
		memmove(dst, bp->data + off%BSIZE, m);
		brelse(bp);
	}
	return n;
}

// PAGEBREAK!
// Write data to inode.
int
writei(struct inode *ip, char *src, uint off, uint n)
{
	uint tot, m;
	int bnum;
	struct buf *bp;

	if(ip->type == T_DEV){
		if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
			return -1;
		return devsw[ip->major].write(ip, src, n, off);
	}

	if(off > ip->size || off + n < off)
		return -1;
	if(off + n > MAXFILE*BSIZE)
		return -1;
// TODO: make disks use the devsw shit
	for(tot=0; tot<n; tot+=m, off+=m, src+=m){
		// if bmap returns < 0 means something bad happened
		if((bnum = bmap(ip, off/BSIZE)) < 0){
			n = -2;
			break;
		}
		bp = bread(ip->dev, bnum);
		m = min(n - tot, BSIZE - off%BSIZE);
		memmove(bp->data + off%BSIZE, src, m);
		log_write(bp);
		brelse(bp);
	}

	if((n > 0 || n == -2) && off > ip->size){
		ip->size = off;
		iupdate(ip);
	}
	return n;
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
	return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
	uint off, inum;
	struct dirent de;

	if(dp->type != T_DIR)
		panic("dirlookup not DIR");
//	cprintf("fs: looking for %s in inode dev%d(%d)\n", name, dp->dev, dp->inum);
	for(off = 0; off < dp->size; off += sizeof(de)){
		if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlink read");
		if(de.inum == 0)
			continue;
//		cprintf("fs: dirlookup: found entry %s (%u)\n", de.name, de.inum);
		if(namecmp(name, de.name) == 0){
			// entry matches path element
			if(poff)
				*poff = off;
			if((namecmp("..", de.name) == 0) && (dp->flags & I_MOUNTROOT)){
				cprintf("fs: dev%d(%d) is a mounted root. returning dev%d(%d) for ..\n",
						dp->dev, dp->inum, dp->mountparent->dev,
						dp->mountparent->inum);
				return iget(dp->mountparent->dev, dp->mountparent->inum);
			}
			inum = de.inum;
			return iget(dp->dev, inum);
		}
	}

	return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(struct inode *dp, char *name, uint inum)
{
	int off;
	struct dirent de;
	struct inode *ip;

	// Check that name is not present.
	if((ip = dirlookup(dp, name, 0)) != 0){
		iput(ip);
		return -1;
	}

	// Look for an empty dirent.
	for(off = 0; off < dp->size; off += sizeof(de)){
		if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlink read");
		if(de.inum == 0)
			break;
	}

	strncpy(de.name, name, DIRSIZ);
	de.inum = inum;
	if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
		panic("dirlink");
	
	return 0;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
	char *s;
	int len;

	while(*path == '/')
		path++;
	if(*path == 0)
		return 0;
	s = path;
	while(*path != '/' && *path != 0)
		path++;
	len = path - s;
	if(len >= DIRSIZ)
		memmove(name, s, DIRSIZ);
	else {
		memmove(name, s, len);
		name[len] = 0;
	}
	while(*path == '/')
		path++;
	return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name, int direct)
{
	struct inode *ip, *next;

	if(proc != nil) {
		if(*path == '/'){
			if(proc->rootdir == nil)
					ip = idup(rootdir);
			else
				ip = idup(proc->rootdir);
		} else
			ip = idup(proc->cwd);
	} else
		ip = idup(rootdir);
	while((path = skipelem(path, name)) != 0){
		ilock(ip);
		if(ip->type != T_DIR){
			if(direct)
				iunlockput_direct(ip);
			else
				iunlockput(ip);
			return 0;
		}
		if(nameiparent && *path == '\0'){
			// Stop one level early.
			iunlock(ip);
			return ip;
		}
		if((next = dirlookup(ip, name, 0)) == 0){
			if(direct)
				iunlockput_direct(ip);
			else
				iunlockput(ip);
			return 0;
		}
		if(direct)
			iunlockput_direct(ip);
		else
			iunlockput(ip);
		ip = next;
	}
	if(nameiparent){
		if(direct)
			iput_direct(ip);
		else
			iput(ip);
		return 0;
	}
	return ip;
}

struct inode*
namei(char *path)
{
	char name[DIRSIZ];
	return namex(path, 0, name, 0);
}

struct inode*
nameiparent(char *path, char *name)
{
	return namex(path, 1, name, 0);
}

struct inode*
namei_direct(char *path)
{
	char name[DIRSIZ];
	return namex(path, 0, name, 1);
}

struct inode*
nameiparent_direct(char *path, char *name)
{
	return namex(path, 1, name, 1);
}
