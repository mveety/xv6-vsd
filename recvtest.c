#include <libc.h>

int
main(int argc, char *argv[])
{
	int msgsz;
	
	printf(1, "waiting for message...\n");
	msgsz = recvwait();
	printf(1, "the next message will by %d bytes\n", msgsz);
	exit();
}
