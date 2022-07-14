#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "flic.h"
#include "globals.h"

static uint32_t getLE32(const uint8_t *src, int *off)
{
	uint32_t v = src[*off] | (src[*off+1]<<8) | (src[*off+2]<<16) | (src[*off+3]<<24);

	*off += 4;

	return v;
}

static uint16_t getLE16(const uint8_t *src, int *off)
{
	uint16_t v = src[*off] | (src[*off+1]<<8);

	*off += 2;

	return v;
}

static int writeZeros(FILE *fptr, int count)
{
	uint8_t buf = 0;
	int i;

	for (i=0; i<count; i++) {
		if (1 != fwrite(&buf, 1,1,fptr)) {
			return -1;
		}
	}
	return 0;
}

static int writeLE16(FILE *fptr, uint32_t val)
{
	uint8_t buf[2];

	buf[0] = val;
	buf[1] = val>>8;

	return fwrite(buf, 2, 1, fptr);
}

static int writeLE32(FILE *fptr, uint32_t val)
{
	uint8_t buf[4];

	buf[0] = val;
	buf[1] = val>>8;
	buf[2] = val>>16;
	buf[3] = val>>24;

	return fwrite(buf, 4, 1, fptr);
}

int readFlicHeader(FILE *fptr, struct FlicHeader *dst)
{
	uint8_t buf[128];
	int off = 0;

	if (1 != fread(buf, 128, 1, fptr)) {
		perror("Could not read flic header");
		return -1;
	}

	dst->size = getLE32(buf, &off);
	dst->type = getLE16(buf, &off);
	dst->frames = getLE16(buf, &off);
	dst->width = getLE16(buf, &off);
	dst->height = getLE16(buf, &off);
	dst->depth = getLE16(buf, &off);
	dst->flags = getLE16(buf, &off);
	dst->speed = getLE32(buf, &off);
	off += 2; // skip reserved word
	dst->created = getLE32(buf, &off);
	dst->creator = getLE32(buf, &off);
	dst->updated = getLE32(buf, &off);
	dst->updater = getLE32(buf, &off);
	dst->aspectx = getLE16(buf, &off);
	dst->aspecty = getLE16(buf, &off);
	off += 38;
	dst->oframe1 = getLE32(buf, &off);
	dst->oframe2 = getLE32(buf, &off);

	if (dst->oframe1 == 0) {
		fprintf(stderr, "Warning: FLC file with zero frame offset - setting to 128\n");
		dst->oframe1 = 128;
	}

	if ((dst->width == 0) || (dst->height == 0)) {
		fprintf(stderr, "Warning: FLC file has zero width and/or height. Assuming 640x480\n");
		dst->width = 640;
		dst->height = 480;
	}

	return 0;
}

int writeFlicHeader(FILE *fptr, const struct FlicHeader *src)
{
	writeLE32(fptr, src->size);

	writeLE16(fptr, src->type);
	writeLE16(fptr, src->frames);
	writeLE16(fptr, src->width);
	writeLE16(fptr, src->height);
	writeLE16(fptr, src->depth);
	writeLE16(fptr, src->flags);

	writeLE32(fptr, src->speed);

	writeLE16(fptr, src->reserved1);

	writeLE32(fptr, src->created);
	writeLE32(fptr, src->creator);
	writeLE32(fptr, src->updated);
	writeLE32(fptr, src->updater);

	writeLE16(fptr, src->aspectx);
	writeLE16(fptr, src->aspecty);

	writeZeros(fptr, sizeof(src->reserved2));

	writeLE32(fptr, src->oframe1);
	writeLE32(fptr, src->oframe2);

	writeZeros(fptr, sizeof(src->reserved3));

	return 0;
}

int writeFrameHeader(FILE *fptr, const struct FlicFrameHeader *src)
{
	writeLE32(fptr, src->size);
	writeLE16(fptr, src->type);
	writeLE16(fptr, src->chunks);
	writeZeros(fptr, 8);

	return 0;
}

uint32_t writeFlicChunkHeader(FILE *fptr, const struct FlicChunkHeader *src)
{
	writeLE32(fptr, src->size);
	writeLE16(fptr, src->type);
	return 6;
}


