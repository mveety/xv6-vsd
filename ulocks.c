#include <libc.h>

unsigned int
cmpswp(volatile uint *ptr, uint new)
{
	uint res;

	asm volatile("				\
				lock			\
				xchgl %0, %1	\
				"
				: "+m"(*ptr), "=a"(res)
				: "1"(new)
				: "cc"
			);
	return res;
}

int
initlock(Lock *l, char *name)
{
	if(name != nil)
		l->name = strdup(name);
	if(l->name == nil && name != nil){
		free(l);
		seterr(EUALLOCFAIL);
		return -1;
	}
	return 0;
}

Lock*
makelock(char *name)
{
	Lock *l;

	l = mallocz(sizeof(Lock));
	if(l == nil)
		return nil;
	if(initlock(l, name) < 0)
		return nil;
	return l;
}

void
freelock(Lock *l)
{
	if(l->name != nil)
		free(l->name);
	free(l);
}

int
held(Lock *l)
{
	if(l->pid == getpid())
		return 1;
	return 0;
}

int
lock(Lock *l)
{
	if(held(l)){
		seterr(EULOCKHELD);
		return -1;
	}
	while(cmpswp(&l->val, 1) != 0)
		if(sleep(0) == -1){
			printf(2, "lock %d:\"%s\": proc killed\n", getpid(),
					l->name != nil ? l->name : "no name");
			exit();
		}
	l->pid = getpid();
	return 0;
}

int
unlock(Lock *l)
{
	if(l->val == 0){
		seterr(EUNOTHELD);
		return -1;
	}
	l->pid = 0;
	cmpswp(&l->val, 0);
	return 0;
}

int
canlock(Lock *l)
{
	return l->val == 0 ? 1 : 0;
}

int
canunlock(Lock *l)
{
	return l->val;
}

