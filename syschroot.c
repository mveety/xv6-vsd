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

// takes path and returns rootdir+path
// basically for converting between chroot paths and
// actual paths.
char*
vroot2root(char *opath)
{
	int rdirlen;
	// this will be a security problem in the future
	char pathbuf[256];
	char *pathbuf_ptr;
	char *path = nil;

	if(proc->rootdir == nil)
		return strdup(path);
	path = opath;
	pathbuf_ptr = pathbuf;
	rdirlen = strlen(proc->rdirpath);
	safestrcpy(pathbuf, proc->rdirpath, 256);
	if(*path == '/')
		path++;
	pathbuf_ptr += rdirlen;
	safestrcpy(pathbuf_ptr, path, 256);
	return strdup(pathbuf);
}

int
chroot(char *path)
{
	struct inode *rp;
	int pathlen;
	char pathbf[128];

	if(proc->injail != 0)
		return -1;
	if(proc->uid != -1)
		return -1;
	begin_op();
	rp = namei(path);
	if(rp == 0){
		return -1;
	} else if(rp->inum == rootdir->inum){
		proc->rootdir = nil;
		return 0;
	}
	if(path[0] != '/')
		return -1;
	ilock(rp);
	pathlen = strlen(path) + 1;
	if(pathlen >= 122){
		iunlockput(rp);
		end_op();
		return -1;
	}
	memcpy(pathbf, path, pathlen);
	if(pathbf[pathlen-2] != '/'){
		pathbf[pathlen-1] = '/';
		pathbf[pathlen] = '\0';
	}
	if(rp->type != T_DIR){
		iunlockput(rp);
		end_op();
		return -1;
	}
	safestrcpy(proc->rdirpath, pathbf, 128);
	if(proc->rootdir != nil)
		iput(proc->rootdir);
	proc->rootdir = idup(rp);
	proc->cwd = proc->rootdir;
	proc->injail = 1;
	return 0;
}