uint32_t writeFlicChunk(FILE *fptr, uint16_t type, uint8_t *data, uint32_t size)
{
	writeLE32(fptr, size + 6);
	writeLE16(fptr, type);
	fwrite(data, size, 1, fptr);
	return 6 + size;
}

static int write_chunk_color64_full(FILE *fptr, palette_t *pal)
{
	uint8_t buf[2048];
	int pos = 0;
	int i;

	// Packet count (LE16) : 1
	buf[pos] = 1; pos++;
	buf[pos] = 0; pos++;

	buf[pos] = 0; pos++; // skip
	buf[pos] = 0; pos++; // change (0 means 256)

	for (i=0; i<256; i++) {
		buf[pos] = pal->colors[i].r; pos++;
		buf[pos] = pal->colors[i].g; pos++;
		buf[pos] = pal->colors[i].b; pos++;
//		printf("%d:[%d,%d,%d], ", i, pal->colors[i].r, pal->colors[i].g, pal->colors[i].b);
	}
printf("Palette chunk payload size: %d\n", pos);
	return writeFlicChunk(fptr, CHK_COLOR_256, buf, pos);
}

// Full uncompressed image chunk
static int write_chunk_copy(FlicFile *ff, const uint8_t *pixels)
{
	struct FlicChunkHeader hd = {
		.type = CHK_FLI_COPY,
		.size = 6 + ff->pixels_allocsize,
	};

	writeFlicChunkHeader(ff->fptr, &hd);
	if (fwrite(pixels, ff->pixels_allocsize, 1, ff->fptr) != 1) {
		return -1;
	}

	return hd.size;
}

int readFrameHeader(FILE *fptr, struct FlicFrameHeader *dst)
{
	uint8_t buf[16];
	int off = 0;

	if (1 != fread(buf, 16, 1, fptr)) {
		if (!feof(fptr)) {
			perror("Could not read frame header");
		}
		return -1;
	}

	dst->size = getLE32(buf, &off);
	dst->type = getLE16(buf, &off);
	dst->chunks = getLE16(buf, &off);

	if (dst->type != 0xF1FA) {
		fprintf(stderr, "Bad frame header type %04x\n", dst->type);
		return -1;
	}
	if (dst->size < 16) {
		fprintf(stderr, "Impossible frame chunk size\n");
		return -1;
	}

	return 0;
}

int readChunkHeader(FILE *fptr, struct FlicChunkHeader *dst)
{
	uint8_t buf[6];
	int off = 0;

	if (1 != fread(buf, 6, 1, fptr)) {
		perror("Could not read frame header");
		return -1;
	}

	dst->size = getLE32(buf, &off);
	dst->type = getLE16(buf, &off);

	return 0;
}

static int flic_findFrame1(FlicFile *ff)
{
	long offset, initial_offset;
	struct FlicChunkHeader ckhd;

	if (g_verbose) {
		printf("Looking for first Frame chunck...");
	}

	initial_offset = ftell(ff->fptr);

	// Start after file header
	fseek(ff->fptr, 128, SEEK_SET);

	while(1) {
		offset = ftell(ff->fptr);
		if (readChunkHeader(ff->fptr, &ckhd)) {
			if (feof(ff->fptr)) {
				break;
			}
			fseek(ff->fptr, initial_offset, SEEK_SET);
			return -1;
		}

		// Found it!
		if (ckhd.type == FRAME_HEADER_TYPE) {
			ff->header.oframe1 = offset;
			fseek(ff->fptr, initial_offset, SEEK_SET);
			return 0;
		}

		fseek(ff->fptr, offset + ckhd.size, SEEK_SET);
		if (feof(ff->fptr))
			break;
	}

	fseek(ff->fptr, initial_offset, SEEK_SET);

	return -1;
}


