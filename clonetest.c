#include <libc.h>

void
clonetest(void)
{
	int *ptr;
	int ppid, cpid;
	Lock *a, *b;
	void *childstk;

	printf(1, "clone test start\n");
	a = makelock(nil);
	b = makelock(nil);
	ptr = malloc(sizeof(int));
	childstk = malloc(4096);
	if(!a || !b || !ptr || !childstk){
		printf(1, "unable to malloc for clonetest\n");
		exit();
	}
	ppid = getpid();
	*ptr = 0;
	lock(a);
	lock(b); // b is owned by child
	b->pid = 0;
	switch(cpid = clone((uint)childstk)){
	case 0:
		printf(1, "child(%d): before: *ptr = %d\n", getpid(), *ptr);
		lock(a);
		printf(1, "child(%d): after: *ptr = %d\n", getpid(), *ptr);
		// sleep(60);
		unlock(b);
		exit();
		break;
	case -1:
		if(rerrstr(nil, 0))
			printf(1, "error: could not create child tfork proc: %r\n");
		else
			printf(1, "error: could not create child tfork proc\n");
		exit();
		break;
	default:
		printf(1, "parent(%d): child %d created\n", ppid, cpid);
		sleep(1);
		*ptr = 10;
		unlock(a);
		printf(1, "parent(%d): *ptr = %d\n", ppid, *ptr);
		lock(b);
		break;
	}
	free(ptr);
	freelock(a);
	freelock(b);
}

int
main(int argc, char *argv[])
{
	clonetest();
	printf(1, "clone test OK\n");
	exit();
	return 0;
}

