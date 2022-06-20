#include <sys/mman.h>
#include <sys/resource.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include "io.h"

#ifndef DEV
# define DEV 1
#endif

// Left most unset bit
static inline int	lmub(uint64_t bytes)
{
	bytes = ~bytes; // Invert bytes
	if (bytes == 0) // Verify their is a unset bit
		return (-1);
	int			offset = 0;
	uint64_t	width = 32;

	// Binary search
	while (width)
	{
		if ((bytes & (((uint64_t)1 << width) - 1)) == 0) // Check if full
		{
			bytes >>= width; // Shift bytes to next half
			offset += width; // Increment counter
		}
		width >>= 1; // Divide by two
	}

	return (offset);
}

// Zones
// total stack memory preallocated 74_304 bytes

// - Tiny [0-32] bytes (suitable for chained list)
// tiny stack memory 8_480 bytes
#define TINY_STORAGE 32
#define IS_TINY_MEMORY_ADDRESS(addr) (addr >= (void *)tiny_data \
		&& addr < (void *)tiny_data + sizeof(tiny_data))
#define GET_TINY_MEMORY_ADDRESS_INDEX(addr) ((addr - (void *)tiny_data) / TINY_STORAGE)
uint64_t	tiny_used[4] = {0};  // to keep track of used block
uint8_t		tiny_size[256];      // size of every memory address
uint8_t		tiny_data[256][TINY_STORAGE];  // actual data/pointer 


// - Small [33-128] bytes (suitable for small length string)
// small stack memory 65_824 bytes
#define SMALL_STORAGE 128
#define IS_SMALL_MEMORY_ADDRESS(addr) (addr >= (void *)small_data \
		&& addr < (void *)small_data + sizeof(small_data))
#define GET_SMALL_MEMORY_ADDRESS_INDEX(addr) ((addr - (void *)small_data) / SMALL_STORAGE)
uint64_t	small_used[4] = {0};
uint8_t		small_size[256];
uint8_t		small_data[256][SMALL_STORAGE];

// - Large [257-∞] bytes (suitable for big buffer allocation)
// 8 bytes header (24 byte in dev mode)
// | [8 bytes] header | data |
// | containing size  |      |
typedef struct chunk_header_s {
	size_t			size;
	#if DEV
		struct chunk_header_s	*next;
		struct chunk_header_s	**prev; // point on use
	#endif
}	chunk_header_t;

#if DEV
	chunk_header_t	*first_large = NULL;
#endif

pthread_mutex_t chunk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t large_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Get the rounded up multiple of the page size */
#define PAGE_SIZE_MULTIPLE(size, pagesize) ((size + pagesize - 1) & ~(pagesize - 1))

static inline void	*malloc_chunk(
	uint64_t *chunk_used,
	uint8_t *chunk_size,
	int storage,
	uint8_t chunk_data[256][storage],
	size_t size
)
{
	int	chunk_id = 0;
	int	idx;

	pthread_mutex_lock(&chunk_mutex);
	while ((idx = lmub(chunk_used[chunk_id])) == -1)
	{
		if (++chunk_id >= 4)
		{
			pthread_mutex_unlock(&chunk_mutex);
			return (NULL);
		}

	}
	chunk_used[chunk_id] |= 1 << idx; // Update to used
	idx += chunk_id * 64;
	chunk_size[idx] = size;           // Update size for realloc
	void	*addr = chunk_data[idx];
	pthread_mutex_unlock(&chunk_mutex);
	return (addr);                    // Return data address
}

static inline void	*malloc_large(size_t size)
{
	/* Add the header size */
	size += sizeof(chunk_header_t); // TODO fix overflow bug

	chunk_header_t	*ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED)
		return (NULL);
	ptr->size = size;
	#if DEV
		pthread_mutex_lock(&large_mutex);
		// Push front
		ptr->next = first_large;
		ptr->prev = &first_large;
		if (first_large)
			first_large->prev = &ptr->next;
		first_large = ptr;
		pthread_mutex_unlock(&large_mutex);
	#endif
	return ((void *)(ptr + 1));
}

void	*malloc(size_t size)
{
	void	*ptr;

	if (size == 0)
		return ((void *)-1);

	if (size <= TINY_STORAGE && (ptr = malloc_chunk(
		tiny_used,
		tiny_size,
		TINY_STORAGE,
		tiny_data,
		size
	)))
		return (ptr);
	

	if (size <= SMALL_STORAGE && (ptr = malloc_chunk(
		small_used,
		small_size,
		SMALL_STORAGE,
		small_data,
		size
	)))
		return (ptr);

	return (malloc_large(size));
}

