#include <libc.h>

int
main(int argc, char *argv[])
{
	int i;
	void *v[10];
	void *p;

	for(i = 0; i < 10; i++){
		v[i] = malloc(4096);
		printf(1, "no-free round %d: &v[%d] = %p\n", i, i, v[i]);
	}
	printf(1, "freeing previous round...\n");
	for(i = 0; i < 10; i++)
		free(v[i]);

	for(i = 0; i < 10; i++){
		p = malloc(4096);
		printf(1, "free 1 round %d: &p = %p\n", i, p);
		free(p);
	}
	for(i = 0; i < 10; i++){
		p = sbrk(4096);
		printf(1, "sbrk round %d: &p = %p\n", i, p);
		free(p);
	}
	for(i = 0; i < 10; i++){
		v[i] = malloc(4096);
		printf(1, "no-free 2 round %d: &v[%d] = %p\n", i, i, v[i]);
	}
	for(i = 0; i < 10; i++){
		p = malloc(4096);
		printf(1, "free 2 round %d: &p = %p\n", i, p);
		free(p);
	}
	for(i = 0; i < 10; i++){
		v[i] = malloc(4096);
		printf(1, "no-free 3 round %d: &v[%d] = %p\n", i, i, v[i]);
	}
	sleep(120);
	return 0;
}
