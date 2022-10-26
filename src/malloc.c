#include <sys/mman.h>
#include <sys/resource.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <execinfo.h>
#include <errno.h>
#include <limits.h>

#include "io.h"
#include "utils.h"

#define PAGE_SIZE_CONST               (8192)
#define PAGE_SIZE                     (getpagesize())

#define TINY_ZONE_PREALLOCATE_SIZE    (PAGE_SIZE_CONST * 16)
#define TINY_ZONE_MAX_SIZE            (TINY_ZONE_PREALLOCATE_SIZE / 400)

#define SMALL_ZONE_PREALLOCATE_SIZE   (PAGE_SIZE_CONST * 32)
#define SMALL_ZONE_MAX_SIZE           (SMALL_ZONE_PREALLOCATE_SIZE / 200)

#define MALLOC_MMAP_PROT              (PROT_READ | PROT_WRITE)
#define MALLOC_MMAP_FLAGS             (MAP_ANON | MAP_PRIVATE)

// This structure is followed by {size} bytes
// which are linked to this allocation
typedef struct Allocation_s {
	// The pointer to the next allocation
	struct Allocation_s		*next;
	// Size of this allocation (without including this header)
	size_t					size;
	// Is this allocation used
	uint8_t					used;
}	Allocation;

typedef struct Mmap_s {
	// The pointer to the next memory map
	struct Mmap_s	*next;
	// Maximum contiguous space aka largest zone available inside
	// uint64_t		max_contiguous;
	// Size of this memory map (include this header)
	size_t			mmap_size;
	// Pointer to the first allocation in this memory map
	Allocation		*allocations;
}	Mmap;


static Mmap				*mapped_zones = NULL;
static pthread_mutex_t	g_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void	*malloc_search(size_t size) {
	Mmap	*zone = mapped_zones;

	while (zone) {
		Allocation	*allocation = zone->allocations;

		while (allocation) {
			if (allocation->used == 0 && allocation->size >= size) {
				Allocation	*left_over = allocation->next;

				if (allocation->size > size + sizeof(Allocation)) {
					// TODO merge with next empty alloc
					left_over = (void *)allocation + sizeof(Allocation) + size;

					left_over->next = allocation->next;
					left_over->size = allocation->size - (size + sizeof(Allocation));
					left_over->used = 0;
				}

				allocation->next = left_over;
				allocation->size = size;
				allocation->used = 1;

				return (void *)allocation + sizeof(Allocation);
			}

			allocation = allocation->next;
		}
		
		zone = zone->next;
	}

	return NULL;
}


static inline void	*malloc_mmap(size_t size) {
	size_t		alloc_size;

	if (size <= TINY_ZONE_MAX_SIZE) {
		alloc_size = TINY_ZONE_PREALLOCATE_SIZE;
	}
	else if (size <= SMALL_ZONE_MAX_SIZE) {
		alloc_size = SMALL_ZONE_PREALLOCATE_SIZE;
	}
	else {
		alloc_size = align_up(sizeof(Mmap) + sizeof(Allocation) + size);
	}

	Mmap	*memory_map
		= mmap(0, alloc_size, MALLOC_MMAP_PROT, MALLOC_MMAP_FLAGS, -1, 0);
	if (memory_map == MAP_FAILED) {
		return NULL;
	}

	Allocation	*allocation = (void *)memory_map + sizeof(Mmap);
	allocation->size = size;
	allocation->used = 1;

	Allocation	*left_over = NULL;

	size_t	used = sizeof(Mmap) + sizeof(Allocation) + size;
	if (alloc_size > used + sizeof(Allocation)) {
		left_over = (void *)memory_map + used;

		left_over->next = NULL;
		left_over->size = alloc_size - (used + sizeof(Allocation));
		left_over->used = 0;
	}

	allocation->next = left_over;

	memory_map->next = mapped_zones;
	memory_map->mmap_size = alloc_size;
	memory_map->allocations = allocation;

	mapped_zones = memory_map;

	return (void *)allocation + sizeof(Allocation);
}


void	*malloc(size_t size) {
	void	*ptr;

	if (size == 0) {
		return (void *)-1;
	}

	pthread_mutex_lock(&g_mutex);

	if ((ptr = malloc_search(size))) {
		pthread_mutex_unlock(&g_mutex);
		return (ptr);
	}

	ptr = malloc_mmap(size);
	pthread_mutex_unlock(&g_mutex);
	return (ptr);
}

static int	get_allocation_info(void *ptr, Allocation **previous, Allocation **current) {
	Mmap	*zone = mapped_zones;

	while (zone) {
		if (ptr < (void*)zone || ptr >= (void*)zone + zone->mmap_size) {
			zone = zone->next;
			continue ;
		}

		*previous = NULL;
		*current = zone->allocations;

		while (*current) {
			if ((void *)*current + sizeof(Allocation) == ptr) {
				return 0;
			}

			*previous = *current;
			*current = (*current)->next;
		}
		return -1;
	}
	return -1;
}

