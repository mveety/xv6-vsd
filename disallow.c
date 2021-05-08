#include <libc.h>

int
main(void)
{
	if(getuid() != -1){
		printf(2, "disallow: error: can only be run by system\n");
		exit();
	}
	if(getsid() != -1){
		printf(2, "disallow: error: can only be run on the global system\n");
		exit();
	}
	if(getts() == 0){
		printf(1, "disallow: disabling timesharing\n");
		disallow();
	} else {
		printf(1, "disallow: enabling timesharing\n");
		disallow();
	}
	exit();
}