void	free(void *ptr)
{
	if (ptr == NULL || ptr == (void *)-1)
		return ;

	if (IS_TINY_MEMORY_ADDRESS(ptr))
	{
		int idx = GET_TINY_MEMORY_ADDRESS_INDEX(ptr);
		pthread_mutex_lock(&chunk_mutex);
		tiny_used[idx / 64] &= ~(1 << (idx & 63));
		pthread_mutex_unlock(&chunk_mutex);
		return ;
	}

	if (IS_SMALL_MEMORY_ADDRESS(ptr))
	{
		int idx = GET_SMALL_MEMORY_ADDRESS_INDEX(ptr);
		pthread_mutex_lock(&chunk_mutex);
		small_used[idx / 64] &= ~(1 << (idx & 63));
		pthread_mutex_unlock(&chunk_mutex);
		return ;
	}

	pthread_mutex_lock(&large_mutex);
	/* Move to allocation start */
	chunk_header_t	*chunk_header = (chunk_header_t *)ptr - 1;

	#if DEV
		if (chunk_header->next)
			chunk_header->next->prev = chunk_header->prev;
		*chunk_header->prev = chunk_header->next;
	#endif
	pthread_mutex_unlock(&large_mutex);

	/* Get allocation size */
	size_t	size = chunk_header->size;
	munmap(chunk_header, size);
}

void	*realloc(void *ptr, size_t newdatasize)
{
	if (ptr == NULL || ptr == (void *)-1)
		return (malloc(newdatasize));

	size_t	datasize = 0;
	if (IS_TINY_MEMORY_ADDRESS(ptr))
	{
		int idx = GET_TINY_MEMORY_ADDRESS_INDEX(ptr);
		pthread_mutex_lock(&chunk_mutex);
		if (newdatasize <= TINY_STORAGE)
		{
			/* Update used space */
			tiny_size[idx] = newdatasize;
			/* Return original pointer */
			pthread_mutex_unlock(&chunk_mutex);
			return (ptr);
		}
		datasize = tiny_size[idx];
		pthread_mutex_unlock(&chunk_mutex);
	}
	else if (IS_SMALL_MEMORY_ADDRESS(ptr))
	{
		int idx = GET_SMALL_MEMORY_ADDRESS_INDEX(ptr);
		pthread_mutex_lock(&chunk_mutex);
		if (newdatasize <= SMALL_STORAGE)
		{
			/* Update used space */
			small_size[idx] = newdatasize;
			/* Return original pointer */
			pthread_mutex_unlock(&chunk_mutex);
			return (ptr);
		}
		datasize = small_size[idx];
		pthread_mutex_unlock(&chunk_mutex);
	}
	else
	{
		pthread_mutex_lock(&large_mutex);
		chunk_header_t	*chunk_header = (chunk_header_t *)ptr - 1;

		/* Get current size */
		size_t	size = chunk_header->size;
		/* Calculate new size */
		size_t	newsize = newdatasize + sizeof(chunk_header_t);

		/* Check available space */
		const int	pagesize = getpagesize();
		size_t		available = PAGE_SIZE_MULTIPLE(size, pagesize);

		if (available >= newsize)
		{
			/* Update used space to know how much to copy next time */
			chunk_header->size = newsize;
			pthread_mutex_unlock(&large_mutex);
			/* Unmap the unused part */
			size_t	newavailable = PAGE_SIZE_MULTIPLE(newsize, pagesize);
			munmap(ptr - sizeof(chunk_header_t) + newavailable, available - newavailable);
			/* Return original pointer */
			return (ptr);
		}
		pthread_mutex_unlock(&large_mutex);

		datasize = size - sizeof(chunk_header_t);
	}

	void	*newptr = malloc(newdatasize);
	
	if (newptr == NULL)
		return (NULL);

	/* Copy memory */
	for (size_t i = 0; i < datasize; ++i)
		((char *)newptr)[i] = ((char *)ptr)[i];

	/* Free old pointer */
	free(ptr);

	return (newptr);
}

