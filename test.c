#include <stdio.h>

#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <time.h>
#include "thread.h"

// - DEFRAGMENT
// - show_alloc_mem_ex
// - thread safety
// - full cleanup
// - safe malloc

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

#define SECTION(fmt, ...) \
	printf("\n::: \033[91m" fmt "\033[0m -->\n\n", ##__VA_ARGS__)

#define STRLEN(s) sizeof(s)
#define PUT(s) write(1, s, STRLEN(s));
#define PUTS(s) PUT(s "\n");

int	main()
{
	PUTS("╔═════════════════════╗");
	PUTS("║   Malloc tests 🐘   ║");
	PUTS("╚═════════════════════╝");
	PUTS("");

	SECTION("test defragment");

	void	*chunks[] = {
		t_malloc(8000),
		t_malloc(8000),
		t_malloc(8000)
	};

	for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); ++i)
		t_free(chunks[i]);

	show_alloc_mem();

	t_free(t_malloc(16000));

	show_alloc_mem();

	void	*nothing;
	void	*tiny_ptr[4];
	void	*small_ptr[4];
	void	*large_ptr[4];

	SECTION("malloc nothing");
	nothing = t_malloc(0);

	SECTION("tiny malloc");
	tiny_ptr[0] = t_malloc(1);
	tiny_ptr[1] = t_malloc(3);
	tiny_ptr[2] = t_malloc(16);
	tiny_ptr[3] = t_malloc(32);

	SECTION("small malloc");
	small_ptr[0] = t_malloc(33);
	small_ptr[1] = t_malloc(35);
	small_ptr[2] = t_malloc(120);
	small_ptr[3] = t_malloc(128);

	show_alloc_mem_ex();

	SECTION("large malloc");
	large_ptr[0] = t_malloc(129);
	large_ptr[1] = t_malloc(800);
	large_ptr[2] = t_malloc(1000);
	large_ptr[3] = t_malloc(100000);

	show_alloc_mem();

	const int	size[] = {0, 5, 35, 9000};
	const char	*types[] = {"nothing", "tiny", "small", "large"};
	for (int i = 0; i < 4; ++i)
	{
		SECTION("realloc to %s", types[i]);
		tiny_ptr[i] = t_realloc(tiny_ptr[i], size[i]);
		small_ptr[i] = t_realloc(small_ptr[i], size[i]);
		large_ptr[i] = t_realloc(large_ptr[i], size[i]);
	}

	show_alloc_mem();

	SECTION("free all memory");
	for (int i = 0; i < 4; ++i)
	{
		t_free(tiny_ptr[i]);
		t_free(small_ptr[i]);
		t_free(large_ptr[i]);
	}

	show_alloc_mem();

	SECTION("realloc from nothing");
	tiny_ptr[0] = t_realloc(nothing, 1);
	small_ptr[0] = t_realloc(nothing, 33);
	large_ptr[0] = t_realloc(nothing, 129);

	show_alloc_mem_ex();

	SECTION("free all memory");
	t_free(tiny_ptr[0]);
	t_free(small_ptr[0]);
	t_free(large_ptr[0]);

	show_alloc_mem();

	SECTION("test thread safety");
	test_threads();
	show_alloc_mem();

	SECTION("test cleanup");
	cleanup();
	show_alloc_mem();

	SECTION("safe malloc");
	void *ptr = safe_malloc(10000000000000000);
	t_free(ptr);
}