static int flic_findFrame2(FlicFile *ff)
{
	long frame_offset, initial_offset;
	struct FlicFrameHeader hd;
	int count = 0;

	if (g_verbose) {
		printf("Seeking for frame 2...");
	}

	initial_offset = ftell(ff->fptr);

	// Start at first frame
	fseek(ff->fptr, ff->header.oframe1, SEEK_SET);

	while(1) {
		frame_offset = ftell(ff->fptr);
		if (readFrameHeader(ff->fptr, &hd)) {
			if (feof(ff->fptr)) {
				break;
			}
			fseek(ff->fptr, initial_offset, SEEK_SET);
			return -1;
		}

		count++;

		if (count == 2) {
			if (g_verbose) {
				printf(" Frame %d found at %ld\n", count, frame_offset);
			}
			ff->header.oframe2 = frame_offset;
			return 0;
		}

		fseek(ff->fptr, frame_offset + hd.size, SEEK_SET);
		if (feof(ff->fptr))
			break;
	}

	fseek(ff->fptr, initial_offset, SEEK_SET);

	return 0;
}

void flic_close(FlicFile *ff)
{
	if (ff) {
		// in encoding contextx, this will be a copy of the first frame.
		// use it to generate a ring frame before closing the file.
		if (ff->pixels_frame1) {
			if (g_verbose) {
				printf("Appending ring frame...\n");
			}
			flic_appendFrame(ff, ff->pixels_frame1, &ff->palette_frame1);
		}

		if (ff->fptr) {
			fclose(ff->fptr);
		}
		if (ff->pixels) {
			free(ff->pixels);
		}
		memset(ff, 0, sizeof(FlicFile));
		free(ff);
	}
}

/* Return true if this looks like a flic file (checks for magic number) */
int isFlicFile(const char *filename)
{
	FILE *fptr;
	uint8_t buf[2];
	uint16_t magic;

	fptr = fopen(filename, "rb");
	if (!fptr) {
		return 0;
	}

	// type
	fseek(fptr, 4, SEEK_SET);
	if (1 != fread(buf, 2, 1, fptr)) {
		fclose(fptr);
		return 0;
	}
	fclose(fptr);

	magic = buf[0] | buf[1] << 8;

	if (magic == FLC_MAGIC)
		return 1;
	if (magic == FLI_MAGIC)
		return 1;

	return 0;
}

FlicFile *flic_open(const char *filename)
{
	FlicFile *ff;
	int res;

	ff = calloc(1, sizeof(FlicFile));
	if (!ff) {
		perror("Could not allocate FlicFile");
		return NULL;
	}

	ff->fptr = fopen(filename, "rb");
	if (!ff->fptr) {
		goto error;
	}

	if (readFlicHeader(ff->fptr, &ff->header)) {
		goto error;
	}

	if (ff->header.type == FLI_MAGIC) {
		if (g_verbose) {
			printf("FLI file detected\n");
		}
		// Convert speed to milliseconds
		ff->header.speed = ff->header.speed * 1000 / 70;
		ff->header.oframe1 = 128;
		flic_findFrame2(ff);
	} else if (ff->header.type == FLC_MAGIC) {
		if (g_verbose) {
			printf("FLIC file detected\n");
		}

		if ((ff->header.oframe1 > ff->header.size) || (ff->header.oframe2 > ff->header.size)) {
			fprintf(stderr, "Warning: Frame offsets out of range\n");
			if (flic_findFrame1(ff)) {
				fprintf(stderr, "Could not find the first frame chunk\n");
				goto error;
			}
		}

		flic_findFrame2(ff);


	} else {
		fprintf(stderr, "Unknown flic type %02x\n", ff->header.type);
		goto error;
	}

	if (ff->header.frames == 0) {
		fprintf(stderr, "Warning: FLC has no frame count. Reading until the EOF\n");
	} else {
		if (g_verbose) {
			printf("Frames: %d\n", ff->header.frames);
		}
	}

	if (ff->header.depth == 0) {
		fprintf(stderr, "Warning: FLIC color depth is 0 - assuming this means 8");
		ff->header.depth = 8;
	}

	if (ff->header.depth != 8) {
		// For FLI files, values other than 8 are not supposed to exist. Complain,
		// but assume depth is 8 an acontinue
		if (ff->header.type == FLI_MAGIC) {
			fprintf(stderr, "Warning: FLI file with invalid (%d) depth - assuming 8\n", ff->header.depth);
			ff->header.depth = 8;
		} else {
			fprintf(stderr, "Error: FLC with depth %d not supported\n", ff->header.depth);
			goto error;
		}
	}


	fseek(ff->fptr, ff->header.oframe1, SEEK_SET);

	ff->pixels_allocsize = ff->header.width * ff->header.height;
	ff->pitch = ff->header.width;
	ff->pixels = calloc(1, ff->pixels_allocsize);
	if (!ff->pixels) {
		perror("Could not allocate pixel buffer for FLIC");
		goto error;
	}

	if (ff->header.frames == 0) {
		fprintf(stderr, "Warning: FLIC with frame count of zero. Counting frames...\n");
		res = flic_countFrames(ff);
		if (res < 0) {
			fprintf(stderr, "Error counting frames\n");
			goto error;
		}
		ff->header.frames = res;
	}

	return ff;

error:
	if (ff) {
		if (ff->pixels) {
			free(ff->pixels);
		}
		if (ff->fptr) {
			fclose(ff->fptr);
		}
		free(ff);
	}
	return NULL;
}

