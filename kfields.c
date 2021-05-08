#include "types.h"
#include "defs.h"
#include "kfields.h"

char*
getnextfield(char *obf, char *inbf, int *iter, int *status)
{
	int i = 0;
	int ibf_len = strlen(inbf);

	if(inbf[0] == '\0'){
		*status = 1;
		return nil;
	}

	for(i = 0; i < ibf_len; i++){
		if(inbf[i] == '\0')
			break;
		if(inbf[i] == ' ')
			break;
		if(inbf[i] == '\n')
			break;
		if(inbf[i] == '\t')
			break;
		obf[i] = inbf[i];
	}
	obf[i] = '\0';
	if(i == 0)
		*status = 1;
	*status = 0;
	*iter += i+1;
	return inbf+*iter;
}

