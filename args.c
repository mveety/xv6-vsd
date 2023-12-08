#include <libc.h>

int
findinstring(char c, char *str)
{
	int i;

	for(i = 0; *str; i++, str++)
		if(c == *str)
			return i;
	return -1;
}

int
args(int argc, char *argv[], int *flags, char *options)
{
	int i[2];
	int fg;
	char c;
	int args;

	for(i[0] = 0, args = 0; i[0] < argc; i[0]++){
		if (*argv[i[0]] == '-'){
			i[1] = 1;
			while(*(argv[i[0]] + i[1]) != 0 || *(argv[i[0]] + i[1]) != ' '){
				c = *(argv[i[0]] + (i[1]++));
				fg = findinstring(c, options);
				if(c == 0)
					break ;
				if(fg == -1)
					return (c == '-' ? i[0] : -1);
				flags[fg] = 1;
				args = i[0];
			}
		}
	}
	return args;
}

char
*argf(int argc, char *argv[], char c)
{
	int i;

	for(i = 0; i < argc; i++){
		if(*argv[i] == '-' && *(argv[i] + 1) == '-')
			break ;
		if(*argv[i] == '-' && *(argv[i] + 1) == c){
			if(argv[i + 1] == nil)
				return nil;
			if(i + 1 < argc && *argv[i + 1] != '-')
				return argv[i + 1];
			else
				return nil;
		}
	}
	return nil;
}

int
lastarg(int argc, char *argv[], char *opts)
{
	int i = 0;
	int args = 0;
	int nextgood = 0;

	for(i = 0, args = 0; i < argc; i++){
		if(*argv[i] == '-'){
			args = i;
			if(*(argv[i] + 1) == '-')
				break ;
			if(findinstring(*(argv[i] +1), opts) >= 0)
				nextgood = 1;
		} else if(nextgood){
			args = i;
			nextgood = 0;
		}
	}
	return args;
}
