#include <libc.h>

int kern = 0;
int ver = 0;
int node = 0;
int rel = 0;
int mach = 0;
int proc = 0;
int plat = 0;
int os = 0;

char *progname;

void printuname(void);
void usage(void);

void
printstr(char *str)
{
	printf(1, "%s ", str);
}

int
main(int argc, char *argv[])
{
	int argvlen = 0;
	char *x;
	int i = 0;

	progname = argv[0];
	os = 0;
	if(argc < 2)
		printuname();
	argvlen = strlen(argv[1]);
	x = argv[1];
	if(x[0] == '-')
		i++;
	for(; i < argvlen; i++){
		switch(x[i]){
		case 'a':
			kern = ver = node = rel = mach = proc = plat = os = 1;
			printuname();
			return 0;
		case 's':
			kern = 1;
			break;
		case 'n':
			node = 1;
			break;
		case 'r':
			rel = 1;
			break;
		case 'v':
			ver = 1;
			break;
		case 'm':
			mach = 1;
			break;
		case 'p':
			proc = 1;
			break;
		case 'i':
			plat = 1;
			break;
		case 'o':
			os = 1;
			break;
		default:
			usage();
		}
	}
	printuname();
	return 0;
}

char*
gethostname(void)
{
	char *buf;
	int fd;

	buf = malloc(256);
	if(buf == nil){
		printstr("uname: memory error\n");
		exit();
	}
	fd = open("/dev/sysname", 0);
	if(fd < 0){
		if(rerrstr(nil, 0))
			printf(2, "uname: could not open /dev/sysname: %r\n");
		else
			printf(2, "uname: could not open /dev/sysname\n");
		exit();
	}
	read(fd, buf, 256);
	close(fd);
	return buf;
}



void
printuname(void)
{
	if(os) printstr("xv6");
	if(node) printstr(gethostname());
	if(ver) printstr("prerelease");
	if(kern) printstr("vsd");
	if(rel) printstr("1");
	if(mach) printstr("i386");
	if(proc) printstr("i386");
	if(plat) printstr("IBM-PC");
	printf(1, "\n");
	exit();
}

void
usage(void)
{
	printf(2, "usage: %s [-asnrvmpio]\n", progname);
	exit();
}

