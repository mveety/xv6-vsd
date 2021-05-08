#include <libc.h>

int
main(int argc, char *argv[])
{
	int infd, outfd;
	char buf[1024];

	if(argc < 3){
		printf(2,"usage: %s [in file] [out file]\n", argv[0]);
		exit();
	}
	infd = open(argv[1], O_RDONLY);
	outfd = open(argv[2], O_WRONLY|O_CREATE);
	if(infd < 0){
		printf(2, "error: could not open %s\n", argv[1]);
		exit();
	} else if(outfd < 0){
		printf(2, "error: could not open %s\n", argv[0]);
		exit();
	}
	while(read(infd, buf, 1024))
		write(outfd, buf, 1024);
	close(infd);
	close(outfd);
	exit();
}