/* Copy pixels from a buffer */
static void flic_memcpy(FlicFile *ff, int x, int y, const uint8_t *src, int size)
{
	if ((y < 0) || (x < 0) || (y >= ff->header.height)) {
		fprintf(stderr, "Warning: flic_memcpy args out of range\n");
		return;
	}

	if (x + size > ff->header.width) {
		fprintf(stderr, "Warning: flic_memcpy attempting to write past image width\n");
		return;
	}


	memcpy(ff->pixels + y * ff->pitch + x, src, size);
}

/* Set pixels using value(s) from an arbitrary size buffer, repeated count times */
static void flic_memset_buf(FlicFile *ff, int x, int y, const uint8_t *buf, int bufsize, int count)
{
	int i;

	if ((y < 0) || (x < 0) || (y >= ff->header.height)) {
		fprintf(stderr, "Warning: flic_memset_buf args out of range\n");
		return;
	}

	if (x + count*bufsize > ff->header.width) {
		fprintf(stderr, "Warning: flic_memset_buf attempting to write past image width\n");
		return;
	}

	if (bufsize == 1) {
		memset(ff->pixels + y * ff->pitch + x, buf[0], count);
	} else {
		for (i=0; i<count; i++) {
			memcpy(ff->pixels + y * ff->pitch + x + i*2, buf, bufsize);
		}
	}
}

/* Black chunk : Set all pixels to 0 */
static int handle_black_chunk(FlicFile *ff, struct FlicChunkHeader *header)
{
	memset(ff->pixels, 0, ff->pixels_allocsize);
	return 0;
}

/* 24/18-bit palette */
static int handle_palette(FlicFile *ff, struct FlicChunkHeader *header)
{
	uint16_t packet_count;
	int i,j;
	uint8_t tmp[3];
	uint8_t skip, replace;
	int toCopy;
	int dst = 0;

	if (fread(tmp, 2, 1, ff->fptr) != 1) {
		perror("Error reading palette\n");
		return -1;
	}

	packet_count = tmp[0] | (tmp[1] << 8);
	if (g_verbose) { printf("Palette packets: %d\n", packet_count); }

	for (i=0; i<packet_count; i++) {
		if (fread(&skip, 1, 1, ff->fptr) != 1) {
			perror("Error reading palette\n");
			return -1;
		}
		if (fread(&replace, 1, 1, ff->fptr) != 1) {
			perror("Error reading palette\n");
			return -1;
		}

		// replace = 0 means 256
		toCopy = (replace == 0) ? 256 : replace;

		if (g_verbose) { printf("Skip: %d, replace: %d - len: %d, ", skip, replace, toCopy); }

		dst += skip;

		for (j=0; j<toCopy; j++) {
			if (fread(&tmp, 3, 1, ff->fptr) != 1) {
				perror("Error reading palette\n");
				return -1;
			}
//			printf("Color %d: rgb=%d,%d,%d\n", dst, tmp[0],tmp[1],tmp[2]);

			if (header->type == CHK_COLOR_64) {
				ff->palette.colors[dst].r = tmp[0] * 255 / 63;
				ff->palette.colors[dst].g = tmp[1] * 255 / 63;
				ff->palette.colors[dst].b = tmp[2] * 255 / 63;
			} else {
				ff->palette.colors[dst].r = tmp[0];
				ff->palette.colors[dst].g = tmp[1];
				ff->palette.colors[dst].b = tmp[2];
			}

			dst++;
		}

		if (g_verbose) { printf("\n"); }
	}

	if (dst > ff->palette.count) {
		if (g_verbose) {
			printf("Total colors used now %d\n", dst);
		}
		ff->palette.count = dst;
	}

	return 0;
}

