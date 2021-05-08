#include <libc.h>

void runshell(int);
void getuname(char*, int);
int parseuname(char*, int);

int
main(int argc, char *argv[])
{
	char buf[129];
	char *consfile;
	int bufmax = 128;
	int uid;
	int emerfd = 0;
	int pid;

	buf[128]='\0';
	if(getuid() != -1){
		printf(2, "error: login can only be run by system\n");
		return -1;
	}
	if(argc == 2){
		printf(2, "login: starting on %s\n", argv[1]);
		consfile = argv[1];
		if((pid = fork()) == 0){
			emerfd = dup(1);
			close(0); close(1); close(2);
			if(open(consfile, O_RDWR) < 0){
				printf(emerfd, "login: error: unable to open %s\n", consfile);
				exit();
			}
			dup(0); dup(0); dup(0);
			close(emerfd);
		} else if(pid == -1) {
			printf(2, "login: unable to fork: %r\n");
			exit();
		} else {
			exit();
		}
	}
	for(;;){
		printf(2, "\nWelcome to VSD Release 1\n");
		if(getts() == 1){
			printf(2,"single user mode, logging in as system\n");
			runshell(-1);
			continue;
		}
		printf(2, "login: ");
		getuname(buf, bufmax);
		switch((uid=parseuname(buf, bufmax))){
		case -1:
			printf(2, "warning: logging in as system\n");
			runshell(uid);
			break;
		case -3:
			printf(2, "invalid username\n");
			break;
		default:
			runshell(uid);
		}
	}
	exit(); // never gets here
}

void
runshell(int uid)
{
	char *argv[2];

	argv[0] = "sh";
	argv[1] = (char*)0;
	if(getts() == 1 && uid != -1){
		printf(2, "error: cannot log in, single user mode\n");
		return;
	}
	switch(fork()){
	case 0:
		if(chuser(uid) == -1){
			printf(2, "unable to chuser to %d\n", uid);
			exit();
		}
		printf(2, "debug: execing /bin/sh\n");
		exec("/bin/sh", argv);
		break;
	case -1:
		printf(2, "error: unable to start process for uid %d\n", uid);
		return;
	default:
		wait();
	}
}

void
getuname(char *bf, int bflen)
{
	gets(bf, bflen);
}

int  // this is a very naive test for "string" equality.
streq(char *str0, char *str1)
{
	if(strlen(str0) != strlen(str1))
		return 0;
	for(int i = 0; i < strlen(str0); i++)
		if(str0[i] != str1[i])
			return 0;
	return 1;
}

int
parseuname(char *bf, int bflen)
{
	if(strlen(bf) > 0)
		bf[strlen(bf)-1] = '\0';
	if(streq(bf, "system"))
		return -1;
	if(streq(bf, "root"))
		return 0;
	return -3;
}

