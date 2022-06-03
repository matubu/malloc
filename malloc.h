#pragma once

#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

void	*malloc(size_t size);
void	free(void *ptr);
void	*realloc(void *ptr, size_t size);

void	show_alloc_mem(void);