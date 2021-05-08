// this program tests the thread nesting prevention and thread reparenting

#include <libc.h>

Lock *l1, *l2, *l3, *l4;
void *c1stk, *c2stk, *c3stk, *c4stk;

void
gchildthread(Lock *l)
{
	int mypid = getpid();

	printf(2, "deepthreads: child %d: waiting for parent's lock\n", mypid);
	lock(l);
	printf(2, "deepthreads: child %d: exiting\n", mypid);
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
		printf(2, "deepthreads: could not create grandchild: %r\n");
		exit();
	}
	printf(2, "deepthreads: child %d: waiting for main lock\n", mypid);
	lock(m);
	printf(2, "deepthreads: child %d: unlocking my child's lock\n", mypid);
	unlock(c);
	printf(2, "deepthreads: child %d: exiting\n", mypid);
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

	l1 = makelock(nil);
	l2 = makelock(nil);
	l3 = makelock(nil);
	l4 = makelock(nil);
	if(!l1 || !l2 || !l3 || !l4){
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
		if(strcmp(argv[1], "-h") == 0)
			usage();
	}
	switch(clone((uint)c1stk)){
	case 0:
		childthread(l1, l2, c2stk);
		break;
	case -1:
		printf(2, "could not create child: %r\n");
		exit();
	}
	switch(clone((uint)c3stk)){
	case 0:
		childthread(l3, l4, c4stk);
		break;
	case -1:
		printf(2, "could not create child: %r\n");
		exit();
	}
	printf(1, "test start\n");
	sleep(10);
	if(proper){
		printf(1, "unlocking\n");
		unlock(l1);
		unlock(l3);
		sleep(10);
	}
	printf(1, "done.\n");
	exit();
}

