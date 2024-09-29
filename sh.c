// Shell.
#include <libc.h>

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

// eventually I will check it from envvars.
// but i don't have them yet.
#define PATH "/bin/"
#define MAXARGS 10

int noprompt = 0;
int bootsh = 0;

struct cmd {
	int type;
};

struct execcmd {
	int type;
	char *argv[MAXARGS];
	char *eargv[MAXARGS];
};

struct redircmd {
	int type;
	struct cmd *cmd;
	char *file;
	char *efile;
	int mode;
	int append;
	int fd;
};

struct pipecmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

struct listcmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

struct backcmd {
	int type;
	struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);
char* addpath(char*);

int  // this is a very naive test for "string" equality.
streq(char *str0, char *str1)
{
	if(strlen(str0) != strlen(str1))
		return 0;
	for(int i = 0; i < strlen(str0); i++)
		if(str0[i] != str1[i])
			return 0;
	return 1;
}

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
	int p[2];
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct listcmd *lcmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;

	if(cmd == 0){
		exit();
		return;
	}
	
	switch(cmd->type){
	default:
		panic("runcmd");
		return;

	case EXEC:
		ecmd = (struct execcmd*)cmd;
		if(ecmd->argv[0] == 0)
			exit();
		exec(addpath(ecmd->argv[0]), ecmd->argv);
		printf(2, "exec %s failed: %r\n", ecmd->argv[0]);
		break;

	case REDIR:
		rcmd = (struct redircmd*)cmd;
		close(rcmd->fd);
		if(open(rcmd->file, rcmd->mode) < 0){
			printf(2, "open %s failed: %r\n", rcmd->file);
			exit();
		}
		if(rcmd->append)
			seek(rcmd->fd, 0, 2);
		if(rerrstr(nil, 0)){
			printf(2, "op failed: %r\n");
			exit();
		}
		runcmd(rcmd->cmd);
		break;

	case LIST:
		lcmd = (struct listcmd*)cmd;
		if(fork1() == 0)
			runcmd(lcmd->left);
		wait();
		runcmd(lcmd->right);
		break;

	case PIPE:
		pcmd = (struct pipecmd*)cmd;
		if(pipe(p) < 0)
			panic("pipe");
		if(fork1() == 0){
			close(1);
			dup(p[1]);
			close(p[0]);
			close(p[1]);
			runcmd(pcmd->left);
		}
		if(fork1() == 0){
			close(0);
			dup(p[0]);
			close(p[0]);
			close(p[1]);
			runcmd(pcmd->right);
		}
		close(p[0]);
		close(p[1]);
		wait();
		wait();
		break;
		
	case BACK:
		bcmd = (struct backcmd*)cmd;
		if(fork1() == 0)
			runcmd(bcmd->cmd);
		break;
	}
	exit();
	return;
}

int
getcmd(char *buf, int nbuf)
{
	if(!noprompt)
		printf(2, "$ ");
	memset(buf, 0, nbuf);
	gets(buf, nbuf);
	if(buf[0] == 0) // EOF
		return -1;
	return 0;
}

int
main(int argc, char *argv[])
{
	static char buf[100];
	int fd;
	char *args;
	int argslen = 0;
	
	if(argc > 1){
		args = argv[1];
		if(*args == '-')
			args++;
		argslen = strlen(args);
		if(argslen > 0){
			for(int i = 0; i < argslen; i++){
				switch(*args){
				case 's':  // silent (aka don't print the prompt)
					noprompt = 1;
					break;
				case 'b':  // boot (use /dev/bootcons instead of provided fds)
					bootsh = 1;
					break;
				case 'h':
				default:
					printf(2, "usage: sh [-bsh]\n");
					exit();
					break;
				}
				args++;
			}
		}
	}
	if(bootsh){ // this might be needed.
		while((fd = open("/dev/bootcons", O_RDWR)) >= 0){
			if(fd >= 3){
				close(fd);
				break;
			}
		}
	}
	// Read and run input commands.
	while(getcmd(buf, sizeof(buf)) >= 0){
		if(buf[0] == '#') // comment a line out
			continue;
		if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
			// Clumsy but will have to do for now.
			// Chdir has no effect on the parent if run in the child.
			buf[strlen(buf)-1] = 0;  // chop \n
			if(chdir(buf+3) < 0)
				printf(2, "cannot cd %s\n", buf+3);
			continue;
		}
		if(streq(buf, "exit\n")) {
			printf(2, "sh: ending\n");
			exit();
		}
		if(fork1() == 0)
			runcmd(parsecmd(buf));
		wait();
	}
	exit();
}

void
panic(char *s)
{
	printf(2, "%s\n", s);
	exit();
}

