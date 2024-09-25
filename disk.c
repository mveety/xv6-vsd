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

#define DRIVER_INUSE 1

struct disk {
	u16int flags;
	u16int driver_flags;
	uint driver_device;
	void (*diskrw)(struct buf*);
	void *aux;
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
register_disk(uint realdisk, void (*realrw)(struct buf*), void *aux)
{
	int i;

	for(i = 0; i < DISKMAX; i++)
		if(!(disks[i].flags & DRIVER_INUSE)){
			disks[i].flags |= DRIVER_INUSE;
			disks[i].driver_device = realdisk;
			disks[i].diskrw = realrw;
			disks[i].aux = aux;
			return i;
		}
	// under normal use 0 will be allocated already as the boot disk.
	// if you get 0 it's an error.
	return 0;
}

void
diskrw(struct buf *b)
{
	disks[b->dev].diskrw(b);
}
