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

FlicFile *flic_open(const char *filename)
{
	FlicFile *ff ;
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
//	printf("Palette packets: %d\n", packet_count);

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

//		printf("Skip: %d, replace: %d - len: %d\n", skip, replace, toCopy);

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