int
fork1(void)
{
	int pid;
	
	pid = fork();
	if(pid == -1)
		panic("fork");
	return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
	struct execcmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = EXEC;
	return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int append, int fd)
{
	struct redircmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = REDIR;
	cmd->cmd = subcmd;
	cmd->file = file;
	cmd->efile = efile;
	cmd->mode = mode;
	cmd->fd = fd;
	cmd->append = append;
	return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
	struct pipecmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = PIPE;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
	struct listcmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = LIST;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
	struct backcmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = BACK;
	cmd->cmd = subcmd;
	return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
	char *s;
	int ret;
	
	s = *ps;
	while(s < es && strchr(whitespace, *s))
		s++;
	if(q)
		*q = s;
	ret = *s;
	switch(*s){
	case 0:
		break;
	case '|':
	case '(':
	case ')':
	case ';':
	case '&':
	case '<':
		s++;
		break;
	case '>':
		s++;
		if(*s == '>'){
			ret = '+';
			s++;
		}
		break;
	default:
		ret = 'a';
		while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
			s++;
		break;
	}
	if(eq)
		*eq = s;
	
	while(s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return ret;
}

int
peek(char **ps, char *es, char *toks)
{
	char *s;
	
	s = *ps;
	while(s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
	char *es;
	struct cmd *cmd;

	es = s + strlen(s);
	cmd = parseline(&s, es);
	peek(&s, es, "");
	if(s != es){
		printf(2, "leftovers: %s\n", s);
		panic("syntax");
	}
	nulterminate(cmd);
	return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
	struct cmd *cmd;

	cmd = parsepipe(ps, es);
	while(peek(ps, es, "&")){
		gettoken(ps, es, 0, 0);
		cmd = backcmd(cmd);
	}
	if(peek(ps, es, ";")){
		gettoken(ps, es, 0, 0);
		cmd = listcmd(cmd, parseline(ps, es));
	}
	return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
	struct cmd *cmd;

	cmd = parseexec(ps, es);
	if(peek(ps, es, "|")){
		gettoken(ps, es, 0, 0);
		cmd = pipecmd(cmd, parsepipe(ps, es));
	}
	return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
	int tok;
	char *q, *eq;

	while(peek(ps, es, "<>")){
		tok = gettoken(ps, es, 0, 0);
		if(gettoken(ps, es, &q, &eq) != 'a')
			panic("missing file for redirection");
		switch(tok){
		case '<':
			cmd = redircmd(cmd, q, eq, O_RDONLY, 0, 0);
			break;
		case '>':
			cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 0, 1);
			break;
		case '+':  // >>
			cmd = redircmd(cmd, q, eq, O_WRONLY, 1, 1);
			break;
		}
	}
	return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
	struct cmd *cmd;

	if(!peek(ps, es, "("))
		panic("parseblock");
	gettoken(ps, es, 0, 0);
	cmd = parseline(ps, es);
	if(!peek(ps, es, ")"))
		panic("syntax - missing )");
	gettoken(ps, es, 0, 0);
	cmd = parseredirs(cmd, ps, es);
	return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
	char *q, *eq;
	int tok, argc;
	struct execcmd *cmd;
	struct cmd *ret;
	
	if(peek(ps, es, "("))
		return parseblock(ps, es);

	ret = execcmd();
	cmd = (struct execcmd*)ret;

	argc = 0;
	ret = parseredirs(ret, ps, es);
	while(!peek(ps, es, "|)&;")){
		if((tok=gettoken(ps, es, &q, &eq)) == 0)
			break;
		if(tok != 'a')
			panic("syntax");
		cmd->argv[argc] = q;
		cmd->eargv[argc] = eq;
		argc++;
		if(argc >= MAXARGS)
			panic("too many args");
		ret = parseredirs(ret, ps, es);
	}
	cmd->argv[argc] = 0;
	cmd->eargv[argc] = 0;
	return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
	int i;
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct listcmd *lcmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;

	if(cmd == 0)
		return 0;
	
	switch(cmd->type){
	case EXEC:
		ecmd = (struct execcmd*)cmd;
		for(i=0; ecmd->argv[i]; i++)
			*ecmd->eargv[i] = 0;
		break;

	case REDIR:
		rcmd = (struct redircmd*)cmd;
		nulterminate(rcmd->cmd);
		*rcmd->efile = 0;
		break;

	case PIPE:
		pcmd = (struct pipecmd*)cmd;
		nulterminate(pcmd->left);
		nulterminate(pcmd->right);
		break;
		
	case LIST:
		lcmd = (struct listcmd*)cmd;
		nulterminate(lcmd->left);
		nulterminate(lcmd->right);
		break;

	case BACK:
		bcmd = (struct backcmd*)cmd;
		nulterminate(bcmd->cmd);
		break;
	}
	return cmd;
}

char*
addpath(char *cmd)
{
	char *bf;
	char *path = PATH;
	int pathlen;
	int cmdlen;

	if(cmd[0] == '.')
		return cmd;
	else if(cmd[0] == '/')
		return cmd;
	pathlen = strlen(path);
	cmdlen = strlen(cmd);
	bf = malloc(pathlen+cmdlen);
	if(bf == nil){
		printf(2, "sh: error: unable to allocate memory\n");
		exit();
	}
	memmove(bf, path, pathlen);
	memmove(bf+pathlen, cmd, cmdlen);
	bf[pathlen+cmdlen] = '\0';
	return bf;
}

