extern int singleuser;

// syscalls
// changes process's user
int chuser(int);
// changes process's visible system
int chsystem(int);
// toggles single user mode
int disallow(void);

// helpers
int checksysperms(struct proc*, struct proc*);


