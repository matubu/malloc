#pragma once

#include <stddef.h>

// Mandatory:
void	*malloc(size_t size);
void	free(void *ptr);
void	*realloc(void *ptr, size_t size);

void	show_alloc_mem(void);
void	show_alloc_mem_ex(void);

void	*calloc(size_t nmemb, size_t size);
void	*reallocarray(void *ptr, size_t nmemb, size_t size);

void	*safe_malloc(size_t size);