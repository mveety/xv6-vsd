#include <libc.h>

char*
fmtname(char *path)
{
	static char buf[DIRSIZ+1];
	char *p;
	
	// Find first character after last slash.
	for(p=path+strlen(path); p >= path && *p != '/'; p--)
		;
	p++;
	
	// Return blank-padded name.
	if(strlen(p) >= DIRSIZ)
		return p;
	memmove(buf, p, strlen(p));
	memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
	return buf;
}

static char *types[] = {
	[T_FILE] "T_FILE",
	[T_DIR] "T_DIR",
	[T_DEV] "T_DEV",
};

void
ls(char *path)
{
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;
	
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
	if(0){
		printf(2, "stat = {\n");
		printf(2, "\ttype = %s\n", types[st.type]);
		printf(2, "\tdev = %d\n", st.dev);
		printf(2, "\tino = %d\n", st.ino);
		printf(2, "\tnlink = %d\n", st.nlink);
		printf(2, "\tsize = %d\n", st.size);
		printf(2, "\towner = %d\n", st.owner);
		printf(2, "\tperms = %x\n", st.perms);
		printf(2, "};\n");
	}
	printf(1, "file type inode size owner perms\n");
	switch(st.type){
	case T_FILE:
		printf(1, "%s %d %d %d %d %x\n", fmtname(path), st.type, st.ino, st.size,
				st.owner, st.perms);
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
			printf(1, "%s %d %d %d %d %x\n", fmtname(buf), st.type, st.ino, st.size,
					st.owner, st.perms);
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
