// On-disk file system format. 
// Both the kernel and user programs use this header file.


#define ROOTINO 1  // root i-number
// i bumped the block size from 512 up to 1024
#define BSIZE 512  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks | free bit map | data blocks ]
//
// mkfs computes the super block and builds an initial file system. The super describes
// the disk layout:
struct superblock {
	char label[11];    // filesystem label (vsd additions)
	uchar version;     // filesystem version (vsd additions)
	uchar bootable;	   // does this filesystem have a kernel (vsd additions)
	uchar system;      // does this filesystem have the OS (vsd additions)
	uint size;         // Size of file system image (blocks)
	uint nblocks;      // Number of data blocks
	uint ninodes;      // Number of inodes.
	uint nlog;         // Number of log blocks
	uint logstart;     // Block number of first log block
	uint inodestart;   // Block number of first inode block
	uint bmapstart;    // Block number of first free map block
	uint bootinode;    // what inode is the loader (vsd additions)
	uint kerninode;    // what inode is the kernel (vsd additions)
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NINDIRECT * 12)

// On-disk inode structure
struct dinode {
	short type;           // File type
	short major;          // Major device number (T_DEV only)
	short minor;          // Minor device number (T_DEV only)
	short owner;		// owner of the inode	
	short perms;		// owner permissions (read, write, exec, hidden)
	short nlink;          // Number of links to inode in file system
	uint size;            // Size of file (bytes)
	uint addrs[NDIRECT];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
	ushort inum;
	char name[DIRSIZ];
};

