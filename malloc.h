#pragma once

void	*malloc(size_t size);
void	free(void *ptr);
void	*realloc(void *ptr, size_t size);

void	show_alloc_mem(void);
void	show_alloc_mem_ex(void);

void	cleanup(void);
void	*safe_malloc(size_t size);