#include <libc.h>

#define USERPERMS (U_READ|U_WRITE|U_EXEC|U_HIDDEN)
#define WORLDPERMS (W_READ|W_WRITE|W_EXEC|W_HIDDEN)

int
parseperms(char *strperms, int world)
{
	int perms = 0;
	int strpermslen;
	int i = 0;

	strpermslen = strlen(strperms);

	if(strperms[i] == 'r' && i < strpermslen){
		perms |= world ? W_READ : U_READ;
		i++;
	}
	if(strperms[i] == 'w' && i < strpermslen){
		perms |= world ? W_WRITE : U_WRITE;
		i++;
	}
	if(strperms[i] == 'x' && i < strpermslen){
		perms |= world ? W_EXEC : U_EXEC;
		i++;
	}
	if(strperms[i] == 'h' && i < strpermslen){
		perms |= world ? W_HIDDEN : U_HIDDEN;
		i++;
	}
	return perms;
}

int
main(int argc, char *argv[])
{
	int flags[] = {0, 0, 0, 0, 0, 0, 0};
	char argflags[] = "uwfmhvd";
	char *filename;
	char *userpermsstr;
	int userperms = 0;
	char *worldpermsstr;
	int worldperms = 0;
	int argsoutput;
	int oldperms;
	int newperms = 0;
	char permsbuf[17];
	
	argsoutput = args(argc, argv, &flags[0], &argflags[0]);
	userpermsstr = argf(argc, argv, 'u');
	worldpermsstr = argf(argc, argv, 'w');
	filename = argf(argc, argv, 'f');

	if(flags[4] || ((!flags[0] || !flags[1]) && !flags[2])){
		printf(2, "usage: %s [-mvd] [-u userperms] [-w worldperms] -f file\n", argv[0]);
		printf(2, "usage: %s [-h]\n", argv[0]);
		return 0;
	}
	oldperms = rfperms(filename);
	if(flags[0])
		userperms = parseperms(userpermsstr, 0);
	if(flags[1])
		worldperms = parseperms(worldpermsstr, 1);
	if(flags[3]){
		newperms = oldperms | userperms;
		newperms |= worldperms;
	} else {
		newperms |= userperms;
		newperms |= worldperms;
	}
//	if(flags[6]){
		printf(2, "%s: argsoutput = %d\n", argv[0], argsoutput);
		printf(2, "%s: userpermsstr = %s\n", argv[0], userpermsstr);
		printf(2, "%s: worldpermsstr = %s\n", argv[0], worldpermsstr);
		printf(2, "%s: userperms = %x\n", argv[0], userperms);
		printf(2, "%s: worldperms = %x\n", argv[0], worldperms);
		printf(2, "%s: oldperms = %x (%s)\n", argv[0], oldperms,
				fmtpermsn(oldperms, permsbuf, sizeof(permsbuf)));
		printf(2, "%s: newperms = %x (%s)\n", argv[0], newperms,
				fmtpermsn(newperms, permsbuf, sizeof(permsbuf)));
//	}
	if(wfperms(filename, newperms) < 0)
		printf(2, "%s: unable to write (%s) to %s\n",
				argv[0], fmtpermsn(newperms, permsbuf, sizeof(permsbuf)), filename);
	return 0;
}	

