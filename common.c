#include <string.h>
#include <stdlib.h>

int payload_size(char *payload) {
	int i;

	memcpy(&i, payload, sizeof(i));

	if (i == -1)
		return 0;

	if (i == 1 || i == 5 || i == 7)
		return 4;

	if (i == 0 || i == 2 || i == 3 || i == 6)
		return 4+32;

	if (i == 4)
		return 4+32+64;

	// Shouldnt be reached, if it is return error state
	return 0;
}

void *xmalloc(unsigned int size) {
	void *ptr;

	ptr = malloc(size);
	if (ptr)
		return ptr;

	exit(255);
}
