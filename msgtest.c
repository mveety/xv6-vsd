#include <libc.h>

void
msgthread(void *args)
{
	Message *msg;
	Mailbox *mbox;
	int *tmp;

	mbox = mailbox();

	printf(1, "msgthread: start\n");
	for(;;){
		msg = receive(mbox);
		switch(msg->sentinel){
		case 1:
			selectmsg(mbox, msg);
			printf(1, "msgthread: got a string: \"%s\"\n", msg->payload);
			freemsg(msg);
			break;
		case 2:
			selectmsg(mbox, msg);
			tmp = msg->payload;
			printf(1, "msgthread: got an int: %d\n", *tmp);
			freemsg(msg);
			break;
		case 3:
			selectmsg(mbox, msg);
			printf(1, "msgthread: got message 3\n");
			freemsg(msg);
			goto done;
			break;
		default:
			//printf(1, "msgthread: bad message. sentinel = %d\n", msg->sentinel);
			break;
		}
	}
done:
	printf(1, "msgthread: pre-flush mbox->messages = %d\n", mbox->messages);
	flush(mbox);
	printf(1, "msgthread: post-flush mbox->messages = %d\n", mbox->messages);
	free(mbox);
	printf(1, "msgthread: exiting\n");
}

int
main(int argc, char *argv[])
{
	char string1[] = "hello world\0";
	int int1 = 1234;
	int tpid;

	tpid = spawn(&msgthread, nil);

	send(tpid, 1, string1, sizeof(string1));
	send(tpid, 2, &int1, sizeof(int1));
	send(tpid, 4, &int1, sizeof(int1));
	send(tpid, 5, &int1, sizeof(int1));
	send(tpid, 6, &int1, sizeof(int1));
	send(tpid, 7, &int1, sizeof(int1));
	send(tpid, 8, &int1, sizeof(int1));
	send(tpid, 9, &int1, sizeof(int1));
	send(tpid, 10, &int1, sizeof(int1));
	send(tpid, 11, &int1, sizeof(int1));
	send(tpid, 3, &int1, sizeof(int1));
	sleep(30);
	exit();
}