#define SHOW_ALLOC_CHUNK(used, size, storage, data, callback) { \
	for (int chunk_idx = 0; chunk_idx < 4; ++chunk_idx) \
	{ \
		for (int idx = 0; idx < 64; ++idx) \
		{ \
			if (used[chunk_idx] & ((uint64_t)1 << idx)) \
			{ \
				PUT("[") PTR(&data[chunk_idx * 64 + idx]) PUT(", ") \
				PTR(&data[chunk_idx * 64 + idx + 1]) PUT("): ") \
				ULONG(size[chunk_idx * 64 + idx]) PUT(" bytes (real ") \
				ULONG(storage) PUTS(" bytes)"); \
				callback((char *)(&data[chunk_idx * 64 + idx]), size[chunk_idx * 64 + idx]); \
			} \
		} \
	} \
}

#if DEV
# define SHOW_ALLOC_LARGE(callback) { \
	PUTS("\033[1;91mLarge\033[0m"); \
	const int	pagesize = getpagesize(); \
	chunk_header_t	*node = first_large; \
	while (node) \
	{ \
		PUT("[") PTR((void *)node + sizeof(chunk_header_t)) PUT(", ") \
		PTR(node + PAGE_SIZE_MULTIPLE(node->size, pagesize)) PUT("): ") \
		ULONG(node->size - sizeof(chunk_header_t)) PUT(" bytes (real ") \
		ULONG(PAGE_SIZE_MULTIPLE(node->size, pagesize)) PUTS(" bytes)"); \
		callback((char *)(node + 1), node->size - sizeof(chunk_header_t)); \
		node = node->next; \
	} \
}
#else
# define SHOW_ALLOC_LARGE()
#endif

#define SHOW_ALLOC_MEM(callback) { \
	pthread_mutex_lock(&chunk_mutex); \
	pthread_mutex_lock(&large_mutex); \
	PUTS("\n═════════════ \033[1;94mAllocated mem\033[0m ═════════════"); \
	PUT("\033[1;94mTiny\033[0m : range[") PTR(tiny_data) PUT(",") PTR(tiny_data + sizeof(tiny_data)) PUTS(")"); \
	SHOW_ALLOC_CHUNK( \
		tiny_used, \
		tiny_size, \
		TINY_STORAGE, \
		tiny_data, \
		callback \
	); \
	PUT("\033[1;92mSmall\033[0m : range[") PTR(small_data) PUT(", ") PTR(small_data + sizeof(small_data)) PUTS(")"); \
	SHOW_ALLOC_CHUNK( \
		small_used, \
		small_size, \
		SMALL_STORAGE, \
		small_data, \
		callback \
	); \
	SHOW_ALLOC_LARGE(callback); \
	PUTS("═════════════════════════════════════════\n"); \
	pthread_mutex_unlock(&chunk_mutex); \
	pthread_mutex_unlock(&large_mutex); \
}

void	show_alloc_mem(void)
{
	SHOW_ALLOC_MEM();
}

void	show_alloc_mem_ex(void)
{
	SHOW_ALLOC_MEM(hexdump);
}

#if DEV
void	cleanup(void)
{
	pthread_mutex_lock(&chunk_mutex);

	for (int chunk_idx = 0; chunk_idx < 4; ++chunk_idx)
	{
		tiny_used[chunk_idx] = 0;
		small_used[chunk_idx] = 0;
	}

	pthread_mutex_unlock(&chunk_mutex);

	pthread_mutex_lock(&large_mutex);
	chunk_header_t	*node = first_large;
	pthread_mutex_unlock(&large_mutex);
	while (node)
	{
		pthread_mutex_lock(&large_mutex);
		chunk_header_t	*next  = node->next;
		pthread_mutex_unlock(&large_mutex);

		free((void *)(node + 1));

		node = next;
	}
}
#endif

#include <execinfo.h>

void	show_backtrace(void)
{
	void	*array[64];
	char	**strings;
	int		size, i;

	size = backtrace(array, 64);
	strings = backtrace_symbols(array, size);
	if (strings == NULL)
	{
		PUTS("\033[91mError\033[0m cannot load backtrace");
		return ;
	}
	PUTS("\nBacktrace:");
	for (i = 0; i < size; i++)
		puts(strings[i]);
}

void	*safe_malloc(size_t size)
{
	void	*ptr = malloc(size);
	if (ptr == NULL)
	{
		cleanup();
		PUT("\033[91mError\033[0m cannot allocate ") ULONG(size) PUTS(" bytes");
		show_backtrace();
		exit(1);
	}
	return (ptr);
}