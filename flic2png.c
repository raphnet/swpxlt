#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "sprite.h"
#include "flic.h"

int g_verbose = 0;

static void printHelp()
{
	printf("Usage: ./flic2png [options] file.[fli,flc]\n");
	printf("\n");
	printf("flic2png read a .FLI or .FLC file and outputs all frames to individual PNG files.\n");

	printf("\nOptions:\n");
	printf(" -h                Print usage information\n");
	printf(" -v                Enable verbose output\n");
	printf(" -o basename       Base filename\n");
	printf("\n");
	printf("Files will be named in the format basename_XXXXX.png where basename is\n");
	printf("what was given using the -o option.\n");
	printf("\n");
	printf("Example:\n");
	printf("  ./flic2png animation.fli -o out/animation\n");
	printf("\n");
	printf("The above will create out/animiation_XXXXX.png files\n");
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
	const char *base = NULL;
	char tmpname[256];
	FlicFile *flic;
	int f;

	while ((opt = getopt(argc, argv, "hvo:")) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
			case 'o': base = optarg; break;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No file specified. See -h for usage.\n");
		return -1;
	}

	if (!base) {
		fprintf(stderr, "No basename specified (-o)\n");
		return -1;
	}

	infilename = argv[optind];
	if (g_verbose) {
		printf("Opening %s...\n", infilename);
	}

	flic = flic_open(infilename);
	if (!flic) {
		fprintf(stderr, "Error in flic file or unsupported flic file\n");
		return -1;
	}

	f = 0;
	while (0 == flic_readOneFrame(flic, 0)) {
		f++;

		sprite_t *s = spriteFromFlicFrame(flic);
		if (s) {
			snprintf(tmpname, sizeof(tmpname), "%s_%05d.png", base, f);
			sprite_savePNG(tmpname, s, 0);
			freeSprite(s);
		}
	}


	flic_close(flic);

	return 0;
}