/* Byte run : full image comressed with RLE */
static int handle_brun(FlicFile *ff, struct FlicChunkHeader *header)
{
	uint8_t pcount, p;
	int8_t count, l;
	int y, x;
	uint8_t tmp[ff->header.width];
	uint8_t r;

//	printf("BRUN\n");

	for (y=0; y<ff->header.height; y++) {
		x = 0;

	//	printf("\nY=%d, ", y);

		// Get line packet count
		if (fread(&pcount, 1, 1, ff->fptr) != 1) {
			return -1;
		}

	//	printf("pcount = %d, ", pcount);
		// Use width instead of packet count for flc support

		for (p=0; x<ff->header.width; p++) {

			// Get the count byte.
			if (fread(&count, 1, 1, ff->fptr) != 1) { return -1; }

			if (count < 0) {
				// literal run
				l = abs(count);
	//			printf("LIT %d, ", l);
				if (fread(tmp, l, 1, ff->fptr) != 1) { return -1; }

				flic_memcpy(ff, x, y, tmp, l);
				x += l;
			} else {
				// RLE run. Read byte to repeat
				if (fread(&r, 1, 1, ff->fptr) != 1) { return -1; }
				flic_memset_buf(ff, x, y, &r, 1, count);
				x += count;
	//			printf("RLE %d, ", count);
			}

		}

	}

	return 0;
}

static int handle_fli_copy(FlicFile *ff, struct FlicChunkHeader *header)
{
	int y;

	for (y=0; y<ff->header.height; y++) {
		if (fread(ff->pixels + y * ff->pitch, ff->header.width, 1, ff->fptr) != 1) { return -1; }
	}

	return 0;
}

/* Word-oriented RLE */
static int handle_delta_flc(FlicFile *ff, struct FlicChunkHeader *header)
{
	uint8_t buf[2];
	uint16_t y,x;
	uint16_t lines;
	int ln, p;
	uint8_t pcount, skip;
	int8_t ptype;
	uint8_t tmp[ff->header.width];
	int16_t op;

	// Get the number of lines in the chunk
	if (fread(buf, 2, 1, ff->fptr) != 1) { return -1; }
	lines = buf[0] | (buf[1] << 8);

	if (g_verbose > 1) { printf("DELTA FLC. Lines: %d\n", lines); }

	y = 0;
	for (ln = 0; ln < lines ; ln++, y++) {
		x = 0;
		if (g_verbose > 1) { printf("\nline=%d,y=%d,", ln, y); }

		pcount = 0;

		do {
			// Get opcodes
			if (fread(buf, 2, 1, ff->fptr) != 1) { return -1; }
			op = buf[0] | buf[1] << 8;

			if ((op & 0xC000) == 0x0000) {
				if (g_verbose > 1) { printf("Packet count=%d\n", pcount); }
				pcount = op;
				break; // always the last or only opcode
			} else if ((op & 0xC000) == 0x8000) {
				if (g_verbose > 1) { printf("Last pixel=0x%02x,", op&0xff); }
				ff->pixels[y*ff->pitch+ff->header.width-1] = op & 0xff;
			} else if ((op & 0xC000) == 0xC000) {
				if (g_verbose > 1) { printf("skip_lines=%d", -op); }
				y += -op;
			} else {
				fprintf(stderr, "Error: Undefined opcode 0x%04x\n", op);
				return -1;
			}
		}
		while(1);


		for (p=0; p<pcount; p++) {
			if (g_verbose > 1) { printf("  Packet %d/%d : ", p+1, pcount); }
			// column count
			if (fread(&skip, 1, 1, ff->fptr) != 1) { return -1; }
			x += skip;
			if (g_verbose > 1) { printf("{skip:%d,",skip); }

			// packet type
			if (fread(&ptype, 1, 1, ff->fptr) != 1) { return -1; }

			if (g_verbose > 1) { printf("ptype=%d, ", ptype); }

			// copy 'count' literal words
			if (ptype > 0) {
				if (g_verbose > 1) { printf("LIT=%d} ", ptype * 2); }
				fflush(stdout);
				if (ptype > ff->header.width) {
					fprintf(stderr, "Literal too long\n");
					return -1;
				}
				if (fread(tmp, ptype * 2, 1, ff->fptr) != 1) { return -1; }
				flic_memcpy(ff, x, y, tmp, ptype * 2);
				x += ptype * 2;
			} else if (ptype < 0) {
				ptype = -ptype;
				// get value to repeat
				if (fread(buf, 2, 1, ff->fptr) != 1) { return -1; }
				flic_memset_buf(ff,x,y,buf,2,ptype);
				x += ptype * 2;
			}

			if (g_verbose > 1) { printf("}\n"); }
		}


	}

	return 0;
}


