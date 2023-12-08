#ifndef ARGS_H
# define ARGS_H

extern int	args(int argc, char *argv[], int *flags, char *options);
extern char	*argf(int argc, char *argv[], char c, void (*fn)(char*));
extern int	lastarg(int argc, char *argv[]);

#endif
