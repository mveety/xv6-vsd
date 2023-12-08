#include <libc.h>

int *global1, *global2;
Lock *a, *b, *c;

void
userproc(void*)
{
	lock(a);
	*global1 = 1234;
//	global2 = mallocz(sizeof(int));
	unlock(c);
	lock(b);
	printf(1, "child: global1 = %p, *global1 = %d\n", global1, *global1);
	printf(1, "child: global2 = %p, *global2 = %d\n", global2, *global2);
	sleep(30);
	exit();
}

int
main(int argc, char *argv[])
{
	int cpid;

	_threads_useclone = 1; // make sure clone is enabled
	a = makelock(nil);
	b = makelock(nil);
	c = makelock(nil);
	lock(a);
	lock(b);
	lock(c);
	c->pid = 0;
	global1 = mallocz(sizeof(int));
//	global2 = mallocz(sizeof(int));
	cpid = spawn(&userproc, nil);
	global2 = mallocz(sizeof(int));
	printf(1, "child (%d) spawned\n", cpid);
	unlock(a);
	lock(c);
	*global2 = 5678;
	unlock(b);
	sleep(1);
	printf(1, "parent: global1 = %p, *global1 = %d\n", global1, *global1);
	printf(1, "parent: global2 = %p, *global2 = %d\n", global2, *global2);
	wait();
	exit();
	return 0;
}
