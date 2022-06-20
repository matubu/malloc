#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "malloc.h"

int	rand_malloc()
{
	return (rand() % 16000);
}

void	*allocate(void *i)
{
	printf("launch thread %d\n", (int)i);
	for (int i = 0; i < 20; ++i)
	{
		void *ptr = malloc(rand_malloc());
		ptr = realloc(ptr, rand_malloc());
		if (rand() % 2)
			free(ptr);
	}
	return (NULL);
}

void	test_threads()
{
	pthread_t threads[64];

	for (size_t i = 0; i < sizeof(threads) / sizeof(threads[0]); ++i)
		pthread_create(threads + i, NULL, allocate, (void *)i);
	for (size_t i = 0; i < sizeof(threads) / sizeof(threads[0]); ++i)
		pthread_join(threads[i], NULL);
}