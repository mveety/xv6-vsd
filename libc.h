#ifndef _libc_h
#define _libc_h

// error messages
#include "errstr.h"

// userspace only error messages
#define EULOCKHELD "proc already holds lock"
#define EUNOTHELD "no one holds the lock"
#define EUALLOCFAIL "memory allocation failed"

// from types.h
#define nil (void*)0
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
typedef unsigned char u8int;
typedef unsigned short u16int;
typedef unsigned int u32int;
typedef unsigned long u64int;
typedef char s8int;
typedef short s16int;
typedef int s32int;
typedef long s64int;
typedef u8int byte;
// on 386+ a word is 16 bits even though its a 32 bit processor
typedef u16int word;
typedef u32int dword;
typedef u64int qword;

// from stat.h
#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

struct stat {
	short type;  // Type of file
	int dev;     // File system's disk device
	uint ino;    // Inode number
	short nlink; // Number of links to file
	uint size;   // Size of file in bytes
	short owner; // who owns me?
	short perms; // what is he allowed to do?
};

// from fcntl.h
#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define OREAD O_RDONLY
#define OWRITE O_WRONLY
#define OCREATE O_CREATE

// from fs.h
#define ROOTINO 1
#define BSIZE 512
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NINDIRECT * 12)
#define DIRSIZ 14

struct dirent {
	ushort inum;
	char name[DIRSIZ];
};

// from user.h
struct rtcdate;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char*, int);
int mknod(char*, short, short);
int unlink(char*);
int fstat(int fd, struct stat*);
int link(char*, char*);
int mkdir(char*);
int chdir(char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int chuser(int);
int chsystem(int);
int disallow(void);
int getuid(void);
int getsid(void);
int getts(void);
int chroot(char*);
int errstr(int, char*, int);
int chperms(char*, int, int);
int tfork(void);
int seek(int, int, int);
int clone(uint);
int sendmsg(int, void*, int);
int recvmsg(void*, int);
int recvwait(void);

// ulib.c
int stat(char*, struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, char*, ...);
char* gets(char*, int max);
char *fgets(int, char*, int);
uint strlen(char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int rerrstr(char*, int);
int werrstr(char*, int);
int rfperms(char*);
int wfperms(char*, int);


// user permissions
#define U_READ 0x0001
#define U_WRITE 0x0002
#define U_EXEC 0x0004
#define U_HIDDEN 0x0008
// world permissions
#define W_READ 0x0100
#define W_WRITE 0x0200
#define W_EXEC 0x0400
#define W_HIDDEN 0x0800

/* vsd additions */

struct Lock {
	uint val;
	char *name;
	int pid;
};

// other typedefs
typedef struct dirent Dirent;
typedef struct stat Stat;
typedef struct RTCtime RTCtime;
typedef struct Lock Lock;

// ulib.c
char* itoa(u64int, int);
// user_string.c
void *memset(void*, int, uint);
int memcmp(void*, void*, uint);
void* memcpy(void*, void*, uint);
int strncmp(char*, char*, uint);
char* strncpy(char*, char*, int);
char* strlcpy(char*, char*, int);
char* strdup(char*);

// umalloc.c
void* mallocz(uint);	// malloc and zero memory

// ulocks.c
Lock *makelock(char*);
void freelock(Lock*);
int lock(Lock*);
int unlock(Lock*);
int canlock(Lock*);
int canunlock(Lock*);

// uthreads.c
extern int _threads_useclone; //defaults to 0
int spawn(void (*entry)(void*), void*);

// sprintf.c
char *sprintf(char*, ...);

// message.c
typedef struct Mailbox Mailbox;
typedef struct Message Message;

struct Mailbox {
	uint messages;
	uint looped;
	Message *head;
	Message *cur;
};

struct Message {
	int sentinel;
	uint len;
	void *payload;
	Message *next;
};

void send(int, int, void*, int);
Message* receive(Mailbox*);
void selectmsg(Mailbox*, Message*);
int freemsg(Message*);
void flush(Mailbox*);
Mailbox *mailbox(void);

#endif
