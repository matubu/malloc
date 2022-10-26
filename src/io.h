#pragma once

#include <unistd.h>

static inline void	put_base(unsigned long long n, const char *base, int baselen)
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

static inline void	put_hex_pad(unsigned long long n, int pad)
{
	char	buf[16];
	int		i;

	i = pad;
	while (i--) {
		buf[i] = "0123456789abcdef"[n % 16];
		n /= 16;
	}
	write(1, buf, pad);
}

#define PUT(s) write(1, s, sizeof(s)-1);
#define PUTS(s) PUT(s "\n");
#define PUT_BASE(n, base) put_base(n, base, sizeof(base)-1);
#define PTR(ptr) PUT("\033[93m0x") PUT_BASE((unsigned long long)ptr, "0123456789abcdef") PUT("\033[0m");
#define ULONG(n) PUT("\033[93m") PUT_BASE(n, "0123456789") PUT("\033[0m");

static inline void	hexdump(const char *ptr, size_t size)
{
	while (size)
	{
		PTR(ptr);
		for (int i = 0; i < 16; ++i)
		{
			if (size == 0)
				break ;
			PUT(" ");
			put_hex_pad(*ptr++, 2);
			--size;
		}
		PUTS("");
	}
}