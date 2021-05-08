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
	short perms;		// owner permissions (read, write, exec, hidden)
	short nlink;
	uint size;
	uint addrs[NDIRECT];
};

// state information
#define I_BUSY 0x1
#define I_VALID 0x2
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
};

extern struct devsw devsw[];

#define CONSOLE 1

// Helper functions
int checkfileperm(struct file*, short);
int checkinodeperm(struct inode*, short);

