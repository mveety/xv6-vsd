#include <libc.h>

char*
fmtname(char *path)
{
	static char buf[DIRSIZ+1];

	return fmtnamen(path, buf, sizeof(buf));
}

char*
fmtperms(int perms, char *buf)
{
	return fmtpermsn(perms, buf, 16);
}

char*
fmtfield(char *data, int fieldsize, char filler)
{
	char buf[64];
	char *field;

	field = fmtfieldn(data, fieldsize, filler, buf, sizeof(buf));
	return strdup(field);
}

static char *types[] = {
	[T_FILE] "File  ",
	[T_DIR] "Direct",
	[T_DEV] "Device",
};

void
ls(char *path)
{
	char buf[512], permsbuf[16], *p;
	char *fmt = "%s %s %s %s %s %s\n";
	char *str_ino, *str_size, *str_owner;
	char *fmt_ino, *fmt_size, *fmt_owner;
	int fd;
	Dirent de;
	Stat st;
	
	if((fd = open(path, 0)) <= 0){
		printf(2, "ls: cannot open %s: %r\n", path);
		return;
	}
	if(rerrstr(nil, 0)){
		printf(2, "ls: %r\n");
		exit();
	}
	if(fstat(fd, &st) < 0){
		printf(2, "ls: cannot stat %s: %r\n", path);
		close(fd);
		return;
	}
	if(rerrstr(nil, 0)){
		printf(2, "ls: %r\n");
		exit();
	}
	printf(1, "file           type   inode  size   owner     perms\n");
	switch(st.type){
	case T_FILE:
	case T_DEV:
		str_ino = swriteint(st.ino, 10, 0);
		str_size = swriteint(st.size, 10, 0);
		str_owner = swriteint(st.owner, 10, 1);
		fmt_ino = fmtfield(str_ino, 5, ' ');
		fmt_size = fmtfield(str_size, 7, ' ');
		fmt_owner = fmtfield(str_owner, 5, ' ');
		printf(1, fmt, fmtname(path), types[st.type], fmt_ino, fmt_size,
				fmt_owner, fmtperms(st.perms, permsbuf));
		free(str_ino);
		free(str_size);
		free(str_owner);
		free(fmt_ino);
		free(fmt_size);
		free(fmt_owner);
		break;
	case T_DIR:
		if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
			printf(1, "ls: path too long\n");
			break;
		}
		strcpy(buf, path);
		p = buf+strlen(buf);
		*p++ = '/';
		while(read(fd, &de, sizeof(de)) == sizeof(de)){
			if(de.inum == 0)
				continue;
			memmove(p, de.name, DIRSIZ);
			p[DIRSIZ] = 0;
			if(stat(buf, &st) < 0){
				printf(1, "ls: cannot stat %s: %r\n", buf);
				continue;
			}
			str_ino = swriteint(st.ino, 10, 0);
			str_size = swriteint(st.size, 10, 0);
			str_owner = swriteint(st.owner, 10, 1);
			fmt_ino = fmtfield(str_ino, 5, ' ');
			fmt_size = fmtfield(str_size, 7, ' ');
			fmt_owner = fmtfield(str_owner, 5, ' ');
			printf(1, fmt, fmtname(buf), types[st.type], fmt_ino, fmt_size,
					fmt_owner, fmtperms(st.perms, permsbuf));
			free(str_ino);
			free(str_size);
			free(str_owner);
			free(fmt_ino);
			free(fmt_size);
			free(fmt_owner);
		}
		if(rerrstr(nil, 0))
			printf(2, "ls: read error: %r\n");
		break;
	}
	close(fd);
}

int
main(int argc, char *argv[])
{
	int i;

	if(argc < 2){
		ls(".");
		exit();
	}
	for(i=1; i<argc; i++)
		ls(argv[i]);
	exit();
}
