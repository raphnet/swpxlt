#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "growbuf.h"

void growbuf_free(struct growbuf *gb)
{
	if (gb) {
		if (gb->data) {
			free(gb->data);
		}
		free(gb);
	}
}

struct growbuf *growbuf_alloc(int initial_size)
{
	struct growbuf *gb;

	gb = calloc(1, sizeof(struct growbuf));
	if (!gb) {
		perror("growbuf");
		return NULL;
	}

	gb->data = calloc(1, initial_size);
	if (!gb->data) {
		perror("could not allocate growbuf initial");
		free(gb);
		return NULL;
	}

	gb->alloc_size = initial_size;

	return gb;
}

void growbuf_append(struct growbuf *gb, const uint8_t *data, int size)
{
	uint8_t *d;
	int newsize;

	if (size <= 0)
		return;

	if (gb->count + size > gb->alloc_size) {
		newsize = gb->alloc_size * 2 + size;
		d = realloc(gb->data, newsize);
		if (!d) {
			perror("Could not grow buffer");
			return;
		}
		gb->data = d;
		gb->alloc_size = newsize;
	}

	memcpy(gb->data + gb->count, data, size);
	gb->count += size;
}

void growbuf_add32le(struct growbuf *gb, uint32_t val)
{
	uint8_t tmp[4] = { val, val >> 8, val >> 16, val >> 24 };
	growbuf_append(gb, tmp, 4);
}

void growbuf_add16le(struct growbuf *gb, uint16_t val)
{
	uint8_t tmp[2] = { val, val >> 8 };
	growbuf_append(gb, tmp, 2);
}

void growbuf_add8(struct growbuf *gb, uint8_t val)
{
	uint8_t tmp[2] = { val };
	growbuf_append(gb, tmp, 1);
}

int growbuf_writeToFPTR(const struct growbuf *gb, FILE *fptr)
{
	return fwrite(gb->data, gb->count, 1, fptr);
}