/* Byte-oriented delta compression */
static int handle_delta_fli(FlicFile *ff, struct FlicChunkHeader *header)
{
	uint8_t buf[2];
	uint16_t y,x;
	uint16_t lines;
	int ln, p;
	uint8_t pcount, skip;
	int8_t rle_count;
	uint8_t tmp[ff->header.width];
	int repeat;
	uint8_t val;

	// Get the line number of the first line with differences
	if (fread(buf, 2, 1, ff->fptr) != 1) { return -1; }
	y = buf[0] | (buf[1] << 8);

	// Get the number of lines in the chunk
	if (fread(buf, 2, 1, ff->fptr) != 1) { return -1; }
	lines = buf[0] | (buf[1] << 8);

	if (g_verbose > 1) { printf("DELTA FLI. Start line: %d, lines: %d\n", y, lines); }

	for (ln = 0; ln < lines ; ln++, y++) {

		x = 0;
		if (g_verbose > 1) { printf("\nline=%d, ", ln); }

		// Line packet count
		if (fread(&pcount, 1, 1, ff->fptr) != 1) { return -1; }
		if (g_verbose > 1) { printf("pcount=%d, ", pcount); }

		for (p=0; p<pcount && x<ff->header.width; p++) {
			// column skip count
			if (fread(&skip, 1, 1, ff->fptr) != 1) { return -1; }
			x += skip;

			if (g_verbose > 1) { printf("{skip:%d,",skip); }

			if (fread(&rle_count, 1, 1, ff->fptr) != 1) { return -1; }


			if (rle_count < 0) { // rle
				repeat = abs(rle_count);
				if (fread(&val, 1, 1, ff->fptr) != 1) { return -1; }
				flic_memset_buf(ff, x, y, &val, 1, repeat);
				x += repeat;
				if (g_verbose > 1) { printf("RLE=%d} ", repeat); }
			} else {
				// rle_count can be zero
				if (rle_count > 0) {
					if (fread(tmp, rle_count, 1, ff->fptr) != 1) { return -1; }
					flic_memcpy(ff, x, y, tmp, rle_count);
					x += rle_count;
				}
				if (g_verbose > 1) { printf("LIT=%d} ", rle_count); }
			}

		}
	}

	return 0;
}

int flic_countFrames(FlicFile *ff)
{
	long frame_offset, initial_offset;
	struct FlicFrameHeader hd;
	int count = 0;

	if (g_verbose) {
		printf("Counting frames...");
	}

	initial_offset = ftell(ff->fptr);

	// Start at first frame
	fseek(ff->fptr, ff->header.oframe1, SEEK_SET);

	while(1) {
		frame_offset = ftell(ff->fptr);
		if (readFrameHeader(ff->fptr, &hd)) {
			if (feof(ff->fptr)) {
				break;
			}
			fseek(ff->fptr, initial_offset, SEEK_SET);
			return -1;
		}

		count++;
		printf("  Frame %d found at %ld", count, frame_offset);

		fseek(ff->fptr, frame_offset + hd.size, SEEK_SET);
		if (feof(ff->fptr))
			break;
	}

	fseek(ff->fptr, initial_offset, SEEK_SET);

	return count;
}

