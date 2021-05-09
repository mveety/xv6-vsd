#include <libc.h>

int
main(int argc, char *argv[])
{
	int pid;
	char message[] = "hello world!";

	if(argc < 2){
		printf(2, "usage: ~s [pid]\n", argv[0]);
		exit();
	}

	pid = atoi(argv[1]);
	printf(1, "message = \"%s\", size = %d\n", message, sizeof(message));
	if(sendmsg(pid, message, sizeof(message)) < 0)
		printf(2, "error: unable to send message\n");
	exit();
}
