#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "users.h"

#define MAXUSER 256
#define MAXUNAME 16

typedef struct Userdef {
	uchar inuse;  // is this def in use
	uchar locked; // is the user locked out
	uchar system; // is this a system user (ie. nondeletable)
	char name[MAXUNAME];   // username string
	int uid;      // user id
	int passwd;   // hashed password
} Userdef;

struct spinlock usertable;
Userdef users[MAXUSER];
int usersi;

int
authinit(void)
{
	Userdef *system_user;

	initlock(&usertable, "auth");
	memset(users, 0, sizeof(Userdef)*256);
	usersi = 0;
	system_user = &users[0];
	for(int i = 0; i < MAXUSER; i++)
		users[i].inuse = 0;
	usersi++;	
	system_user->inuse = 1;
	system_user->locked = 0;
	system_user->system = 1;
	system_user->name = "system";
	system_user->uid = -1;
	system_user->passwd = 0;
}

void
reorderutable(void)
{
	Userdef nusers[256];
	int i;

	// in normal cases this is only called when the user table is already
	// locked. if you try locking it again deadlock occurs.
	// acquire(&usertable);
	memcpy(nusers, users, MAXUSER*sizeof(Userdef));
	usersi = 0;
	for(i = 0; i < MAXUSER; i++){
		if(nusers[i].inuse == 1){
			users[usersi].inuse = nusers[i].inuse;
			users[usersi].locked = nusers[i].locked;
			users[usersi].system = nusers[i].system;
			memcpy(&users[usersi].name, &nusers[i].name, MAXUNAME);
			users[usersi].uid = nusers[i].uid;
			users[usersi].passwd = nusers[i].passwd;
			usersi++;
		}
	}
	memset(nusers, 0, MAXUSER*sizeof(Userdef)); // nuke that memory
	// release(&usertable);
}

int
adduser(int uid, char *name, int passwd)
{
	Userdef *nuser;
	int unamelen;

	acquire(&usertable);
	if(usersi >= MAXUSER){
		seterr(ESNOFREEUSER);
		return -1;
	}
	nuser = &users[usersi];
	unamelen = strlen(name);
	if(unamelen+1 >= MAXUNAME){
		seterr(ESUNAMETOOLONG);
		return -1;
	} else if(unamelen <= 1){
		seterr(ESUNAMETOOSHORT);
		return -1;
	}
	nuser->uid = uid;
	memcpy(nuser->name, name, unamelen);
	nuser->passwd = passwd;
	nuser->inuse = 1;
	nuser->locked = 0;
	nuser->system = 0;
	usersi++;
	return 0;
}

int
deluser(int uid)
{
	int found = 0;

	for(int i = 0; i < MAXUSER; i++){
		if(users[i].uid == uid){
			users[i].inuse = 0;
			found = 1;
			break;
		}
	}
	if(!found){
		seterr(ESNOUSER);
		return -1;
	}
	reorderutable();
	return 0;
}

// this implements "shithash"
int
hashpass(char *passwd, int *hash)
{
	int bf = 0;
	int i = 0;
	int passlen;

	passlen = strlen(passwd);
	for(i = 0; i < passlen; i++)
		bf += (int)passwd[i];
	bf += passlen;
	memcpy(hash, bf, sizeof(int));
	return 0;
}



