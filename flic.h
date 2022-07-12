#ifndef _flic_h__
#define _flic_h__

#include <stdint.h>

#include "palette.h"
#include "sprite.h"

#define FLC_MAGIC	0xAF12
#define FLI_MAGIC	0xAF11

#define FRAME_HEADER_TYPE	0xF1FA

// Chunk types
#define CHK_COLOR_256   4
#define CHK_DELTA_FLC   7
#define CHK_COLOR_64    11
#define CHK_DELTA_FLI   12
#define CHK_BLACK       13
#define CHK_BYTE_RUN    15
#define CHK_FLI_COPY	16
#define CHK_FL_MINI		18

struct FlicHeader {
	uint32_t size;
	uint16_t type;
	uint16_t frames;
	uint16_t width;
	uint16_t height;
	uint16_t depth;
	uint16_t flags;
	uint32_t speed; // milliseconds for FLC, 1/70s fo FLI
	uint32_t created;
	uint32_t creator; // FLC only
	uint32_t updated; // FLC only
	uint32_t updater; // FLC only
	uint16_t aspectx; // FLC only
	uint16_t aspecty; // FLC only
	uint8_t reserved1[38];
	uint32_t oframe1; // FLC only
	uint32_t oframe2; // FLC only
	uint8_t reserved2[40];
};

struct FlicPrefix {
	uint32_t size;
	uint16_t type;
	uint16_t chunks;
	uint8_t reserved[8];
};

struct FlicFrameHeader {
	uint32_t size;
	uint16_t type;
	uint16_t chunks;
	uint8_t reserved[8];
};

struct FlicChunkHeader {
	uint32_t size;
	uint16_t type;
};

int readFlicHeader(FILE *fptr, struct FlicHeader *dst);
int readFrameHeader(FILE *fptr, struct FlicFrameHeader *dst);
int readChunkHeader(FILE *fptr, struct FlicChunkHeader *dst);

typedef struct _FlicFile {
	FILE *fptr;
	struct FlicHeader header;
	struct FlicFrameHeader frameHeader;
	palette_t palette;
	uint8_t *pixels;
	int pitch;
	int pixels_allocsize;
	uint16_t cur_frame;
} FlicFile;

FlicFile *flic_open(const char *filename);
void flic_close(FlicFile *ff);
int flic_countFrames(FlicFile *ff);
int flic_readOneFrame(FlicFile *ff, int loop);

void printFlicInfo(FlicFile *ff);


#endif
