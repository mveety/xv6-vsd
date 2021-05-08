#include <libc.h>

void usage(void);

int
main(int argc, char *argv[])
{
	int sleeplen = 0;

	if(argc < 2 || argc > 2)
		usage();
	sleeplen = atoi(argv[1]);
	if(sleeplen <=0){
		printf(2, "sleep: error: duration must be greater than 0\n");
		usage();
	}
	sleep(sleeplen);
	exit();
}

void
usage(void)
{
	printf(2, "usage: sleep [duration]\n");
	exit();
}

