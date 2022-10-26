#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "malloc.h"

int	main() {
	// Test defragmenting
	char	*ptr1 = malloc(10);
	char	*ptr2 = malloc(10);
	char	*ptr3 = malloc(10);

	printf("%.0s%.0s%.0s", ptr1, ptr2, ptr3);

	show_alloc_mem();
	free(ptr1);
	show_alloc_mem();
	free(ptr3);
	show_alloc_mem();
	free(ptr2);
	show_alloc_mem();
	ptr1 = malloc(10);
	ptr2 = malloc(10);
	ptr3 = malloc(10);
	show_alloc_mem();

	printf("%.0s%.0s%.0s", ptr1, ptr2, ptr3);

	// Make lots of allocations
	srand(time(NULL));

	for (int i = 0; i < 10000; i++) {
		int		size = rand() % 1000;
		char	*ptr = malloc(size);

		for (int j = 0; j < size; ++j) {
			ptr[j] = j;
		}

		if (rand() % 2) {
			free(ptr);
		}
	}


	void	*ptr = ;

	show_alloc_mem();
}