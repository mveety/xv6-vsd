#include <libc.h>

int
main(int argc, char *argv[])
{
	int msgsz;
	char *message;
	
	printf(1, "waiting for message...\n");
	msgsz = recvwait();
	printf(1, "the next message will be %d bytes\n", msgsz);
	message = mallocz(msgsz);
	recvmsg(message, msgsz);
	printf(1, "message = \"%s\"\n", message);
	exit();
}
