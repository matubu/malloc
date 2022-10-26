#include "malloc.h"

int main() {
	printf("%s\n", strdup("42 nice"));
	show_alloc_mem_ex();
}