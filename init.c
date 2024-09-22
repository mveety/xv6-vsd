// init: The initial user-level program

#include <libc.h>

char *argv[] = { "login", 0 };
char *shargv[] = { "sh", "-sb", 0 };

int runrc(void);
int runlogin(void);

int
main(void)
{
	int pid, wpid;
	int uid, sid;
	int cfd;

	uid = getuid();
	sid = getsid();
	if(open("/dev/syscons", O_RDWR) < 0){
		if(mkdir("/dev") == 0){
			mknod("/dev/syscons", 1, 1);
			open("/dev/syscons", O_RDWR);
		} else if((cfd = open("/dev", O_RDONLY)) >= 0){
			close(cfd);
			mknod("/dev/syscons", 1, 1);
			open("/dev/syscons", O_RDWR);
		} else {
				printf(1, "init: error: could not create boot console\n");
				for(;;);
		}
	}
	dup(0);  // stdout
	dup(0);  // stderr
	if(sid == -1){
		printf(1, "init: running on global system\n");
	}
	if(uid != -1){
		printf(1, "init: error: init can only be run by system\n");
		return -1;
	}
	runrc();
	// pid = runlogin();
	// while((wpid=wait()) >= 0 && wpid != pid) ;
	while(wait() >= 0) ;
	return 0;
}

int
runrc()
{
	int pid = 0;
	printf(2, "init: running /etc/rc\n");
	switch((pid = fork())){
	case 0:
		close(0);
		open("/etc/rc", O_RDONLY);
		if(rerrstr(nil, 0))
			printf(2, "init: error: could not open /etc/rc: %r\n");
		exec("/bin/sh", shargv);
		break;
	case -1:
		printf(2, "init: error: unable to execute /etc/rc\n");
		for(;;) ;
		break;
	default:
		while(wait() != pid) ;
		return pid;
		break;
	}
	return 0;
}

int
runlogin()
{
	int pid = 0;
	printf(2, "init: running /bin/login\n");
	switch((pid = fork())){
	case 0:
		exec("/bin/login", argv);
		break;
	case -1:
		printf(2, "init: error: unable to execute /etc/rc\n");
		for(;;) ;
		break;
	default:
		return pid;
		break;
	}
	return 0;
}
