#include <libc.h>

void
usage(char *argv0)
{
	printf(1, "usage: %s [-abcdeghjk] [-d stuff] -f [stuff]\n", argv0);
	exit();
}

int
main(int argc, char *argv[])
{
	int flags[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	char argflags[] = "abcdefghjk";
	char *testf;
	char *testd;
	int totalargs;
	int argsoutput;
	int i;

	argsoutput = args(argc, argv, &flags[0], &argflags[0]);
	testf = argf(argc, argv, 'f');
	testd = argf(argc, argv, 'd');
	totalargs = lastarg(argc, argv, "df");

	printf(1, "argsoutput = %d\n", argsoutput);
	printf(1, "testd = %s\n", testd);
	printf(1, "testf = %s\n", testf);
	printf(1, "totalargs = %d\n", totalargs);
	for(i = 0; i < 10; i++)
		printf(1, "\tflags[%d] = %d\n", i, flags[i]);
	if(flags[7])
		usage(argv[0]);
	return 0;
}
