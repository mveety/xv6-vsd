#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "users.h"

int
checksysperms(struct proc *p0, struct proc *p1)
{
// this returns 0 if the process p1 can operate on process p0
	int p0uid, p1uid;
	int p0sid, p1sid;

// no permissions checking in single user mode
	if(singleuser == 1)
		return 0;
	p0uid = p0->uid;
	p1uid = p1->uid;
	p0sid = p0->sid;
	p1sid = p1->sid;
	if(p1sid == -1 && p0sid > p1sid){
		return 0;
	}else if (p1sid == p0sid){
		if(p1uid == -1)
			return 0;
		else if(p1uid == p0uid)
			return 0;
		}
	return -1;
}

int
chuser(int uid)
{
// in single user mode we only allow uid -1
	if(singleuser == 1){
		if(uid == -1)
			return 0;
		else
			return -1;
	}
	if(proc->uid == -1 && uid >= -1){
		proc->uid = uid;
		return 0;
	}
	return -1;
}

int
chsystem(int sid)
{
// only one system in single user
	if(singleuser == 1)
		return 0;
	if(proc->sid == -1 && sid >= 0){
		proc->sid = sid;
		proc->uid = -1;
		return 0;
	}
	return -1;
}

int
disallow(void)
{
	if(singleuser == 1){
		singleuser = 0;
		return 0;
	}
	if(singleuser == 0 && proc->uid == -1 && proc->sid == -1){
		singleuser = 1;
		return 0;
	}
	return -1;
}
