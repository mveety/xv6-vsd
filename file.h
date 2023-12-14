struct file {
	enum { FD_NONE, FD_PIPE, FD_INODE } type;
	int ref; // reference count
	char readable;
	char writable;
	struct pipe *pipe;
	struct inode *ip;
	uint off;
};


// in-memory copy of an inode
struct inode {
	uint dev;           // Device number
	uint inum;          // Inode number
	int ref;            // Reference count
	int flags;          // I_BUSY, I_VALID

	short type;         // copy of disk inode
	short major;
	short minor;
	short owner;		// owner of the inode
	uchar perms;		// owner permissions (read, write, exec, hidden)
	uchar attrib;		// optional attributes
	short nlink;
	uint size;
	uint addrs[NDIRECT];
};

// state information
#define I_BUSY 0x1
#define I_VALID 0x2

// user permissions
#define U_READ (1<<0)
#define U_WRITE (1<<1)
#define U_EXEC (1<<2)
#define U_HIDDEN (1<<3)
// world permissions
#define W_READ (1<<4)
#define W_WRITE (1<<5)
#define W_EXEC (1<<6)
#define W_HIDDEN (1<<7)

// file operations
#define OP_READ		1
#define OP_WRITE	2
#define OP_EXEC		3
#define OP_OPEN		4
#define OP_UNLINK	5
#define OP_LINK		6
#define OP_CREATE	7

// table mapping major device number to
// device functions
struct devsw {
	int (*read)(struct inode*, char*, int, int);
	int (*write)(struct inode*, char*, int, int);
	int (*control)(struct inode*, int, void*, int);
};

extern struct devsw devsw[];
extern struct inode *rootdir;

#define CONSOLE 1

// Helper functions
int checkfileperm(struct file*, short);
int checkinodeperm(struct inode*, short);

