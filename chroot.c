#include <libc.h>

void dochroot(char*);

int
main(int argc, char *argv[])
{
	if(argc != 2){
		printf(2, "usage: %s [path]\n", argv[0]);
		exit();
	}
	switch(fork()){
	case 0:
		dochroot(argv[1]);
		break;
	case -1:
		printf(2, "error: fork failed\n");
		exit();
		break;
	default:
		wait();
		exit();
	}
	return 0;
}

void
dochroot(char *path)
{
	int fd0, fd1;
	char fnamebuf[256];
	char buf[1026];
	char *chroot_sh = "/_CHROOT_SH";
	char *args[] = {"sh", nil};
	int pathlen = 0;

	fd0 = open("/bin/sh", O_RDONLY);
	if(fd0 < 0){
		printf(2, "error: could not open /bin/sh\n");
		exit();
	}
	pathlen = strlen(path);
	strcpy(fnamebuf, path);
	strcpy(fnamebuf+pathlen, chroot_sh);
	fd1 = open(fnamebuf, O_WRONLY|O_CREATE);
	if(fd1 < 0){
		printf(2, "error: could not open %s\n", fnamebuf);
		exit();
	}
	while(read(fd0, buf, 1024))
		write(fd1, buf, 1024);
	close(fd0);
	close(fd1);
	if(chroot(path) == -1){
		printf(2, "error: unable to change root to %s\n", path);
		exit();
	}
	exec(chroot_sh, args);
	// if we get here something failed. just exit.
	printf(2, "error: unable to exec new shell (name = %s)\n", chroot_sh);
	exit();
}

