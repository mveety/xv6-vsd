// hopefully most of these will be sysctls soon.
// though some might be loader vars (like nproc and shit)
#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU         24  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV        256  // maximum major device number
#define ROOTDEV       0  // device number of boot disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       5000  // size of file system in blocks
#define THREADMAX    32  // the maximum number of child threads
#define DISKMAX      32 // maximum number of disks we can track
#define COM1 0x3f8 // first serial port (for bootelf)