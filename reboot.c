#include <libc.h>

int
main(void)
{
	int fd = open("/dev/sysctl", O_RDWR);
	char errbuf[256];

	if(getuid() >= 0){
		printf(2, "reboot: can only be run by system\n");
		exit();
		return 0;
	}
	memset(errbuf, 0, 256);
	if(fd < 0){
		if(rerrstr(errbuf, 0)){
			rerrstr(errbuf, 256);
		}
		printf(2, "reboot: could not open /dev/sysctl: %s\n", errbuf);
		exit();
		return -1;
	}
	write(fd, "reboot  ", strlen("reboot  "));
	if(rerrstr(errbuf, 0)){
		rerrstr(errbuf, 256);
		printf(2, "reboot: error: %s\n", errbuf);
		exit();
		return -1;
	}
	return 0;
}

