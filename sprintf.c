#include <libc.h>

char*
swriteint(int xx, int base, int sgn)
{
	static char digits[] = "0123456789ABCDEF";
	char buf[16];
	char obuf[17];
	int i = 0, j = 0, neg = 0;
	uint x;

	if(sgn && xx < 0){
		neg = 1;
		x = -xx;
	} else {
		x = xx;
	}
	
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);
	if(neg)
		buf[i++] = '-';

	while(--i >= 0){
		obuf[j] = buf[i];
		j++;
	}
	obuf[j] = 0;
	return strdup(obuf);
}

char*
extendstring(char *in, int nlen)
{
	int curlen;
	int olen;
	char *ostr;

	curlen = strlen(in)+1;
	olen = curlen + nlen;
	if(!(ostr = mallocz(olen)))
		return nil;
	memcpy(ostr, in, curlen);
	free(in);
	return ostr;
}

char*
oputc(char c, char *obuf, int oi, uint *olen)
{
	// yay side effects!
	// i'm a bad person, don't mention it.
	char *nobuf;
	
	if(oi < *olen - 1){
		obuf[oi++] = c;
		return obuf;
	}
	nobuf = extendstring(obuf, 32);
	if(nobuf == nil)
		return nil;
	*olen += 32;
	return oputc(c, nobuf, oi, olen);
}

char*
sprintf(char *fmt, ...)
{
	char *obuf;
	char *s;
	char errbuf[256];
	char *tmp;
	int c, i, oi, state, k;
	uint olen;
	uint *ap;

	if(!(obuf = mallocz(256)))
		return nil;
	olen = 256;
	oi = 0;
	state = 0;
	memset(errbuf, 0, 256);
	ap = (uint*)(void*)&fmt + 1;
	for(i = 0; fmt[i]; i++){
		c = fmt[i] & 0xff;
		if(state == 0){
			if(c == '%'){
				state = '%';
			} else {
				obuf = oputc(c, obuf, oi, &olen);
				oi++;
			}
		} else if(state == '%'){
			if(c == 'd'){
				tmp = swriteint(*ap, 10, 1);
				for(k = 0; k < strlen(tmp); k++){
					obuf = oputc(tmp[k], obuf, oi, &olen);
					oi++;
				}
				free(tmp);
				ap++;
			} else if(c == 'u'){
				tmp = swriteint(*ap, 10, 0);
				for(k = 0; k < strlen(tmp); k++){
					obuf = oputc(tmp[k], obuf, oi, &olen);
					oi++;
				}
				free(tmp);
				ap++;
			} else if(c == 'x' || c == 'p'){
				tmp = swriteint(*ap, 16, 0);
				for(k = 0; k < strlen(tmp); k++){
					obuf = oputc(tmp[k], obuf, oi, &olen);
					oi++;
				}
				free(tmp);
				ap++;
			} else if(c == 's'){
				s = (char*)*ap;
				ap++;
				if(s == 0)
					s = "(null)";
				while(*s != 0){
					obuf = oputc(*s, obuf, oi, &olen);
					s++;
					oi++;
				}
			} else if(c == 'r'){
				if(rerrstr(errbuf, 0))
					rerrstr(errbuf, 256);
				for(int i = 0; errbuf[i] != '\0'; i++){
					obuf = oputc(errbuf[i], obuf, oi, &olen);
					oi++;
				}
				memset(errbuf, 0, 256);
			} else if(c == 'c'){
				obuf = oputc(*ap, obuf, oi, &olen);
				ap++;
				oi++;
			} else {
				obuf = oputc('%', obuf, oi, &olen);
				oi++;
				obuf = oputc(c, obuf, oi, &olen);
				oi++;
			}
			state = 0;
		}
	}

	tmp = strdup(obuf);
	free(obuf);
	return tmp;
}
