#include "types.h"
#include "x86.h"
#include "newboot/smalloc.h"

char *heap;
uint hoffset;
uint heaplen;

void
smalloc_panic(void)
{
	for(;;)
		hlt();
}

void
smalloc_init(void *h, uint len)
{
	int i = 0;

	hoffset = 0;
	heaplen = len;
	heap = h;
	for(i = 0; i < heaplen; i++)
		heap[i] = (char)0;
}

void*
smalloc(uint sz)
{
	void *nptr;

	if(hoffset+sz >= heaplen)
		smalloc_panic();
	nptr = &heap[hoffset];
	hoffset += sz;
	return nptr;
}
