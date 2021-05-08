#include <libc.h>

int sysctlfd;

void usage(void);
void sysuart(int);
void singleuser(int);
void allowthreads(int);
void useclone(int);

int
main(int argc, char *argv[])
{
	if(getuid() > -1){
		printf(2, "sysctl: error: can only be run by system\n");
		exit();
	}
	if(argc < 2)
		usage();

	if((sysctlfd = open("/dev/sysctl", O_RDWR)) < 0){
		printf(2, "sysctl: error: could not open /dev/sysctl: %r\n");
		exit();
	}

	if(strcmp(argv[1], "sysuart") == 0){
		if(argc < 3){
			printf(2, "usage: sysctl sysuart [0|1]\n");
			exit();
		}
		sysuart(atoi(argv[2]));
	} else if(strcmp(argv[1], "singleuser") == 0){
		if(argc < 3){
			printf(2, "usage: sysctl singleuser [0|1]\n");
			exit();
		}
		singleuser(atoi(argv[2]));
	} else if(strcmp(argv[1], "allowthreads") == 0){
		if(argc < 3){
			printf(2, "usage: sysctl allowthreads [0|1]\n");
			exit();
		}
		allowthreads(atoi(argv[2]));
	} else if(strcmp(argv[1], "useclone") == 0){
		if(argc < 3){
			printf(2, "usage: sysctl useclone [0|1]\n");
			exit();
		}
		useclone(atoi(argv[2]));
	} else
		usage();
	close(sysctlfd);
	exit();
}

void
sysuart(int st)
{
	char *enable = "sysuart yes  \0";
	char *disable = "sysuart no  \0";
	if(st)
		write(sysctlfd, enable, strlen(enable));
	else
		write(sysctlfd, disable, strlen(disable));
}

void
singleuser(int st)
{
	char *enable = "singleuser yes  \0";
	char *disable = "singleuser no  \0";
	if(st)
		write(sysctlfd, enable, strlen(enable));
	else
		write(sysctlfd, disable, strlen(disable));
}

void
allowthreads(int st)
{
	char *enable = "allowthreads yes  \0";
	char *disable = "allowthreads no  \0";
	if(st)
		write(sysctlfd, enable, strlen(enable));
	else
		write(sysctlfd, disable, strlen(disable));
}

void
useclone(int st)
{
	char *enable = "useclone yes  \0";
	char *disable = "useclone no  \0";
	if(st)
		write(sysctlfd, enable, strlen(enable));
	else
		write(sysctlfd, disable, strlen(disable));
}

void
usage(void)
{
	printf(2, "usage: sysctl [sysctl command] [options...]\n");
	exit();
}

