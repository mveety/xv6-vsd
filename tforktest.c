#include <libc.h>

void
tforktest(void)
{
	int *ptr;
	int ppid, cpid;
	Lock *l, *a;

	printf(1, "tfork test start\n");
	l = makelock(nil);
	a = makelock(nil);
	ptr = malloc(sizeof(int));
	if(!l || !ptr){
		printf(1, "unable to malloc for tforktest\n");
		exit();
	}
	ppid = getpid();
	*ptr = 0;
	lock(l);
	lock(a);
	switch(cpid = tfork()){
	case 0:
		sleep(1);
		printf(1, "child(%d): before: *ptr = %d\n", getpid(), *ptr);
		unlock(a);
		lock(l);
		printf(1, "child(%d): after: *ptr = %d\n", getpid(), *ptr);
		unlock(l);
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
		sleep(2);
		lock(a);
		*ptr = 10;
		printf(1, "parent(%d): *ptr = %d\n", ppid, *ptr);
		unlock(l);
		break;
	}
	wait();
	free(ptr);
	freelock(l);
	freelock(a);
	printf(1, "tfork test OK\n");
}

int
main(int argc, char *argv[])
{
	tforktest();
	exit();
}

