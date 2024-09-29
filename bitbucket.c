// bitbucket.c -- implements /dev/null, /dev/zero, and /dev/robpike

#include "types.h"
#include "defs.h"
#include "param.h"
#include "errstr.h"
#include "traps.h"
#include "spinlock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

struct spinlock null_lock;
struct spinlock zero_lock;
struct spinlock rob_lock;
int robcounter;

int bitbucket_read(struct inode*, char*, int, int);
int bitbucket_write(struct inode*, char*, int, int);

// writes always succeed and no data can be read
int nullrw(int, char*, int);
// writes always succeed and a stream of nil is read
int zerorw(int, char*, int);
// no.
int robrw(int, char*, int);

void
bitbucket_init(void)
{
	cprintf("cpu%d: system: starting bitbucket\n", cpu->id);
	initlock(&null_lock, "null dev");
	initlock(&zero_lock, "zero dev");
	initlock(&rob_lock, "rob pike's lock");

	robcounter = 0;
	devsw[2].write = &bitbucket_write;
	devsw[2].read = &bitbucket_read;
};

int
bitbucket_read(struct inode *ip, char *bf, int nbytes, int off)
{
	switch(ip->minor){
	case 0:
		return nullrw(0, bf, nbytes);
	case 1:
		return zerorw(0, bf, nbytes);
	case 2:
		return robrw(0, bf, nbytes);
	default:
		seterr(EDNOTEXIST);
		return -1;
	}
}

int
bitbucket_write(struct inode *ip, char *bf, int nbytes, int off)
{
	switch(ip->minor){
	case 0:
		return nullrw(1, bf, nbytes);
	case 1:
		return zerorw(1, bf, nbytes);
	case 2:
		return robrw(1, bf, nbytes);
	default:
		seterr(EDNOTEXIST);
		return -1;
	}
}

int
nullrw(int dir, char *bf, int nbytes)
{
	Lock(null_lock);
	if(dir == 0){
		seterr(EDNOREAD);
		Unlock(null_lock);
		return -1;
	} else if (dir == 1) {
		Unlock(null_lock);
		return nbytes;
	}
	Unlock(null_lock);
	return 0;
}

int
zerorw(int dir, char *bf, int nbytes)
{
	char *rbf;

	Lock(zero_lock);
	rbf = bf;
	if(dir == 0){
		for(int i = 0; i < nbytes; i++)
			rbf[i] = '\0';
		Unlock(zero_lock);
		return nbytes;
	} else if(dir == 1) {
		Unlock(zero_lock);
		return nbytes;
	}
	return -1;
}

int
robrw(int dir, char *bf, int nbytes)
{
	char quote[] = "I'd spell creat with an e.\n";
	int readlen;

	Lock(rob_lock);
	if(robcounter >= sizeof(quote))
		robcounter = 0;
	if(dir == 0){
		readlen = nbytes <= (sizeof(quote) - robcounter) ? nbytes : sizeof(quote) - robcounter;
		memcpy(bf, quote+robcounter, readlen);
		robcounter += readlen;
		Unlock(rob_lock);
		return readlen;
	} else if(dir == 1) {
		seterr(EDFUCKYOU);
		Unlock(rob_lock);
		return 0;
	}
	return -1;
}
