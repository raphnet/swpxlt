#ifndef _growbuf_h__
#define _growbuf_h__

#include <stdio.h>
#include <stdint.h>

struct growbuf {
	int count;

	uint8_t *data;
	int alloc_size;
};

void growbuf_free(struct growbuf *gb);
struct growbuf *growbuf_alloc(int initial_size);
void growbuf_append(struct growbuf *gb, const uint8_t *data, int size);

void growbuf_add32le(struct growbuf *gb, uint32_t val);
void growbuf_add16le(struct growbuf *gb, uint16_t val);
void growbuf_add8(struct growbuf *gb, uint8_t val);

int growbuf_writeToFPTR(const struct growbuf *gb, FILE *fptr);

#endif // _growbuf_h__
