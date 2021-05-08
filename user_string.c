#include "libc.h"

void*
memset(void *dst, int c, uint n)
{
	char *bf = dst;
	for(int i = 0; i < n; i++)
		*(bf+i) = (char)c;
	return dst;
}

int
memcmp(void *v1, void *v2, uint n)
{
	const uchar *s1, *s2;
	
	s1 = v1;
	s2 = v2;
	while(n-- > 0){
		if(*s1 != *s2)
			return *s1 - *s2;
		s1++, s2++;
	}

	return 0;
}

// memcpy exists to placate GCC.  Use memmove.
void*
memcpy(void *dst, void *src, uint n)
{
	return memmove(dst, src, n);
}

int
strncmp(char *p, char *q, uint n)
{
	while(n > 0 && *p && *p == *q)
		n--, p++, q++;
	if(n == 0)
		return 0;
	return (uchar)*p - (uchar)*q;
}

char*
strncpy(char *s, char *t, int n)
{
	char *os;
	
	os = s;
	while(n-- > 0 && (*s++ = *t++) != 0)
		;
	while(n-- > 0)
		*s++ = 0;
	return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char*
strlcpy(char *s, char *t, int n)
{
	char *os;
	
	os = s;
	if(n <= 0)
		return os;
	while(--n > 0 && (*s++ = *t++) != 0)
		;
	*s = 0;
	return os;
}

char*
strdup(char *s)
{
	int slen = strlen(s);
	char *duped = malloc(slen+2);
	if(duped == nil){
		seterr(EKNOMEM);
		return nil;
	}
	memset(duped, 0, slen+2);
	strcpy(duped, s);
	return duped;
}

