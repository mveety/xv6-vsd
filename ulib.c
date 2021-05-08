#include "libc.h"

extern int main(int argc, char *argv[]);

int
_start(int argc, char *argv[])
{
	getuid();
	main(argc, argv);
	exit();
}

char*
strcpy(char *s, char *t)
{
	char *os;

	os = s;
	while((*s++ = *t++) != 0)
		;
	return os;
}

int
strcmp(const char *p, const char *q)
{
	while(*p && *p == *q)
		p++, q++;
	return (uchar)*p - (uchar)*q;
}

uint
strlen(char *s)
{
	int n;

	for(n = 0; s[n]; n++)
		;
	return n;
}

char*
strchr(const char *s, char c)
{
	for(; *s; s++)
		if(*s == c)
			return (char*)s;
	return 0;
}

char*
gets(char *buf, int max)
{
	int i, cc;
	char c;

	for(i=0; i+1 < max; ){
		cc = read(0, &c, 1);
		if(cc < 1)
			break;
		buf[i++] = c;
		if(c == '\n' || c == '\r')
			break;
	}
	buf[i] = '\0';
	return buf;
}

char*
fgets(int fd, char *buf, int max)
{
	int i, cc;
	char c;

	for(i=0; i+1 < max; ){
		cc = read(fd, &c, 1);
		if(cc < 1)
			break;
		buf[i++] = c;
		if(c == '\n' || c == '\r')
			break;
	}
	buf[i] = '\0';
	return buf;
}

int
stat(char *n, struct stat *st)
{
	int fd;
	int r;

	fd = open(n, O_RDONLY);
	if(fd < 0)
		return -1;
	r = fstat(fd, st);
	close(fd);
	return r;
}

int
atoi(const char *s)
{
	int n;

	n = 0;
	while('0' <= *s && *s <= '9')
		n = n*10 + *s++ - '0';
	return n;
}

/*
void*
memmove(void *vdst, void *vsrc, int n)
{
	char *dst, *src;
	
	dst = vdst;
	src = vsrc;
	while(n-- > 0)
		*dst++ = *src++;
	return vdst;
}
*/

void*
memmove(void *dst, void *src, int n)
{
	char *pdst = dst;
	char *psrc = src;
	for(int i = 0; i < n; i++)
		*(pdst+i) = *(psrc+i);
	return dst;
}

// rerrstr and werrstr are the wrappers around the errstr syscall that
// helps make the userland interface act like the kernel inferface.
int
rerrstr(char *buf, int nbytes)
{
	return errstr(0, buf, nbytes);
}

int
werrstr(char *buf, int nbytes)
{
	return errstr(1, buf, nbytes);
}

char*	// untested
itoa(u64int d, int base)
{
	char *s, *r, *se, *sp;
	int i;

	s = mallocz(256);
	se = &s[254];  // some padding for safety
	if(d == 0){
		s[0] = '0';
		r = mallocz(strlen(s)+1);
		strcpy(r, s);
		free(s);
		return r;
	}
	if(base != 10)
	if(base != 16){
		free(s);
		seterr("invalid base (should be 10 or 16)");
		return nil;
	}
	while(d){
		*--se = "0123456789abcdef"[d%base];
		d = d/base;
	}
	sp = s;
	while(*sp == '\0')
		sp++;
	r = mallocz(strlen(sp)+1);
	strcpy(r, sp);
	free(s);
	return r;
}

int
rfperms(char *fname)
{
	return chperms(fname, 0, 0);
}

int
wfperms(char *fname, int perms)
{
	return chperms(fname, 1, perms);
}
