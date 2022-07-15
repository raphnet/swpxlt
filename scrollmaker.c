#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include "sprite.h"
#include "anim.h"
#include "globals.h"

int g_verbose = 0;

#define MAX_LAYERS	16

#define DEFAULT_OUTPUT_FILE	"scrollmaker.flc"
#define DEFAULT_FRAME_RATE	60

#define DEFAULT_W	320
#define DEFAULT_H	200

enum {
	OPT_ADD_LAYER = 256,
	OPT_FPS,
	OPT_SPD,
	OPT_WIDTH,
	OPT_HEIGHT
};

static struct option long_options[] = {
	{ "help",		no_argument,		0,	'h' },
	{ "verbose",	no_argument,		0,	'v' },
	{ "out",		required_argument,	0,	'o' },
	{ "width",		required_argument,	0,	OPT_WIDTH },
	{ "height",		required_argument,	0,	OPT_HEIGHT },
	{ "spdx",		required_argument,	0,	OPT_SPD },
	{ "addlayer",	required_argument,	0,	OPT_ADD_LAYER },
	{ "fps",		required_argument,	0,	OPT_FPS },
};

static void printHelp()
{
	printf("Usage: ./flicmerge [options] files...\n");
	printf("\nflicmerge reads 1 or more files (FLC/FLI/PNG) and outputs a single\n");
	printf(".FLC file.\n");

	printf("\nGeneral / Global options:\n");
	printf(" -h                      Print usage information\n");
	printf(" -v                      Enable verbose output\n");
	printf(" -width w            Output width\n");
	printf(" -height h           Output height\n");
	printf(" -o, -out filename.flc   Output file. Default: %s\n", DEFAULT_OUTPUT_FILE);
	printf(" -fps value              Output frame rate\n");

	printf("\nOptions for next layer:\n");
	printf(" -spdx speed              Pixels-per-frame horizontal movement for layer\n");

	printf(" -addlayer layerfile      Add layer\n");
}

struct layer {
	sprite_t *sprite;
	int initial_x, initial_y;
	int cur_x, cur_y;
	int speed;
};

struct layer layers[MAX_LAYERS];
int num_layers;

void resetLayerPositions()
{
	int i;

	for (i=0; i<num_layers; i++) {
		layers[i].cur_x = layers[i].initial_x;
		layers[i].cur_y = layers[i].initial_y;
	}
}

int allLayersInInitialPosition()
{
	int i;

	for (i=0; i<num_layers; i++) {
		if (layers[i].cur_x != layers[i].initial_x)
			return 0;
		if (layers[i].cur_y != layers[i].initial_y)
			return 0;
	}

	return 1;
}

void updateLayerPositions()
{
	int i;
	struct layer *l = layers;

	for (i=0; i<num_layers; i++,l++) {
		l->cur_x += l->speed;

		if (l->cur_x >= l->sprite->w) {
			l->cur_x = l->cur_x - l->sprite->w;
		}
		if (l->cur_x <= -l->sprite->w) {
			l->cur_x = l->sprite->w + l->cur_x;
		}

	//	printf("  Layer %d xpos now %d\n", i + 1, l->cur_x);
	}

}

void blitLayers(sprite_t *dst)
{
	int i;
	struct layer *l = layers;
	spriterect_t dstrect;

	// TODO : It is assumed that layers are larger than the canvas right now...

	for (i=0; i<num_layers; i++,l++) {

		dstrect.x = l->cur_x;
		dstrect.y = l->cur_y;

		sprite_copyRect(l->sprite, NULL, dst, &dstrect);

		if (l->cur_x > 0) {
			dstrect.x = l->cur_x - l->sprite->w;
			sprite_copyRect(l->sprite, NULL, dst, &dstrect);
		}

		if (l->cur_x < 0) {
			dstrect.x = l->cur_x + l->sprite->w;
			sprite_copyRect(l->sprite, NULL, dst, &dstrect);
		}

	}

}

int main(int argc, char **argv)
{
	int opt;
	char *e;
	const char *infilename;
	const char *outfilename = DEFAULT_OUTPUT_FILE;
	FlicFile *outflic = NULL;
	FlicFile *flic = NULL;
	sprite_t *img = NULL;
	animation_t *anim = NULL;
	int w = DEFAULT_W, h = DEFAULT_H;
	int fps = 30;
	int next_layer_speed = 0;
	int frameno = 0;

	while ((opt = getopt_long_only(argc, argv, "hvo:w:h:", long_options, NULL)) != -1) {
		switch (opt) {
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;
			case 'o': outfilename = optarg; break;
			case OPT_FPS:
					fps = strtol(optarg, &e, 0);
					if ((e == optarg)||(fps<1)) {
						fprintf(stderr, "invalid fps\n");
						return 1;
					}
					break;
			case OPT_WIDTH:
					w = strtol(optarg, &e, 0);
					if ((e == optarg)||(w<1)) {
						fprintf(stderr, "invalid width\n");
						return 1;
					}
					break;
			case OPT_HEIGHT:
					h = strtol(optarg, &e, 0);
					if ((e == optarg)||(h<1)) {
						fprintf(stderr, "invalid height\n");
						return 1;
					}
					break;
			case OPT_SPD:
					next_layer_speed = strtol(optarg, &e, 0);
					if ((e == optarg)) {
						fprintf(stderr, "invalid speed\n");
						return 1;
					}
					break;
			case OPT_ADD_LAYER:
					if (num_layers >= MAX_LAYERS) {
						fprintf(stderr, "too many layers\n");
						return -1;
					}
					layers[num_layers].speed = -next_layer_speed;
					layers[num_layers].initial_x = 0;
					layers[num_layers].initial_y = 0;
					layers[num_layers].sprite = sprite_load(optarg, 0, SPRITE_LOADFLAG_DROP_TRANSPARENT);
					if (!layers[num_layers].sprite) {
						fprintf(stderr, "Error loading layer from %s\n", optarg);
						return -1;
					}
					num_layers++;
					break;
		}
	}

	if (num_layers<1) {
		fprintf(stderr, "No layers loaded. See -h for usage.\n");
		return -1;
	}

	img = allocSprite(w, h, 256, SPRITE_FLAG_OPAQUE);
	if (!img) {
		fprintf(stderr, "could not allocate canvas\n");
		return -1;
	}

	// all layers are expected to share the same palette at the moment
	sprite_applyPalette(img, &layers[0].sprite->palette);

	outflic = flic_create(outfilename, w, h);
	if (!outflic) {
		fprintf(stderr, "could not create output flic\n");
		return -1;
	}
	if (fps > 0) {
		outflic->header.speed = 1000 / fps;
	}

	resetLayerPositions();

	frameno = 0;
	do {
//		printf("Frame %d\n", frameno);

		updateLayerPositions();

		blitLayers(img);
		if (flic_appendFrame(outflic, img->pixels, &img->palette)) {
			fprintf(stderr, "error writing flic frame\n");
			return -1;
		}

		frameno++;

	} while (!allLayersInInitialPosition());

	printf("Animation loop completed\n");
	printf("Total %d frames\n", frameno);

	flic_close(outflic);
	printf("Wrote %s\n", outfilename);

	freeSprite(img);


	return 0;
}

