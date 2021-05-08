// this program tests the thread nesting prevention and thread reparenting

#include <libc.h>

Lock *printlock;
Lock *l1, *l2, *l3, *l4;
void *c1stk, *c2stk, *c3stk, *c4stk;

void
gchildthread(Lock *l)
{
	int mypid = getpid();

	lock(printlock);
	printf(2, "deepthreads: child %d: waiting for parent's lock\n", mypid);
	unlock(printlock);
	lock(l);
	lock(printlock);
	printf(2, "deepthreads: child %d: exiting\n", mypid);
	unlock(printlock);
	exit();
}

void
childthread(Lock *m, Lock *c, void *stk)
{
	int mypid = getpid();
	switch(clone((uint)stk)){
	case 0:
		gchildthread(c);
		break;
	case -1:
		lock(printlock);
		printf(2, "deepthreads: could not create grandchild: %r\n");
		unlock(printlock);
		exit();
	}
	lock(printlock);
	printf(2, "deepthreads: child %d: waiting for main lock\n", mypid);
	unlock(printlock);
	lock(m);
	lock(printlock);
	printf(2, "deepthreads: child %d: unlocking my child's lock\n", mypid);
	unlock(printlock);
	unlock(c);
	lock(printlock);
	printf(2, "deepthreads: child %d: exiting\n", mypid);
	unlock(printlock);
	exit();
}

void
usage(void)
{
	printf(2, "usage: deepthreads [-p]\n");
	exit();
}

int
main(int argc, char *argv[])
{
	int proper = 0;
	int longsleep = 0;

	l1 = makelock(nil);
	l2 = makelock(nil);
	l3 = makelock(nil);
	l4 = makelock(nil);
	printlock = makelock(nil);
	if(!l1 || !l2 || !l3 || !l4 || !printlock){
		printf(2, "could not create locks: %r\n");
		exit();
	}
	c1stk = malloc(4096);
	c2stk = malloc(4096);
	c3stk = malloc(4096);
	c4stk = malloc(4096);
	if(!c1stk || !c2stk || !c3stk || !c4stk){
		printf(2, "could not create stacks: %r\n");
		exit();
	}
	lock(l1);
	lock(l2);
	lock(l3);
	lock(l4);

	if(argc == 2){
		if(strcmp(argv[1], "-p") == 0)
			proper = 1;
		if(strcmp(argv[1], "-S") == 0){
			proper = 1;
			longsleep = 1;
		}
		if(strcmp(argv[1], "-h") == 0)
			usage();
	}
	switch(clone((uint)c1stk)){
	case 0:
		childthread(l1, l2, c2stk);
		break;
	case -1:
		lock(printlock);
		printf(2, "could not create child: %r\n");
		unlock(printlock);
		exit();
	}
	switch(clone((uint)c3stk)){
	case 0:
		childthread(l3, l4, c4stk);
		break;
	case -1:
		lock(printlock);
		printf(2, "could not create child: %r\n");
		unlock(printlock);
		exit();
	}
	lock(printlock);
	printf(1, "test start\n");
	unlock(printlock);
	sleep(10);
	if(proper){
		lock(printlock);
		printf(1, "unlocking\n");
		unlock(printlock);
		unlock(l1);
		unlock(l3);
		sleep(10);
	}
	sleep(10);
	if(longsleep)
		sleep(60);
	printf(1, "done.\n");
	exit();
}

