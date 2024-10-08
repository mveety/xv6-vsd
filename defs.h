struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct rtcdate;
struct spinlock;
struct stat;
struct superblock;
struct Message;

typedef struct Message Message;

#define INPUT_BUF 128
struct inputbuf {
	char buf[INPUT_BUF];
	void (*iputc)(int);
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
};

extern int halted;
extern int sysuart;
extern int syscons;

// bio.c
void            binit(void);
struct buf*     bread(uint, uint);
void            brelse(struct buf*);
void            bwrite(struct buf*);

// console.c
void            consoleinit1(void);
void            consoleinit2(void);
void            cprintf(char*, ...);
void            consoleintr(int(*)(void), struct inputbuf*);
void            panic(char*) __attribute__((noreturn));
void			cgamove(int, int);
void			cgaputc(int);
void			cgaprintstr(char*);

// exec.c
int             exec(char*, char**);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file*, char*, int n);
int             filestat(struct file*, struct stat*);
int             filewrite(struct file*, char*, int n);
int				fileseek(struct file*, int n, int dir);

// fs.c
void            readsb(int dev, struct superblock *sb);
int     dirlink(struct inode*, char*, uint);
struct inode*   dirlookup(struct inode*, char*, uint*);
struct inode*   ialloc(uint, short);
struct inode*   iget(uint, uint);
struct inode*   idup(struct inode*);
void			fsinit(int dev);
void            iinit(int dev);
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void		iref(struct inode*);
void		iunref(struct inode*);
void            iunlockput(struct inode*);
void			iunlockput_direct(struct inode*);
int		imount(struct inode*, struct inode*);
void		iunmount(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
struct inode*   namei_direct(char*);
struct inode*   nameiparent_direct(char*, char*);
int             readi(struct inode*, char*, uint, uint);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, char*, uint, uint);

// ide.c
void            ideinit(void);
void            ideintr(void);
void            iderw(struct buf*);

// ioapic.c
void            ioapicenable(int irq, int cpu);
extern uchar    ioapicid;
void            ioapicinit(void);

// kalloc.c
char*           kalloc(void);
void            kfree(char*);
void            kinit1(void*, void*);
void            kinit2(void*, void*);
void			kmfree(void*);
void*			kmalloc(uint);
void*			kmallocz(uint);

// kbd.c
void            kbdintr(void);

// lapic.c
void            cmostime(struct rtcdate *r);
int             cpunum(void);
extern volatile uint*    lapic;
int				lapicid(void);
void            lapiceoi(void);
void            lapicinit(void);
void            lapicstartap(uchar, uint);
void            microdelay(int);

// log.c
void            initlog(uint dev);
void            log_write(struct buf*);
void            begin_op(uint dev);
void            end_op(uint dev);

// mp.c
extern int      ismp;
int             mpbcpu(void);
void            mpinit(void);
void            mpstartthem(void);

// mux.c
void			muxinit(void);
void			muxintr(void);

// picirq.c
void            picenable(int);
void            picinit(void);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, char*, int);
int             pipewrite(struct pipe*, char*, int);

//PAGEBREAK: 16
// proc.c
int				cpuid(void);
struct cpu*		mycpu(void);
struct proc*    copyproc(struct proc*);
void            exit(void);
int             fork(void);
int				tfork(void);
int				clone(uint);
int             growproc(int);
int             kill(int);
void            pinit(void);
void            procdump(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            sleep(void*, struct spinlock*);
void            userinit(void);
void			kernelinit(void);
int             wait(void);
void            wakeup(void*);
void            yield(void);
// this function writes to the per-process error string buffer
int             werrstr(char*, int);
// this function reads from the per-process error string buffer
// a call to rerrstr with uint 0 and char nil will return 1 if the
// errstr has been set since last call to rerrstr
int             rerrstr(char*, int);
struct proc*    findpid(int);
uint            precvwait(void);
Message*        precvmsg(void);
int             psendmsg(struct proc*, Message*);

// swtch.S
void            swtch(struct context**, struct context*);

// spinlock.c
void            acquire(struct spinlock*);
void            getcallerpcs(void*, uint*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            pushcli(void);
void            popcli(void);

// string.c
int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memcpy(void*, const void*, uint);
void*           memset(void*, int, uint);
char*           safestrcpy(char*, const char*, int);
int             strncmp(const char*, const char*, uint);
char*           strncpy(char*, const char*, int);
char*           strdup(char*);
int strcmp(const char*, const char*);
int strlen(char*);

// syscall.c
int             argint(int, int*);
int             argptr(int, char**, int);
int             argstr(int, char**);
int             fetchint(uint, int*);
int             fetchstr(uint, char**);
void            syscall(void);

// timer.c
void            timerinit(void);

// trap.c
void            idtinit(void);
extern uint     ticks;
void            tvinit(void);
extern struct spinlock tickslock;

// uart.c
void		earlyuartputc(char);
void		earlyuartprintstr(char*);
void            uartinit(void);
void            uartintr(void);
void            uartputc(int);

// vm.c
void            seginit(void);
void            kvmalloc(void);
void            vmenable(void);
pde_t*          setupkvm(void);
char*           uva2ka(pde_t*, char*);
int             allocuvm(pde_t*, uint, uint);
int             deallocuvm(pde_t*, uint, uint);
void            freevm(pde_t*);
void            inituvm(pde_t*, char*, uint);
int             loaduvm(pde_t*, char*, struct inode*, uint, uint);
pde_t*          copyuvm(pde_t*, uint);
void            switchuvm(struct proc*);
void            switchkvm(void);
int             copyout(pde_t*, uint, void*, uint);
void            clearpteu(pde_t *pgdir, char *uva);

// bitbucket.c
void bitbucket_init(void);

// sysctl.c
void sysctl_init(void);
void sysname_init(void);

// syschroot.c
char* vroot2root(char*);
int chroot(char*);

// sysauth.c
// initializes the auth subsystem
int authinit(void);
// adds user (uid, name, hashed password)
int adduser(int, char*, int);
// deletes a user (uid)
int deluser(int);
// checks a user (uid, hashed password)
int checkuser(int, int);
// generates a password hash (password str, returned hash)
int hashpass(char*, int*);
// converts uid to uname (uid, name, sizeof(name))
int uid2name(int, char*, int);
// converts uname to uid (uname, returned uid)
int name2uid(char*, int*);

// disk.c
// is the device registered
int realdisk_exists(uint);
// get the driver specific device id
uint realdisk(uint);
// get and set driver specific flags
u16int *diskflags(uint);
// manage disks
uint register_disk(int, uint, void (*realrw)(struct buf*), void*);
int unregister_disk(uint);
// the method
void diskrw(struct buf*);
// mark disks as mounted/unmounted
int diskmount0(uint, struct superblock*);
int diskmount1(uint, struct inode*);
int diskmount(uint, struct inode*, struct superblock*);
void diskunmount(uint);
int diskmounted(uint);
struct inode *diskroot(uint);
struct superblock *getsuperblock(uint);
struct log *getlog(uint);
int disktype(uint);


// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))
// for 9front compatibility
#define nelem(x) (sizeof(x)/sizeof((x)[0]))

// nil because its just better.
#define nil (void*)0

#define Lock(a) acquire(&a)
#define Unlock(a) release(&a)
