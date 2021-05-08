#include <libc.h>

int
main(int argc, char *argv[])
{
	int perms;
	if(argc < 2){
		printf(2, "usage: %s [file]\n", argv[0]);
		exit();
	}
	perms = rfperms(argv[1]);
	if(perms == -1){
		printf(2, "error: %r\n");
		exit();
	}
	printf(1, "%s (user=", argv[1]);
	if((perms&U_READ) == U_READ)
		printf(1, "r");
	else
		printf(1, "-");
	if((perms&U_WRITE) == U_WRITE)
		printf(1, "w");
	else
		printf(1, "-");
	if((perms&U_EXEC) == U_EXEC)
		printf(1, "x");
	else
		printf(1, "-");
	if((perms&U_HIDDEN) == U_HIDDEN)
		printf(1, "h");
	else
		printf(1, "-");
	printf(1, ", world=");
	if((perms&U_READ) == U_READ)
		printf(1, "r");
	else
		printf(1, "-");
	if((perms&U_WRITE) == U_WRITE)
		printf(1, "w");
	else
		printf(1, "-");
	if((perms&U_EXEC) == U_EXEC)
		printf(1, "x");
	else
		printf(1, "-");
	if((perms&U_HIDDEN) == U_HIDDEN)
		printf(1, "h");
	else
		printf(1, "-");
	printf(1,")\n");
	exit();
}

