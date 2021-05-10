#include <libc.h>

int ppid;

void
printer(void* stuff)
{
	int msgsz;
	char *msg;
	char goodbye[] = "goodbye!";
	int i = 0;

	printf(1, "printer: start\n");
	sleep(5);
	for(;;){
		msgsz = recvwait();
		msg = mallocz(msgsz);
		recvmsg(msg, msgsz);
		if(msg[0] == 4)
			break;
		printf(1, "printer: msg%d: %s\n", i, msg);
//		free(msg);
		i++;
	}
	printf(1,"printer: sleeping 30 seconds\n");
	sleep(30); // wtf it like jumps to the start after this returns?
	sendmsg(ppid, goodbye, sizeof(goodbye));
	printf(1, "printer: done.\n");
}

int
main(int argc, char *argv[])
{
	void *stack = malloc(4096);
	int tpid;
	int i;
	int argsz;
	char done = 4;
	void *test;

	ppid = getpid();
	if(argc < 2){
		printf(2, "usage: %s [strings]\n", argv[0]);
		exit();
	}

	tpid = spawn(&printer, nil);
	
	sleep(10);
	for(i = 0; i < argc-1; i++){
		argsz = strlen(argv[i+1])+1;
		sendmsg(tpid, argv[i+1], argsz);
	}
	sendmsg(tpid, &done, 1);
	sleep(1);
	printf(1, "waiting for child...\n");
	recvwait();
	sleep(1);
	printf(1, "done.\n");
	exit();
}
