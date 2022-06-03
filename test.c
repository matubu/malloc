#include <stdio.h>

#if 42
	#include "malloc.h"
#else
	#include <stdlib.h>
#endif

void	*alloc(size_t n)
{
	void	*ptr = malloc(n);

	printf("alloc(%zu) -> %p\n", n, ptr);
	for (size_t i = 0; i < n; i++)
	{
		((char *)ptr)[i] = i;
		printf("[%zu] = %d\n", i, ((char *)ptr)[i]);
	}
	return (ptr);
}

int	main()
{
	free(alloc(2));
	alloc(2);
	alloc(0);
	alloc(4);
	void *ptr = alloc(8);
	realloc(ptr, 12);
	realloc(ptr, 2);
	realloc(ptr, 32);
	realloc(ptr, 64);
	ptr = realloc(ptr, 8004);
	realloc(ptr, 2);
	realloc(ptr, 8004);
	free(alloc(10));

	show_alloc_mem();
}