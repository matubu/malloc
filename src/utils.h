#pragma once

#include <stdint.h>
#include <unistd.h>

// static uint64_t	left_most_bit(uint64_t bytes) {
// 	bytes = ~bytes; // Invert bytes
// 	if (bytes == 0) // Verify their is a unset bit
// 		return (-1);
// 	int			offset = 0;
// 	uint64_t	width = 32;

// 	// Binary search
// 	while (width)
// 	{
// 		if ((bytes & (((uint64_t)1 << width) - 1)) == 0) // Check if full
// 		{
// 			bytes >>= width; // Shift bytes to next half
// 			offset += width; // Increment counter
// 		}
// 		width >>= 1; // Divide by two
// 	}

// 	return (offset);
// }

// Align the pointer by rounding up
static unsigned long long align_up(unsigned long long value) {
	const size_t pagesize = getpagesize();

	return ((value + pagesize - 1) / pagesize * pagesize);
}

static void	ft_memcpy(void *dest, void *source, size_t count) {
	while (count--) {
		*(char *)dest++ = *(char *)source++;
	}
}