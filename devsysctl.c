// sysctl.c -- modify system parameters and status

#include "types.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "errstr.h"
#include "traps.h"
#include "spinlock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "kfields.h"
#include "users.h"

struct spinlock sysctl_lock;
struct spinlock sysname_lock;
struct spinlock systime_lock;

// some of the sysctls (other than sysuart and singleuser)
int halted = 0;
char sysname[256];
int sysnamelen = 0;

int allowthreads = 1;
int useclone = 1;
int msgnote = 0;

struct rtcdate boottime;

int sysctl_write(struct inode*, char*, int, int);
int sysctl_read(struct inode*, char*, int, int);
int sysname_read(struct inode*, char*, int, int);
int sysname_write(struct inode*, char*, int, int);
int systime_read(struct inode*, char*, int, int);
int systime_write(struct inode*, char*, int, int);

int startdrv(char*);
void reboot(void);
void shutdown(int);

void
sysctl_init(void)
{
	cprintf("cpu%d: system: starting sysctl\n", cpu->id);
	initlock(&sysctl_lock, "sysctl lock");
	devsw[3].write = &sysctl_write;
	devsw[3].read = &sysctl_read;
}

void
sysname_init(void)
{
	cprintf("cpu%d: system: setting sysname\n", cpu->id);
	initlock(&sysname_lock, "sysname lock");
	safestrcpy(sysname, "amnesiac", 255);
	devsw[5].write = &sysname_write;
	devsw[5].read = &sysname_read;
	cmostime(&boottime);
}

void
systime_init(void)
{
	cprintf("cpu%d: system: starting clock\n", cpu->id);
	initlock(&systime_lock, "systime lock");
	devsw[8].write = &systime_write;
	devsw[8].read = &systime_read;
}

#define scmsgsz 15
char *scmsg[] = {
	[0]  "sysuart 1     \n",
	[1]  "sysuart 0     \n",
	[2]  "singleuser 1  \n",
	[3]  "singleuser 0  \n",
	[4]  "allowthreads 1\n",
	[5]  "allowthreads 0\n",
	[6]  "useclone 1    \n",
	[7]  "useclone 0    \n",
	[8]  "msgnote 1     \n",
	[9]  "msgnote 0     \n",
	[10] "syscons 1     \n",
	[11] "syscons 0     \n",
};

int
diskctl_read(struct inode *ip, char *bf, int nbytes, int off)
{
	return 0;
}

int
sysctl_read(struct inode* ip, char* bf, int nbytes, int off)
{
	int scmsz = scmsgsz*6;
	char buf[scmsz+5];
	int rnbytes;

	switch(ip->minor){
	case 0: // sysctl
		if(off >= scmsz)
			return 0;
		if(nbytes >= scmsz)
			rnbytes = scmsz;
		else
			rnbytes = nbytes;
		memset(buf, 0, sizeof(buf));
		memmove(&buf[0], sysuart ? scmsg[0] : scmsg[1], scmsgsz);
		memmove(&buf[15], syscons ? scmsg[10] : scmsg[11], scmsgsz);
		memmove(&buf[30], singleuser ? scmsg[2] : scmsg[3], scmsgsz);
		memmove(&buf[45], allowthreads ? scmsg[4] : scmsg[5], scmsgsz);
		memmove(&buf[60], useclone ? scmsg[6] : scmsg[7], scmsgsz);
		memmove(&buf[75], msgnote ? scmsg[8] : scmsg[9], scmsgsz);
		memmove(bf, &buf[0], rnbytes);
		return rnbytes;
	case 1: // diskctl
		return diskctl_read(ip, bf, nbytes, off);
	}
	return 0;
}

