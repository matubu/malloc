#include <sys/mman.h>
#include <sys/resource.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <execinfo.h>

#include "io.h"
#include "utils.h"

#define PAGE_SIZE_CONST               (8192)
#define PAGE_SIZE                     (getpagesize())

#define TINY_ZONE_PREALLOCATE_SIZE    (PAGE_SIZE_CONST * 4)
#define TINY_ZONE_MAX_SIZE            (TINY_ZONE_PREALLOCATE_SIZE / 400)

#define SMALL_ZONE_PREALLOCATE_SIZE   (PAGE_SIZE_CONST * 8)
#define SMALL_ZONE_MAX_SIZE           (TINY_ZONE_MAX_SIZE / 400)

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


static Mmap *mapped_zones = NULL;


static inline void	*malloc_search(size_t size) {
	Mmap	*zone = mapped_zones;

	while (zone) {
		Allocation	*allocation = zone->allocations;

		while (allocation) {
			if (allocation->used == 0 && allocation->size >= size) {
				Allocation	*left_over = allocation->next;

				if (allocation->size > size + sizeof(Allocation)) {
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

	if ((ptr = malloc_search(size))) {
		return (ptr);
	}

	return (malloc_mmap(size));
}

int	get_allocation_info(void *ptr, Allocation **previous, Allocation **current) {
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

	if (ptr == NULL || get_allocation_info(ptr, &prev_allocation, &allocation) < 0) {
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
}

void	*realloc(void *ptr, size_t size) {
	Allocation	*prev_allocation;
	Allocation	*allocation;

	if (size == 0) {
		return ptr;
	}

	if (get_allocation_info(ptr, &prev_allocation, &allocation) < 0) {
		return NULL;
	}

	if (allocation->size >= size) {
		return ptr;
	}

	if (allocation->next && allocation->next->used == 0
		&& allocation->size + sizeof(Allocation) + allocation->next->size > size) {
		// Merge allocation
		// TODO Add update available left_over
		allocation->size += allocation->next->size + sizeof(Allocation);
		allocation->next = allocation->next->next;
		return ptr;
	}

	// Allocate new memory
	void	*new_ptr = malloc(size);

	if (new_ptr == NULL) {
		return NULL;
	}
	ft_memcpy(new_ptr, ptr, allocation->size);
	free(ptr);
	return ptr;
}

#include <stdio.h>

void	show_alloc_mem(void) {
	Mmap	*zone = mapped_zones;

	printf("Zones:\n");
	while (zone) {
		printf(
			"\x1b[94mZone\x1b[0m [%p-%p] (\x1b[93m%ld\x1b[0m bytes):\n",
			zone,
			(void *)zone + zone->mmap_size,
			zone->mmap_size
		);

		Allocation	*allocation = zone->allocations;

		while (allocation) {
			printf(
				"\t\x1b[94mAllocation\x1b[0m [%p-%p]:\n",
				allocation,
				(void *)allocation + allocation->size + sizeof(Allocation)
			);
			printf("\t\t.ptr: \x1b[93m%p\x1b[0m\n", (void *)allocation + sizeof(Allocation));
			printf("\t\t.size: \x1b[93m%ld\x1b[0m\n", allocation->size);
			printf("\t\t.used: %s\x1b[0m\n", allocation->used ? "\x1b[92mtrue" : "\x1b[91mfalse");

			allocation = allocation->next;
		}

		zone = zone->next;
	}
}

// TODO merge Mmap ?
// TODO mutex