void	free(void *ptr) {
	Allocation	*prev_allocation;
	Allocation	*allocation;

	pthread_mutex_lock(&g_mutex);
	if (ptr == NULL || get_allocation_info(ptr, &prev_allocation, &allocation) < 0) {
		pthread_mutex_unlock(&g_mutex);
		return ;
	}

	allocation->used = 0;

	// Merge with previous
	if (prev_allocation && prev_allocation->used == 0) {
		prev_allocation->size += sizeof(Allocation) + allocation->size;
		prev_allocation->next = allocation->next;
		allocation = prev_allocation;
	}

	// Merge with next
	if (allocation->next && allocation->next->used == 0) {
		allocation->size += allocation->next->size + sizeof(Allocation);
		allocation->next = allocation->next->next;
	}
	pthread_mutex_unlock(&g_mutex);
}

void	*realloc(void *ptr, size_t size) {
	Allocation	*prev_allocation;
	Allocation	*allocation;

	if (size == 0) {
		return ptr;
	}

	pthread_mutex_lock(&g_mutex);
	if (get_allocation_info(ptr, &prev_allocation, &allocation) < 0) {
		pthread_mutex_unlock(&g_mutex);
		return NULL;
	}

	if (allocation->size >= size) {
		pthread_mutex_unlock(&g_mutex);
		return ptr;
	}

	if (allocation->next && allocation->next->used == 0
		&& allocation->size + sizeof(Allocation) + allocation->next->size >= size) {
		// Merge allocation
		// TODO Add update available left_over
		allocation->size += allocation->next->size + sizeof(Allocation);
		allocation->next = allocation->next->next;
		pthread_mutex_unlock(&g_mutex);
		return ptr;
	}

	pthread_mutex_unlock(&g_mutex);
	// Allocate new memory
	void	*new_ptr = malloc(size);

	if (new_ptr == NULL) {
		return NULL;
	}
	ft_memcpy(new_ptr, ptr, allocation->size);
	free(ptr);
	return new_ptr;
}

static inline void	sort_mmap() {
	int		swap = 1;

	if (mapped_zones == NULL) {
		return ;
	}
	while (swap) {
		swap = 0;

		Mmap	**prev = &mapped_zones;
		Mmap	*node = mapped_zones->next;

		while (node) {
			if (*prev > node) {
				swap = 1;
				(*prev)->next = node->next;
				node->next = *prev;
				*prev = node;
				break ;
			}
			prev = &(*prev)->next;
			node = node->next;
		}
	}
}

static void	_show_alloc_mem(int enable_hexdump) {
	pthread_mutex_lock(&g_mutex);
	PUTS("Alloc mem report:");

	sort_mmap();

	Mmap	*zone = mapped_zones;
	size_t	total_bytes_used = 0;

	while (zone) {
		// Zone header
		PUT("\x1b[1;94mZone\x1b[0m(");
		PTR(zone);
		PUT("-");
		PTR((void *)zone + zone->mmap_size);
		PUT("), ");
		ULONG(zone->mmap_size);
		PUTS(" bytes");

		Allocation	*allocation = zone->allocations;

		while (allocation) {
			// Allocation header
			if (allocation->next) {
				PUT(" ├");
			} else {
				PUT(" ╰");
			}
			PUT("─> \x1b[94m");
			if (allocation->size <= TINY_ZONE_MAX_SIZE) {
				PUT("Tiny");
			} else if (allocation->size <= SMALL_ZONE_MAX_SIZE) {
				PUT("Small");
			} else {
				PUT("Large");
			}
			PUT("Allocation\x1b[0m(");
			PTR(allocation);
			PUT("-");
			PTR((void *)allocation + allocation->size + sizeof(Allocation));
			PUT(") -> ")
			PTR((void *)allocation + sizeof(Allocation));
			PUT(", ");
			ULONG(allocation->size);
			PUT(" bytes");
			if (allocation->used == 0) {
				PUT(" \x1b[91m(not used)\x1b[0m");
			} else {
				total_bytes_used += allocation->size;
			}
			PUTS("");

			if (allocation->used == 1 && enable_hexdump) {
				hexdump((void *)allocation + sizeof(Allocation), allocation->size);
			}

			allocation = allocation->next;
		}

		zone = zone->next;
	}

	PUT("Total bytes used ");
	ULONG(total_bytes_used);
	PUTS(" bytes");

	pthread_mutex_unlock(&g_mutex);
}

void	show_alloc_mem(void) {
	_show_alloc_mem(0);
}
void	show_alloc_mem_ex(void) {
	_show_alloc_mem(1);
}

// Bonus
void	*calloc(size_t nmemb, size_t size) {
	if (nmemb == 0 || size == 0) {
		return (void *)-1;
	}
	// Overflow check
	if (nmemb > INT_MAX / size) {
		errno = ENOMEM;
		return (NULL);
	}
	char *ptr = malloc(nmemb * size);
	if (ptr == NULL) {
		return (NULL);
	}
	for (size_t i = 0; i < size; ++i) {
		ptr[i] = 0;
	}
	return (ptr);
}

void	*reallocarray(void *ptr, size_t nmemb, size_t size) {
	if (nmemb == 0 || size == 0) {
		return (void *)-1;
	}
	// Overflow check
	if (nmemb > INT_MAX / size) {
		errno = ENOMEM;
		return (NULL);
	}
	return (realloc(ptr, nmemb * size));
}

void	*safe_malloc(size_t size) {
	void	*ptr = malloc(size);

	if (ptr == NULL) {
		PUTS("\x1b[91mError\x1b[91m: failed to allocate memory");
		exit(1);
	}
	return (ptr);
}
