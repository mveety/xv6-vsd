#include "libc.h"

char*
fmtnamen(char *path, char *buf, int buflen)
{
	char *p;
	int namelen;

	// find first character after the last slash
for(p=path+strlen(path); p >= path && *p != '/'; p--)
		;
	p++;

	// return the blank padded name
	if((namelen = strlen(p)) >= buflen)
		return p;
	memmove(buf, p, namelen);
	memset(buf+namelen, ' ', buflen - namelen);
	return buf;
}

char* // the buffer needs to be longer than 16 bytes
fmtpermsn(int perms, char *buf, int buflen)
{
//	"U:rwxh,W:rwxh"

	int i = 0;

	if(buflen < 16)
		return nil;
	memset(buf, 0, buflen);

	buf[i++] = 'U';
	buf[i++] = ':';

	if((perms&U_READ) == U_READ)
		buf[i++] = 'r';
	else
		buf[i++] = '-';
	if((perms&U_WRITE) == U_WRITE)
		buf[i++] = 'w';
	else
		buf[i++] = '-';
	if((perms&U_EXEC) == U_EXEC)
		buf[i++] = 'x';
	else
		buf[i++] = '-';
	if((perms&U_HIDDEN) == U_HIDDEN)
		buf[i++] = 'h';
	else
		buf[i++] = '-';

	buf[i++] = ',';
	buf[i++] = 'W';
	buf[i++] = ':';

	if((perms&W_READ) == W_READ)
		buf[i++] = 'r';
	else
		buf[i++] = '-';
	if((perms&W_WRITE) == W_WRITE)
		buf[i++] = 'w';
	else
		buf[i++] = '-';
	if((perms&W_EXEC) == W_EXEC)
		buf[i++] = 'x';
	else
		buf[i++] = '-';
	if((perms&W_HIDDEN) == W_HIDDEN)
		buf[i++] = 'h';
	else
		buf[i++] = '-';

	return buf;
}

char*
fmtfieldn(char *data, int fieldsize, char filler, char *buf, int bufsize)
{
	int datasize, wrapper;
	int i, j;

	datasize = strlen(data);
	memset(buf, 0, bufsize);

	if(datasize > fieldsize){
		strncpy(buf, data, datasize < bufsize ? datasize : bufsize - 1);
		return buf;
	}

	wrapper = fieldsize - datasize;
	for(i = 0; i < (wrapper/2); i++)
		buf[i] = filler;

	for(j = 0; i < bufsize && j < datasize; i++, j++)
		buf[i] = data[j];

	for(; i < fieldsize; i++)
		buf[i] = filler;

	return buf;
}

