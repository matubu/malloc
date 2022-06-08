#include <stdio.h>

#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <time.h>

#if 1
	#include "malloc.h"
#else
	#include <stdlib.h>
	#define show_alloc_mem() puts("\033[91;1m[[real malloc mode]]\033[0m");
#endif

void	*t_malloc(size_t n)
{
	printf("\033[92;1mmalloc\033[0m(\033[93m%zu\033[0m)", n); fflush(stdout);
	void	*ptr = malloc(n);
	printf(" -> \033[94;1mptr\033[0m(\033[93m%p\033[0m)", ptr); fflush(stdout);
	for (size_t i = 0; i < n; i++)
		((char *)ptr)[i] = i;
	printf(" \033[92m[memcheck]\033[0m\n");
	return (ptr);
}

void	t_free(void *ptr)
{
	printf("\033[91;1mfree\033[0m(\033[93m%p\033[0m)\n", ptr);
	free(ptr);
}

void	*t_realloc(void *ptr, size_t n)
{
	printf("\033[94;1mrealloc\033[0m(\033[93m%p, %zu\033[0m)", ptr, n); fflush(stdout);
	void	*new_ptr = realloc(ptr, n);
	printf(" -> \033[94;1mptr\033[0m(\033[93m%p\033[0m)", new_ptr); fflush(stdout);
	for (size_t i = 0; i < n; i++)
		((char *)new_ptr)[i] = i;
	printf(" \033[92m[memcheck]\033[0m\n");
	return (new_ptr);
}

int	main()
{
	puts("╔═════════════════════╗");
	puts("║   Malloc tests 🐘   ║");
	puts("╚═════════════════════╝");
	puts("");

	void	*ptr[4];

	ptr[2] = t_malloc(31);
	ptr[2] = t_malloc(33);
	ptr[0] = t_malloc(0);
	ptr[2] = t_malloc(32);
	ptr[1] = t_malloc(3);
	ptr[2] = t_malloc(16);
	ptr[3] = t_malloc(12459);

	show_alloc_mem();

	puts("Test free");
	t_free(ptr[0]);
	t_free(ptr[1]);
	t_free(ptr[2]);
	t_free(ptr[3]);

	puts("Realloc on same spot");
	ptr[0] = t_malloc(0);
	ptr[1] = t_malloc(3);
	ptr[2] = t_malloc(16);
	ptr[3] = t_malloc(12459);

	show_alloc_mem();

	t_free(ptr[0]);
	t_free(ptr[1]);
	t_free(ptr[2]);
	t_free(ptr[3]);

	show_alloc_mem();

	puts("Allocate multiple pages");
	ptr[0] = t_malloc(getpagesize() * 1000);
	printf("malloc 1000 pages:  %p\n", ptr[0]);
	ptr[0] = t_realloc(ptr[0], 10);
	printf("realloc 10 bytes:   %p\n", ptr[0]);
	ptr[0] = t_realloc(ptr[0], getpagesize() * 1000);
	printf("realloc 1000 pages: %p\n", ptr[0]);

	show_alloc_mem();

	puts("Realloc tiny address");
	ptr[0] = t_malloc(1);
	ptr[0] = t_realloc(ptr[0], 32);
	ptr[0] = t_realloc(ptr[0], 1);
	ptr[0] = t_realloc(ptr[0], 128);

	puts("malloc a lot");
	// printf("malloc %p\n", malloc(10000000000000));
	printf("malloc %p\n", malloc(1000000000000));

	// clock_t	start = clock();
	// for (int i = 0; i < 3907; ++i)
	// {
	// 	char	*char_ptr[256];
	// 	for (size_t j = 0; j < 256; ++j)
	// 		char_ptr[j] = malloc(1);
	// 	for (size_t j = 0; j < 256; ++j)
	// 		free(char_ptr[j]);
	// }
	// printf("one million allocation: %lfms\n", (double)(clock() - start) / CLOCKS_PER_SEC * 1000);

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

	show_alloc_mem();

	puts("");
	puts("╔═════════════════════════╗");
	puts("║   Malloc tests end 🐘   ║");
	puts("╚═════════════════════════╝");
}