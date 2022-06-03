#include "malloc.h"
#include <stdio.h>

typedef struct chunk_header_s {
	size_t			size;
}	chunk_header_t;

/* Get the rounded up multiple of the page size */
#define PAGE_SIZE_MULTIPLE(size, pagesize) ((size + pagesize - 1) & ~(pagesize - 1))

void	free(void *ptr)
{
	if (ptr == NULL)
		return ;

	/* Move to allocation start */
	ptr -= sizeof(chunk_header_t);
	/* Get allocation size */
	size_t	size = ((chunk_header_t *)ptr)->size;
	munmap(ptr, size);
}

void	*malloc(size_t size)
{
	chunk_header_t	*ptr;

	/* Add the header size */
	size += sizeof(chunk_header_t);

	if (!(ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0)))
		return (NULL);
	ptr->size = size;
	return ((void *)(ptr + 1));
}

void	*realloc(void *ptr, size_t newdatasize)
{
	if (ptr == NULL)
		return (malloc(newdatasize));

	/* Add the header size */
	size_t	newsize = newdatasize + sizeof(chunk_header_t);

	/* Get current size */
	size_t	size = ((chunk_header_t *)ptr - 1)->size;

	/* Check available space */
	const int	pagesize = getpagesize();
	size_t		available = PAGE_SIZE_MULTIPLE(size, pagesize);

	if (available >= newsize)
	{
		/* Update used space to know how much to copy next time */
		((chunk_header_t *)ptr - 1)->size = newsize;
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
}