//
// File descriptors
//

#include "types.h"
#include "errstr.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "file.h"
#include "spinlock.h"
#include "mmu.h"
#include "proc.h"
#include "users.h"

struct devsw devsw[NDEV];
struct {
	struct spinlock lock;
	struct file file[NFILE];
} ftable;

#define isowner (owner == proc->uid) 

int
checkfileperm(struct file *f, short op)
{
	int owner;
	short perms;
	static char *ops[] = {
	[OP_READ]	"OP_READ",
	[OP_WRITE]	"OP_WRITE",
	[OP_EXEC]	"OP_EXEC",
	[OP_OPEN]	"OP_OPEN",
	};

	owner = f->ip->owner;
	perms = f->ip->perms;

	// in single user mode system had godlike authority, and normal
	// users are unable to modify files. This is for safety reasons.
	// If the machine is put into single user mode while other users
	// are online, something bad almost certainly happened. Preventing
	// users from modifiying files in this state will hopefully help
	// help the system's admin.
	if(proc->uid == -1 && singleuser == 1)
		return 0;
	if(proc->uid != -1 && singleuser == 1)
		return -1;
	switch(op){
	case OP_READ:
		if(isowner)
			if(perms&U_READ)
				return 0;
		if(perms&W_READ)
			return 0;
		break;
	case OP_WRITE:
		if(isowner)
			if(perms&U_WRITE)
				return 0;
		if(perms&W_WRITE)
			return 0;
		break;
	case OP_EXEC:
		if(isowner)
			if((perms&U_READ) && (perms&U_EXEC))
				return 0;
		if((perms&W_READ) && (perms&W_EXEC))
			return 0;
		break;
	case OP_OPEN:
		if(owner)
			if((perms&U_READ) || (perms&U_WRITE))
				return 0;
		if((perms&W_READ) || (perms&W_WRITE))
			return 0;
		break;
	case OP_UNLINK:
		if(owner)
		if((perms&U_READ) && (perms&U_WRITE))
			return 0;
		if((perms&W_READ) && (perms&W_WRITE))
			return 0;
	}
	return 1;
}

int
checkinodeperm(struct inode *ip, short op)
{
	struct file f;

	f.ip = ip;
	return checkfileperm(&f, op);
}

void
fileinit(void)
{
	initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
	struct file *f;

	acquire(&ftable.lock);
	for(f = ftable.file; f < ftable.file + NFILE; f++){
		if(f->ref == 0){
			f->ref = 1;
			release(&ftable.lock);
			return f;
		}
	}
	release(&ftable.lock);
	return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
	acquire(&ftable.lock);
	if(f->ref < 1)
		panic("filedup");
	f->ref++;
	release(&ftable.lock);
	return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
	struct file ff;

	acquire(&ftable.lock);
	if(f->ref < 1)
		panic("fileclose");
	if(--f->ref > 0){
		release(&ftable.lock);
		return;
	}
	ff = *f;
	f->ref = 0;
	f->type = FD_NONE;
	release(&ftable.lock);
	
	if(ff.type == FD_PIPE)
		pipeclose(ff.pipe, ff.writable);
	else if(ff.type == FD_INODE){
		begin_op();
		iput(ff.ip);
		end_op();
	}
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
	if(f->type == FD_INODE){
		if(checkfileperm(f, OP_OPEN)){
			seterr(ESNOPERMS);
			return -1;
		}
		ilock(f->ip);
		stati(f->ip, st);
		iunlock(f->ip);
		return 0;
	}
	return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
	int r;

	if(!f->readable){
		seterr(EIUNREAD);
		return -1;
	}

	if(f->type == FD_PIPE)
		return piperead(f->pipe, addr, n);
	if(f->type == FD_INODE){
		ilock(f->ip);
		if((r = readi(f->ip, addr, f->off, n)) > 0)
			f->off += r;
		iunlock(f->ip);
		return r;
	}
	seterr(EIFUNKNOWN);
	return -1;
}

// seek in file f
int
fileseek(struct file *f, int n, int dir)
{
	if(!f->readable && !f->writable){
		seterr(EIUNREAD);
		return -1;
	}
	if(f->type == FD_PIPE){
		seterr(EIBADTYPE);
		return -1;
	}
	if(f->type == FD_INODE){
		switch(dir){
		case 0:
			f->off = n;
			break;
		case 1:
			f->off += n;
			break;
		case 2:
			f->off = n + f->ip->size;
			break;
		}
		return f->off;
	}
	seterr(EIFUNKNOWN);
	return -1;
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
	int r;

	if(f->writable == 0){
		seterr(EIUNWRITE);
		return -1;
	}
	if(f->type == FD_PIPE)
		return pipewrite(f->pipe, addr, n);
/*	if(checkfileperm(f, OP_WRITE)){
		seterr(ESNOPERMS);
		return -1;
	} */
	if(f->type == FD_INODE){
		// write a few blocks at a time to avoid exceeding
		// the maximum log transaction size, including
		// i-node, indirect block, allocation blocks,
		// and 2 blocks of slop for non-aligned writes.
		// this really belongs lower down, since writei()
		// might be writing a device like the console.
		int max = ((LOGSIZE-1-1-2) / 2) * 512;
		int i = 0;
		while(i < n){
			int n1 = n - i;
			if(n1 > max)
				n1 = max;

			begin_op();
			ilock(f->ip);
			if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
				f->off += r;
			iunlock(f->ip);
			end_op();

			if(r < 0)
				break;
			if(r != n1){
				seterr(EISHORTWRITE);
				return -1;
			}
			i += r;
		}
		return i == n ? n : -1;
	}
	seterr(EIFUNKNOWN);
	return -1;
}

