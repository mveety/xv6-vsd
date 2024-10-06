#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "fs.h"
#include "buf.h"
#include "errstr.h"
#include "file.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"

#define DRIVER_INUSE (1<<0)
#define DISK_MOUNTED (1<<1)
#define DISK_INITIALIZED (1<<2)

struct disk {
	u16int flags;
	u16int driver_flags;
	int type;
	uint driver_device;
	void (*diskrw)(struct buf*);
	void *aux;
	struct inode *mountroot;
	struct superblock sb;
	struct log *log;
};

struct disk disks[DISKMAX];

int
realdisk_exists(uint devid)
{
	if(devid >= DISKMAX)
		return -1;
	if(!(disks[devid].flags & DRIVER_INUSE))
		return -2;
	return 0;
}

uint
realdisk(uint devid)
{
	return disks[devid].driver_device;
}

u16int*
diskflags(uint devid)
{
	return &disks[devid].driver_flags;
}

uint
register_disk(int type, uint realdisk, void (*realrw)(struct buf*), void *aux)
{
	int i;

	for(i = 0; i < DISKMAX; i++)
		if(!(disks[i].flags & DRIVER_INUSE)){
			disks[i].flags |= DRIVER_INUSE;
			disks[i].driver_device = realdisk;
			disks[i].type = type;
			disks[i].diskrw = realrw;
			disks[i].aux = aux;
			return i;
		}
	// under normal use 0 will be allocated already as the boot disk.
	// if you get 0 it's an error.
	return 0;
}

int
disktype(uint devid)
{
	if(devid >= DISKMAX)
		return -1;
	if(!(disks[devid].flags & DRIVER_INUSE))
		return -2;
	return disks[devid].type;
}

void
diskrw(struct buf *b)
{
	disks[b->dev].diskrw(b);
}

int
diskmounted(uint disk)
{
	if(disks[disk].flags & DISK_MOUNTED)
		return 1;
	return 0;
}

int
diskmount0(uint disk, struct superblock *sb)
{
	if(diskmounted(disk))
		return -1;
	disks[disk].flags |= DISK_MOUNTED;
	memcpy(&disks[disk].sb, sb, sizeof(struct superblock));
	disks[disk].log = kmallocz(sizeof(struct log));
	return 0;
}

int
diskmount1(uint disk, struct inode *mountroot)
{
	if(!diskmounted(disk))
		return -1;
	disks[disk].mountroot = mountroot;
	disks[disk].flags |= DISK_INITIALIZED;
	return 0;
}

int
diskmount(uint disk, struct inode *mountroot, struct superblock *sb)
{
	if(diskmounted(disk))
		return -1;
	disks[disk].mountroot = mountroot;
	disks[disk].flags |= DISK_MOUNTED;
	memcpy(&disks[disk].sb, sb, sizeof(struct superblock));
	return 0;
}

void
diskunmount(uint disk)
{
	if(diskmounted(disk)){
		disks[disk].mountroot = nil;
		disks[disk].flags &= ~DISK_MOUNTED;
		disks[disk].flags &= ~DISK_INITIALIZED;
		kmfree(disks[disk].log);
	}
}

struct inode*
diskroot(uint disk)
{
	if(diskmounted(disk))
		return disks[disk].mountroot;
	return nil;
}

struct superblock*
getsuperblock(uint disk)
{
	if(diskmounted(disk))
		return &disks[disk].sb;
	return nil;
}

void
setsuperblock(uint disk, struct superblock *sb)
{
	if(diskmounted(disk))
		memcpy(&disks[disk].sb, sb, sizeof(struct superblock));
}

struct log*
getlog(uint disk)
{
	if(diskmounted(disk))
		return disks[disk].log;
	return nil;
}
