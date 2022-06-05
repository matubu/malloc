#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

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

// TODO
// malloc(tiny) ✅
// malloc(small) ❌
// malloc(large) ✅

// free(tiny) ✅
// free(small) ❌
// free(large) ✅

// realloc(tiny) ❌
// realloc(small) ❌
// realloc(large) ✅

// show_alloc_mem(tiny) ✅
// show_alloc_mem(small) ❌
// show_alloc_mem(large) ✅

// Zones
// total stack memory preallocated 74_304 bytes


// - Tiny [0-32] bytes (suitable for chained list)
// tiny stack memory 8_480 bytes
uint8_t		tiny_data[256][32];  // actual data/pointer 
uint8_t		tiny_size[256];      // size of every memory address
uint64_t	tiny_used[4] = {0};  // to keep track of used block


// - Small [33-256] bytes (suitable for small length string)
// small stack memory 65_824 bytes
uint8_t		small_data[256][256];
uint8_t		small_size[256];
uint64_t	small_used[4] = {0};

// - Chunked memory block


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


/* Get the rounded up multiple of the page size */
#define PAGE_SIZE_MULTIPLE(size, pagesize) ((size + pagesize - 1) & ~(pagesize - 1))

static inline void	*malloc_tiny(size_t size)
{
	int	chunk_id = 0;
	int	idx;

	while ((idx = lmub(tiny_used[chunk_id])) == -1)
		if (++chunk_id >= 4)
			return (NULL);
	tiny_used[chunk_id] |= 1 << idx; // Update to used
	idx += chunk_id * 64;
	tiny_size[idx] = size;           // Update size for realloc
	return (tiny_data[idx]);         // Return data address
}

// static inline void	*malloc_small(size_t size)
// {
// 	(void)size;
// 	return (NULL);
// }

static inline void	*malloc_large(size_t size)
{
	chunk_header_t	*ptr;

	/* Add the header size */
	size += sizeof(chunk_header_t);

	if (!(ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0)))
		return (NULL);
	ptr->size = size;
	#if DEV
		// Push front
		ptr->next = first_large;
		ptr->prev = &first_large;
		if (first_large)
			first_large->prev = &ptr->next;
		first_large = ptr;
	#endif
	return ((void *)(ptr + 1));
}

void	*malloc(size_t size)
{
	void	*ptr;

	if (size <= 32 && (ptr = malloc_tiny(size)))
		return (ptr);
	// if (size <= 256 && (ptr = malloc_small(size)))
	// 	return (ptr);

	return (malloc_large(size));
}

void	free(void *ptr)
{
	if (ptr == NULL)
		return ;

	if (
		ptr >= (void *)tiny_data
		&& ptr < (void *)tiny_data + sizeof(tiny_data)
	)
	{
		int idx = (ptr - (void *)tiny_data) / sizeof(tiny_data[0]);
		tiny_used[idx / 64] &= ~(1 << (idx & 63));
		return ;
	}

	// if (
	// 	ptr >= small_data
	// 	&& ptr <= small_data + sizeof(small_data))
	// {
	// 	return ;
	// }

	/* Move to allocation start */
	chunk_header_t	*chunk_header = (chunk_header_t *)ptr - 1;

	#if DEV
		if (chunk_header->next)
			chunk_header->next->prev = chunk_header->prev;
		*chunk_header->prev = chunk_header->next;
	#endif

	/* Get allocation size */
	size_t	size = chunk_header->size;
	munmap(ptr, size);
}

void	*realloc(void *ptr, size_t newdatasize)
{
	if (ptr == NULL)
		return (malloc(newdatasize));

	/* Add the header size */
	size_t	newsize = newdatasize + sizeof(chunk_header_t);

	chunk_header_t	*chunk_header = (chunk_header_t *)ptr - 1;

	/* Get current size */
	size_t	size = chunk_header->size;

	/* Check available space */
	const int	pagesize = getpagesize();
	size_t		available = PAGE_SIZE_MULTIPLE(size, pagesize);

	if (available >= newsize)
	{
		/* Update used space to know how much to copy next time */
		chunk_header->size = newsize;
		/* Unmap the unused part */
		size_t	newavailable = PAGE_SIZE_MULTIPLE(newsize, pagesize);
		munmap(ptr - sizeof(chunk_header_t) + newavailable, available - newavailable);
		/* Return original pointer */
		return (ptr);
	}

	void	*newptr = malloc(newdatasize);
	
	if (newptr == NULL)
		return (NULL);

	/* Copy memory */
	size_t	datasize = size - sizeof(chunk_header_t);
	for (size_t i = 0; i < datasize; ++i)
		((char *)newptr)[i] = ((char *)ptr)[i];

	/* Free old pointer */
	free(ptr);

	return (newptr);
}

void	show_alloc_mem(void)
{
	printf("\n═════════════ \033[1;94mAllocated mem\033[0m ═════════════\n");
	printf("\033[94mTiny\033[0m : [%p, %p)\n", (void *)tiny_data, (void *)tiny_data + sizeof(tiny_data));
	for (int chunk_idx = 0; chunk_idx < 4; ++chunk_idx)
	{
		for (int idx = 0; idx < 64; ++idx)
		{
			if (tiny_used[chunk_idx] & ((uint64_t)1 << idx))
			{
				printf("[%p, %p): %d bytes (real %ld bytes)\n",
					(void *)&tiny_data[chunk_idx * 64 + idx],
					(void *)&tiny_data[chunk_idx * 64 + idx + 1],
					tiny_size[chunk_idx * 64 + idx],
					sizeof(tiny_data[0])
				);
			}
		}
	}
	#if DEV
		printf("\033[91mLarge\033[0m\n");
		const int	pagesize = getpagesize();
		chunk_header_t	*node = first_large;

		while (node)
		{
			printf("[%p, %p): %ld bytes (real %ld bytes)\n",
				(void *)node + sizeof(chunk_header_t),
				(void *)node + PAGE_SIZE_MULTIPLE(node->size, pagesize),
				node->size - sizeof(chunk_header_t),
				PAGE_SIZE_MULTIPLE(node->size, pagesize)
			);
			node = node->next;
		}
	#endif
	printf("═════════════════════════════════════════\n\n");
}