int flic_readOneFrame(FlicFile *ff, int loop)
{
	int i;
	struct FlicChunkHeader chunkHeader;
	long chunk_offset;
	long frame_offset;

	// Handle looping
	if (loop) {
		if (ff->cur_frame >= ff->header.frames) {
			if (g_verbose) {
				printf("Looping\n");
			}
			ff->cur_frame = 1;
			fseek(ff->fptr, ff->header.oframe2, SEEK_SET);
		}
	}

	frame_offset = ftell(ff->fptr);
	if (readFrameHeader(ff->fptr, &ff->frameHeader)) {
		printf("%d read header failed\n", ff->cur_frame);
		return -1;
	}

	if (g_verbose) {
		printf("Frame %d/%d size %d, type: %04X, chunks: %d\n",
			ff->cur_frame+1, ff->header.frames,
			ff->frameHeader.size, ff->frameHeader.type, ff->frameHeader.chunks);
	}

	for (i=0; i<ff->frameHeader.chunks; i++) {

		chunk_offset = ftell(ff->fptr);
		if (readChunkHeader(ff->fptr, &chunkHeader)) {
			printf("frame %d read chunk header failed\n", ff->cur_frame);
			return -1;
		}
		if (g_verbose) {
			printf("  Chunk size %d, type %04X\n", chunkHeader.size, chunkHeader.type);
		}


		switch (chunkHeader.type)
		{
			case CHK_BLACK:
				if (handle_black_chunk(ff, &chunkHeader)) {
					return -1;
				}
				break;

			case CHK_COLOR_256:
			case CHK_COLOR_64:
				if (handle_palette(ff, &chunkHeader)) {
					fprintf(stderr, "Error in palette chunk\n");
					return -1;
				}
				break;

			case CHK_BYTE_RUN:
				if (handle_brun(ff, &chunkHeader)) {
					fprintf(stderr, "Error in byte run chunk\n");
					return -1;
				}
				break;

			case CHK_DELTA_FLI:
				if (handle_delta_fli(ff, &chunkHeader)) {
					fprintf(stderr, "Error in delta fli chunk\n");
					return -1;
				}
				break;

			case CHK_DELTA_FLC:
				if (handle_delta_flc(ff, &chunkHeader)) {
					fprintf(stderr, "Error in delta flc chunk\n");
					return -1;
				}
				break;

			case CHK_FLI_COPY:
				if (handle_fli_copy(ff, &chunkHeader)) {
					fprintf(stderr, "Error in fli copy chunk\n");
					return -1;
				}
				break;

			case CHK_FL_MINI:
				break;

			default:
				fprintf(stderr, "Warning: skipping chunk type 0x%04x\n", chunkHeader.type);
				break;
		}

		fseek(ff->fptr, chunk_offset + chunkHeader.size, SEEK_SET);
	}

	/* Always seek to next framme according to size from Frame header.
	 * Fixes some files where the subchunks do not fill the frame size
	 * completely or exceeds it... */
	fseek(ff->fptr, frame_offset + ff->frameHeader.size, SEEK_SET);
	ff->cur_frame++;

	return 0;
}

void printFlicInfo(FlicFile *ff)
{
	if (ff->header.type == FLC_MAGIC) {
		printf("FLC file {\n");
	} else if (ff->header.type == FLI_MAGIC) {
		printf("FLI file {\n");
	} else {
		printf("Unknown {\n");
	}

	printf("  File size: %d\n", ff->header.size);
	printf("  Frames: %d\n", ff->header.frames);
	printf("  Image size: %d x %d\n", ff->header.width, ff->header.height);
	printf("  Depth: %d\n", ff->header.depth);
	printf("  Flags: 0x%04x\n", ff->header.flags);
	printf("  Delay between frames: %d ms\n", ff->header.speed);
	printf("  First frame offset: %d\n", ff->header.oframe1);
	printf("  Second frame offset: %d\n", ff->header.oframe2);

	printf("}\n");
}

