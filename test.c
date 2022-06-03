#include <stdio.h>

#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#if 1
	#include "malloc.h"
#else
	#include <stdlib.h>
#endif

void	*test_malloc(size_t n)
{
	printf("\033[92;1mmalloc\033[0m(\033[93m%zu\033[0m)", n); fflush(stdout);
	void	*ptr = malloc(n);
	printf(" -> \033[94;1mptr\033[0m(\033[93m%p\033[0m)", ptr); fflush(stdout);
	for (size_t i = 0; i < n; i++)
		((char *)ptr)[i] = i;
	printf(" \033[92m[memcheck]\033[0m\n");
	return (ptr);
}
#define malloc test_malloc

void	test_free(void *ptr)
{
	printf("\033[91;1mfree\033[0m(\033[93m%p\033[0m)\n", ptr);
	free(ptr);
}
#define free test_free

void	*test_realloc(void *ptr, size_t n)
{
	printf("\033[94;1mrealloc\033[0m(\033[93m%p, %zu\033[0m)", ptr, n); fflush(stdout);
	void	*new_ptr = realloc(ptr, n);
	printf(" -> \033[94;1mptr\033[0m(\033[93m%p\033[0m)", new_ptr); fflush(stdout);
	for (size_t i = 0; i < n; i++)
		((char *)new_ptr)[i] = i;
	printf(" \033[92m[memcheck]\033[0m\n");
	return (new_ptr);
}
#define realloc test_realloc

int	main()
{
	puts("╔═════════════════════╗");
	puts("║   Malloc tests 🐘   ║");
	puts("╚═════════════════════╝");
	puts("");

	void	*ptr[4];

	ptr[0] = malloc(1);
	ptr[1] = malloc(3);
	ptr[2] = malloc(4);
	ptr[3] = malloc(12459);

	puts("Test free");
	free(ptr[0]);
	free(ptr[1]);
	free(ptr[2]);
	free(ptr[3]);

	puts("Realloc on same spot");
	ptr[0] = malloc(52);
	free(ptr[0]);

	puts("Allocate multiple pages");
	ptr[0] = malloc(getpagesize() * 1000);
	printf("malloc 1000 pages:  %p\n", ptr[0]);
	ptr[0] = realloc(ptr[0], 10);
	printf("realloc 10 bytes:   %p\n", ptr[0]);
	ptr[0] = realloc(ptr[0], getpagesize() * 1000);
	printf("realloc 1000 pages: %p\n", ptr[0]);

	// free(alloc(2));
	// void *p1 = alloc(2);
	// void *p2 = alloc(0);
	// void *p3 = alloc(4);
	// void *ptr = alloc(8);
	// ptr = realloc(ptr, 12);
	// ptr = realloc(ptr, 2);
	// ptr = realloc(ptr, 32);
	// ptr = realloc(ptr, 64);
	// ptr = realloc(ptr, 8004);
	// ptr = realloc(ptr, 2);
	// ptr = realloc(ptr, 8004);
	// free(alloc(10));
	// free(p1);
	// free(p2);
	// free(p3);
	// free(ptr);

	// printf("-- realloc --\n");
	// ptr = realloc(malloc(1), 8191-8);
	// ptr = realloc(malloc(1), 8192-8);
	// ptr = realloc(malloc(1), 8193-8);
	// printf("-- realloc end --\n");

	// show_alloc_mem();

	puts("");
	puts("╔═════════════════════════╗");
	puts("║   Malloc tests end 🐘   ║");
	puts("╚═════════════════════════╝");
}