int
sysctl_write(struct inode *ip, char *bf, int nbytes, int off)
{
	char buf[256];
	int i = 0;
	char *rbf;
	int st = 0;

	if(ip->minor != 0)
		return 0;
	memset(buf, 0 ,256);
	rbf = bf;
	if(nbytes <= 0){
		seterr(EDNOWRITE);
		return -1;
	}
	// security.
	if(proc->uid >= 0 || proc->sid >= 0){
		seterr(EDFUCKYOU);
		return -1;
	}
	for(i = 0; i < nbytes;){
		rbf = getnextfield(buf, rbf, &i, &st);
		if(st)
			break;
		if(strcmp(buf, "reboot") == 0)
			reboot();
		else if(strcmp(buf, "halt") == 0)
			shutdown(0);
		else if(strcmp(buf, "nosysuart") == 0){
			cprintf("cpu%d: sysuart = no\n", cpu->id);
			sysuart = 0;
			break;
		} else if(strcmp(buf, "sysuart") == 0){
			rbf = getnextfield(buf, rbf, &i, &st);
			if(st){
				cprintf("cpu%d: sysuart = yes\n", cpu->id);
				sysuart = 1;
			}
			if(strcmp(buf, "yes") == 0){
				sysuart = 1;
				cprintf("cpu%d: sysuart = yes\n", cpu->id);
			} else if(strcmp(buf, "no") == 0){
				sysuart = 0;
				cprintf("cpu%d: sysuart = no\n", cpu->id);
			}
			break;
		} else if(strcmp(buf, "syscons") == 0){
			rbf = getnextfield(buf, rbf, &i, &st);
			if(st){
				cprintf("cpu%d: syscons = yes\n", cpu->id);
				syscons = 1;
			}
			if(strcmp(buf, "yes") == 0){
				syscons = 1;
				cprintf("cpu%d: syscons = yes\n", cpu->id);
			} else if(strcmp(buf, "no") == 0){
				syscons = 0;
				cprintf("cpu%d: syscons = no\n", cpu->id);
			}
			break;
		} else if(strcmp(buf, "singleuser") == 0){
			rbf = getnextfield(buf, rbf, &i, &st);
			if(st)
				break;
			if(strcmp(buf, "yes") == 0){
				singleuser = 1;
				cprintf("cpu%d: singleuser = yes\n", cpu->id);
			} else if(strcmp(buf, "no") == 0){
				singleuser = 0;
				cprintf("cpu%d: singleuser = no\n", cpu->id);
			}
			break;
		} else if(strcmp(buf, "allowthreads") == 0){
			rbf = getnextfield(buf, rbf, &i, &st);
			if(st)
				break;
			if(strcmp(buf, "yes") == 0){
				allowthreads = 1;
				cprintf("cpu%d: allowthreads = yes\n", cpu->id);
			} else if(strcmp(buf, "no") == 0){
				allowthreads = 0;
				cprintf("cpu%d: allowthreads = no\n", cpu->id);
			}
			break;
		} else if(strcmp(buf, "useclone") == 0){
			rbf = getnextfield(buf, rbf, &i, &st);
			if(st)
				break;
			if(strcmp(buf, "yes") == 0){
				useclone = 1;
				cprintf("cpu%d: useclone = yes\n", cpu->id);
			} else if(strcmp(buf, "no") == 0){
				useclone = 0;
				cprintf("cpu%d: useclone = no\n", cpu->id);
			}
			break;
		} else if(strcmp(buf, "startdrv") == 0){
			rbf = getnextfield(buf, rbf, &i, &st);
			if(st)
				break;
			if(!startdrv(buf)){
				seterr(EDSTARTFAIL);
				return -1;
			}
			break;
		} else if(strcmp(buf, "msgnote") == 0){
			rbf = getnextfield(buf, rbf, &i, &st);
			if(st)
				break;
			if(strcmp(buf, "yes") == 0){
				msgnote = 1;
				cprintf("cpu%d: msgnote = yes\n", cpu->id);
			} else if(strcmp(buf, "no") == 0){
				msgnote = 0;
				cprintf("cpu%d: msgnote = no\n", cpu->id);
			}
			break;
		} else {
			seterr(EDBADCMD);
			return -1;
		}
	}
	return i;
}

int
sysname_read(struct inode *ip, char *bf, int nbytes, int off)
{
	char *rbf = bf;
	int i = 0;
	if(sysnamelen == 0)
		sysnamelen = strlen(sysname);
	if(off >= 256 || off >= sysnamelen)
		return 0;
	while(sysname[i] != '\0' && i < nbytes){
		rbf[i] = sysname[i];
		i++;
	}
	return i;
}

int
sysname_write(struct inode *ip, char *bf, int nbytes, int off)
{
	int i = 0;
	char *rbf = bf;

	if(nbytes <= 0){
		seterr(EDNOWRITE);
		return -1;
	}
	if(proc->pid >= 0 || proc->sid >= 0){
		seterr(EDFUCKYOU);
		return -1;
	}

	while(rbf[i] != '\0' && i < nbytes){
		sysname[i] = rbf[i];
		i++;
	}
	sysnamelen = i;
	return i;
}

int
startdrv(char *bf)
{
	// whatever. don't need it yet
	cprintf("cpu%d: starting %s\n", bf);
	return 1;
}

void
reboot(void)
{
	cli();
	sysuart = 1;
	syscons = 1;
	cprintf("cpu%d: rebooting...\n", cpu->id);
	u8int good = 0x02;
	while(good & 0x02)
		good = inb(0x64);
	outb(0x64, 0xfe);
	for(;;)
		hlt();
}

void
shutdown(int op)
{
	uint xticks;
	uint rsbuf;
	uint run_seconds;
	uint uptime_seconds;
	uint uptime_hours;
	uint uptime_minutes;

	xticks = ticks;
	rsbuf = run_seconds = xticks/100;
	uptime_hours = (rsbuf/60/60);
	rsbuf = (rsbuf - ((rsbuf/60/60)*60*60));
	uptime_minutes = (rsbuf/60);
	rsbuf = (rsbuf - ((rsbuf/60)*60));
	uptime_seconds = rsbuf;
	sysuart = 1;
	syscons = 1;
	if(op == 0){
		cli();
		cprintf("cpu%d: system online for %uh %um %us\n", cpu->id,
				uptime_hours, uptime_minutes, uptime_seconds);
		// cprintf("cpu%d: system online for %u ticks\n",cpu->id, xticks);
		cprintf("cpu%d: system halted.\n", cpu->id);
		halted = 1;
		for(;;)
			hlt();
	}
}

int
systime_write(struct inode *ip, char *bf, int nbytes, int off)
{
	seterr(EDNOWRITE);
	return -1;
}

int
systime_read(struct inode *ip, char *bf, int nbytes, int off)
{
	seterr(EDNOREAD);
	return -1;
//	uint xticks;
//	struct rtcdate curtime;
//	u64int unixtime = 0;
//	u64int jan12000 = 946684800;
//
//	xticks = ticks;
//	cmostime(&curtime);
//	unixtime += curtime.second;
//	unixtime += (curtime.minute)*(60);
//	unixtime += (curtime.hour)*(60*60);
//	unixtime += (curtime.day)*(24*60*60);
}