FlicFile *flic_create(const char *filename, int w, int h)
{
	FlicFile *ff;

	ff = calloc(1, sizeof(FlicFile));
	if (!ff) {
		perror("Could not allocate FlicFile");
		return NULL;
	}

	ff->fptr = fopen(filename, "w+");
	if (!ff->fptr) {
		perror(filename);
		free(ff);
		return NULL;
	}

	ff->header.size = 128; // file header
	ff->header.type = FLC_MAGIC;
	ff->header.width = w;
	ff->header.height = h;
	ff->header.depth = 8;
	ff->header.oframe1 = 128;

	ff->pitch = ff->header.width;
	ff->pixels_allocsize = ff->header.width * ff->header.height;
	ff->pixels = calloc(1, ff->pixels_allocsize);
	if (!ff->pixels) {
		perror("Could not allocate pixel buffer for FLIC");
		free(ff);
		return NULL;
	}

	writeFlicHeader(ff->fptr, &ff->header);

	printf("Main header size %ld\n", ftell(ff->fptr));

	return ff;
}

int flic_appendFrame(FlicFile *ff, uint8_t *pixels, palette_t *palette)
{
	struct FlicFrameHeader frameHeader = {
		.type = FRAME_HEADER_TYPE,
		.size = 6,
	};
	long headeroff, endoff;

	if (fseek(ff->fptr, 0, SEEK_END)) {
		perror("could not seek");
		return -1;
	}

	// write a temporary frame header
	headeroff = ftell(ff->fptr);
	writeFrameHeader(ff->fptr, &frameHeader);

	// output frame subchunks...

	// If palette changed, or if this is the first frame, we need to emit a palette chunk
	if (!palettes_match(&ff->palette, palette) || ff->header.frames == 0) {
		frameHeader.chunks++;
		// lazy complete palette chunk - no compression
		frameHeader.size += write_chunk_color64_full(ff->fptr, palette);
		// update copy of palette for future comparison
		memcpy(&ff->palette, palette, sizeof(palette_t));
	}

	// Output a BRUN or COPY chunk on the first frame as we are starting from nothing.
	if (ff->header.frames == 0) {
		frameHeader.chunks++;
		frameHeader.size += write_chunk_copy(ff, pixels);
		// update the "last image" for future comparison
		memcpy(ff->pixels, pixels, ff->pixels_allocsize);
	} else {
		// Otherwise, if a difference was detected, output the most efficient chunk
		// type (TODO!) for now it is all uncompressed
		if (memcmp(ff->pixels, pixels, ff->pixels_allocsize) || ff->header.frames == 0) {
			frameHeader.chunks++;
			frameHeader.size += write_chunk_copy(ff, pixels);

			// update the "last image" for future comparison
			memcpy(ff->pixels, pixels, ff->pixels_allocsize);
		}
	}

	// make a copy of the first frame, for encoding the ring frame later
	if (ff->header.frames == 0) {
		memcpy(&ff->palette_frame1, &ff->palette, sizeof(palette_t));
		ff->pixels_frame1 = calloc(1, ff->pixels_allocsize);
		memcpy(ff->pixels_frame1, ff->pixels, ff->pixels_allocsize);
		if (!ff->pixels_frame1) {
			perror("Could not allocate frame 1 copy");
			return -1;
		}
	}

	// note end position after writing frame subchunks
	endoff = ftell(ff->fptr);

	// check how many bytes were written, write the updated frame header
	frameHeader.size = endoff - headeroff;
	fseek(ff->fptr, headeroff, SEEK_SET);
//	printf("Write frame header at %ld\n", headeroff);
//	printf("size: %ld\n", endoff - headeroff);
	writeFrameHeader(ff->fptr, &frameHeader);

	// update file header
	ff->header.frames++;
	ff->header.size += frameHeader.size;
	fseek(ff->fptr, 0, SEEK_SET);
	writeFlicHeader(ff->fptr, &ff->header);


	return 0;
}

int flic_frameToSprite(const FlicFile *ff, sprite_t *s)
{
	if ((ff->header.width != s->w) || (ff->header.height != s->h)) {
		fprintf(stderr, "Error: flic_frameToSprite size mismatch\n");
		return -1;
	}
	memcpy(s->pixels, ff->pixels, ff->pixels_allocsize);
	memcpy(&s->palette, &ff->palette, sizeof(palette_t));

	return 0;
}

sprite_t *flic_spriteFromFrame(const FlicFile *ff)
{
	sprite_t *s;

	s = allocSprite(ff->header.width, ff->header.height, 256, 0);
	if (s) {
		flic_frameToSprite(ff, s);
	}

	return s;
}

