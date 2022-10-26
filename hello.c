#include <sys/mman.h>
#include <sys/resource.h>
#include <execinfo.h>
#include <stdio.h>
#include <unistd.h>

int main() {
	int size = getpagesize();
	char	*ptr_a = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	char	*ptr_b = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

	ptr_a[0] = 5;

	printf("%p %p\n", ptr_a, ptr_b);

	int ret = munmap(ptr_a, size * 2);

	printf("Two in one: %d\n", ret);

	char	*ptr = mmap(0, size * 2, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

	printf("%p\n", ptr);
	
	int ret_a = munmap(ptr, size);
	int ret_b = 0; // munmap(ptr + size, size);

	printf("One in two: %d %d\n", ret_a, ret_b);
	ptr[size] = 'a';

	printf("%d\n", size);
}