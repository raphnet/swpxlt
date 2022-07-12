#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "sprite.h"
#include "flic.h"

int g_verbose = 0;

static void printHelp()
{
	printf("Usage: ./flicinfo [options] file.[fli,flc]\n");

	printf("\nOptions:\n");
	printf(" -h                Print usage information\n");
	printf(" -v                Enable verbose output\n");
}

sprite_t *spriteFromFlicFrame(FlicFile *ff)
{
	sprite_t *s;

	s = allocSprite(ff->header.width, ff->header.height, 256, 0);
	if (!s)
		return NULL;

	memcpy(s->pixels, ff->pixels, ff->pixels_allocsize);
	memcpy(&s->palette, &ff->palette, sizeof(palette_t));

	return s;
}

int main(int argc, char **argv)
{
	int opt;
	const char *infilename;
	FlicFile *flic;

	while ((opt = getopt(argc, argv, "hv")) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No file specified. See -h for usage.\n");
		return -1;
	}

	infilename = argv[optind];
	printf("Opening %s...\n", infilename);

	flic = flic_open(infilename);
	if (!flic) {
		fprintf(stderr, "Error in flic file or unsupported flic file\n");
		return -1;
	}

	printFlicInfo(flic);

	flic_close(flic);

	return 0;
}

