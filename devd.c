#include <libc.h>

char devd_errstr[ERRMAX];

#define devd_ESYNTAX "syntax error"

typedef struct devicedef {
	char *fname;
	int major;
	int minor;
} devicedef;

int debug = 0;
int nodaemon = 0;
int firstrun = 1;
int parent;

int ddi = 0;
int nddi = 0;
devicedef defs[256];  // should be enough for now.
devicedef newdefs[256];
char linebuf[128];

char*
readline(int fd)
{
	char buf[128];
	char *retval;

	memset(buf, 0, 128);
	fgets(fd, buf, 127);
	retval = linebuf;
	if(debug)
		printf(2, "devd: len = %d, line = %s\n", strlen(buf), buf);
	if(buf[0] != '\0'){
		memcpy(retval, buf, sizeof(buf));
		return retval;
	}
	return 0;
}

int
devdefequ(devicedef *a, devicedef *b)
{
	if(strcmp(a->fname, b->fname) == 0)
		return 1;
	return 0;
}

int
newdev(devicedef *a)
{
	if(a->fname == nil)
		return -1;
	for(int i = 0; i < ddi; i++)
		if(devdefequ(a, &defs[i]))
			return 0;
	return 1;
}

int
parseent(char *och)
{
	char bf1[42];
	char bf2[42];
	char bf3[42];
	int strmax = strlen(och);
	char *ch, *fch;
	devicedef *d;
	int i;

	if(strmax < 2)
		return 0;
	if(*och == '#')
		return 0;
	fch = ch = strdup(och);
	if(debug)
		printf(2, "devd: instring = %s, instring_len = %d\n", ch, strmax);
	d = &newdefs[nddi];
	memset(d, 0, sizeof(devicedef));
	memset(bf1, 0, 42);
	memset(bf2, 0, 42);
	memset(bf3, 0, 42);
	for(i = 0; i < strmax; i++){
		if(ch[i] == ' ' || ch[i] == '\n'){
			ch[i] = '\0';
			break;
		}
	}
	strcpy(bf1, ch);
	if(debug)
		printf(2, "devd: bf1 = %s\n", bf1);
	ch += i;
	ch++;
	strmax -= i;
	for(i = 0; i < strmax; i++){
		if(ch[i] == ' ' || ch[i] == '\n'){
			ch[i] = '\0';
			break;
		} else {
			bf2[i] = ch[i];
		}
	}
	strcpy(bf2, ch);
	ch += i;
	ch++;
	strmax -= i;
	strcpy(bf3, ch);
	newdefs[nddi].fname = strdup(bf1);
	newdefs[nddi].major = atoi(bf2);
	newdefs[nddi].minor = atoi(bf3);
	nddi++;
	free(fch);
	return nddi-1;
}

int
readfile(char *fname)
{
	int fd;
	char *bf;
	int i = 1;
	int opstat;
	char fnbuf[256];
	char *rfnbuf;
	int nd = 0;

	fd = open(fname, O_RDONLY);
	if(fd < 0){
		seterr(EICANTOPEN);
		return -1;
	}
	memset(fnbuf, 0, 256);
	memcpy(fnbuf, "/dev/", 5);
	rfnbuf = fnbuf+5;
	for(;;){
		bf = readline(fd);
		if(bf == 0)
			break;
			
		if(*bf == '#')
			continue;
		if(*bf == '\n')
			continue;
		if(bf[0] == 'e' && bf[1] == 'n' && bf[2] == 'd')
			break;
		if(parseent(bf) < 0){
			if(firstrun)
				printf(2, "devd: error on line %d: %s\n", i, bf);
			seterr(devd_ESYNTAX);
			return -1;
		}
		i++;
	}
	close(fd);
	if(firstrun)
		printf(1, "devd: found %d devices\n", nddi);
	for(i = 0; i < nddi; i++){
		if((nd = newdev(&newdefs[i])) == 1){
			if(ddi < 256){
				defs[ddi].fname = strdup(newdefs[i].fname);
				defs[ddi].major = newdefs[i].major;
				defs[ddi].minor = newdefs[i].minor;
				if(firstrun)
					printf(1, "devd: adding new device %s(%d,%d)\n",
							defs[ddi].fname, defs[ddi].major, defs[ddi].minor);
				strcpy(rfnbuf, defs[ddi].fname);
				opstat = mknod(fnbuf, (short)defs[ddi].major, (short)defs[ddi].minor);
				if(opstat < 0){
					if(open(fnbuf, O_RDWR) > -1){
						if(0)
							printf(1, "devd: device already created\n");
					} else {
						if(firstrun)
							printf(1, "devd: failed to add device %s.\n", defs[ddi].fname);
						return -1;
					}
				}
				memset(rfnbuf, 0, 251);
				ddi++;
			} else {
				if(firstrun)
					printf(2, "device table full\n");
				return -1;
			}
		} else {
			if(nd == -1)
				break;
		}
	}
	memset(newdefs, 0, sizeof(devicedef)*256);
	return 0;
}

void
run(void)
{
	int status;	
	status = readfile("/etc/devices");
	if(status == -1)
		if(firstrun){
			printf(2, "devd: processing /etc/devices failed. exiting.: %r\n");
			send(parent, 0, nil, 0);
			exit();
		}
	firstrun = 0;
}

void
usage(void)
{
	printf(2, "usage: devd [-df]\n");
	exit();
}

void
do_update(void*)
{
	printf(1, "devd: scanning /etc/devices\n");
	run();
}

void
daemon(void*)
{
	receive(nil);
	for(;;){
		do_update(nil);
		if(parent > 0){
			send(parent, 0, nil, 0);
			parent = 0;
		}
		sleep(80);
	}
}

int
main(int argc, char *argv[])
{
	int argv_len = 0, pid;
	char *args;

	if(getuid() != -1){
		printf(2, "devd: error: can only be run by system\n");
		exit();
		return 0;
	}
	if(argc == 2){
		args = argv[1];
		argv_len = strlen(argv[1]);
		if(*args == '-'){
			argv_len--;
			args++;
		}
		for(int i = 0; i < argv_len; i++){
			args += i;
			switch(*args){
			case 'f':
				nodaemon = 1;
				break;
			case 'd':
				debug = 1;
				break;
			default:
				usage();
				break;
			}
		}
		if(debug){
			printf(2, "devd: debugging enabled\n");
		}
	}
	if(nodaemon){
		printf(2,"devd: not forking off daemon\n");
		run();
		exit();
	}
	parent = getpid();
	pid = pspawn(&daemon, nil);
	printf(1, "devd: made background process (pid = %d)\n", pid);
	send(pid, 0, nil, 0);
	receive(nil);
	return 0;
}
