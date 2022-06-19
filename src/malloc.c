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

	while ((idx = lmub(chunk_used[chunk_id])) == -1)
		if (++chunk_id >= 4)
			return (NULL);
	chunk_used[chunk_id] |= 1 << idx; // Update to used
	idx += chunk_id * 64;
	chunk_size[idx] = size;           // Update size for realloc
	return (chunk_data[idx]);         // Return data address
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
		tiny_used[idx / 64] &= ~(1 << (idx & 63));
		return ;
	}

	if (IS_SMALL_MEMORY_ADDRESS(ptr))
	{
		int idx = GET_SMALL_MEMORY_ADDRESS_INDEX(ptr);
		small_used[idx / 64] &= ~(1 << (idx & 63));
		return ;
	}

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
	if (ptr == NULL || ptr == (void *)-1)
		return (malloc(newdatasize));

	size_t	datasize = 0;
	if (IS_TINY_MEMORY_ADDRESS(ptr))
	{
		int idx = GET_TINY_MEMORY_ADDRESS_INDEX(ptr);
		if (newdatasize <= TINY_STORAGE)
		{
			/* Update used space */
			tiny_size[idx] = newdatasize;
			/* Return original pointer */
			return (ptr);
		}
		datasize = tiny_size[idx];
	}
	else if (IS_SMALL_MEMORY_ADDRESS(ptr))
	{
		int idx = GET_SMALL_MEMORY_ADDRESS_INDEX(ptr);
		if (newdatasize <= SMALL_STORAGE)
		{
			/* Update used space */
			small_size[idx] = newdatasize;
			/* Return original pointer */
			return (ptr);
		}
		datasize = small_size[idx];
	}
	else
	{
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
			/* Unmap the unused part */
			size_t	newavailable = PAGE_SIZE_MULTIPLE(newsize, pagesize);
			munmap(ptr - sizeof(chunk_header_t) + newavailable, available - newavailable);
			/* Return original pointer */
			return (ptr);
		}

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

void	put_base(unsigned long long n, const char *base, int baselen)
{
	char	buf[19];
	int		i;

	i = 19;
	do {
		buf[--i] = base[n % baselen];
		n /= baselen;
	} while (n);
	write(1, buf + i, 19 - i);
}

#define PUT(s) write(1, s, sizeof(s));
#define PUTS(s) PUT(s "\n");
#define PUT_BASE(n, base) put_base(n, base, sizeof(base));
#define PTR(ptr) PUT("\033[93m0x") PUT_BASE((unsigned long long)ptr, "0123456789abcef") PUT("\033[0m");
#define ULONG(n) PUT("\033[93m") PUT_BASE(n, "0123456789") PUT("\033[0m");


void	show_alloc_chunk(
	uint64_t *chunk_used,
	uint8_t *chunk_size,
	int storage,
	uint8_t chunk_data[256][storage]
)
{
	for (int chunk_idx = 0; chunk_idx < 4; ++chunk_idx)
	{
		for (int idx = 0; idx < 64; ++idx)
		{
			if (chunk_used[chunk_idx] & ((uint64_t)1 << idx))
			{
				PUT("[") PTR(&chunk_data[chunk_idx * 64 + idx]) PUT(", ")
				PTR(&chunk_data[chunk_idx * 64 + idx + 1]) PUT("): ")
				ULONG(chunk_size[chunk_idx * 64 + idx]) PUT(" bytes (real ")
				ULONG(storage) PUTS(" bytes)");
			}
		}
	}
}

void	show_alloc_mem(void)
{
	PUTS("\n═════════════ \033[1;94mAllocated mem\033[0m ═════════════");
	PUT("\033[94mTiny\033[0m : \033[94mrange\033[0m[") PTR(tiny_data) PUT(",") PTR(tiny_data + sizeof(tiny_data)) PUTS(")");
	show_alloc_chunk(
		tiny_used,
		tiny_size,
		TINY_STORAGE,
		tiny_data
	);
	PUT("\033[92mSmall\033[0m : \033[94mrange\033[0m[") PTR(small_data) PUT(", ") PTR(small_data + sizeof(small_data)) PUTS(")");
	show_alloc_chunk(
		small_used,
		small_size,
		SMALL_STORAGE,
		small_data
	);
	#if DEV
		PUTS("\033[91mLarge\033[0m");
		const int	pagesize = getpagesize();
		chunk_header_t	*node = first_large;

		while (node)
		{
			PUT("[") PTR(node + sizeof(chunk_header_t)) PUT(", ")
			PTR(node + PAGE_SIZE_MULTIPLE(node->size, pagesize)) PUT("): ")
			ULONG(node->size - sizeof(chunk_header_t)) PUT(" bytes (real ")
			ULONG(PAGE_SIZE_MULTIPLE(node->size, pagesize)) PUTS(" bytes)");
			node = node->next;
		}
	#endif
	PUTS("═════════════════════════════════════════\n");
}