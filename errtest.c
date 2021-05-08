#include <libc.h>

int
main(void)
{
	char errstrbuf[128];
	int st;

	printf(1, "writing errstr 1. status = ");
	printf(1, "%d\n", werrstr(EROBPIKE, sizeof(EROBPIKE)));
	printf(1, "reading errstr 1. value = ");
	st = rerrstr(errstrbuf, 128);
	printf(1, "%s (status = %d)\n", errstrbuf, st);
	if(strcmp(errstrbuf, EROBPIKE) == 0)
		printf(1, "errstr matches defined errstr\n");
	else
		printf(1, "errstr does not match defined errstr\n");
	printf(1, "writing errstr 2. status = ");
	printf(1, "%d\n", werrstr(EAIJU, sizeof(EAIJU)));
	printf(1, "check val = %d\n", rerrstr(nil, 0));
	printf(1, "check cal = %d\n", rerrstr(nil, 0));
	printf(1, "done.\n");
	exit();
}

