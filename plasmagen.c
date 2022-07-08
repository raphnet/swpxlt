#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <math.h>
#include "sprite.h"
#include "globals.h"

#define DEFAULT_HEIGHT		128
#define DEFAULT_WIDTH		128
#define DEFAULT_NUM_COLORS	16

int g_verbose = 0;


void printHelp(void)
{
	printf("Usage: ./plasmagen [options]\n");

	printf(" -h                Print usage information\n");
	printf(" -v                Print usage information\n");
	printf(" -W width          Output image width (default: %d)\n", DEFAULT_WIDTH);
	printf(" -H height         Output image height (default: %d)\n", DEFAULT_HEIGHT);
	printf(" -c num_colors     Number of colors to use (default: %d)\n", DEFAULT_NUM_COLORS);
	printf(" -o output_file    Output filename (required)\n");

}

void generateGrayscalePalette(int n_colors, sprite_t *spr)
{
	int i;
	int intensity;

	for (i=0; i<n_colors; i++) {

		intensity = sin(i*M_PI/n_colors) * 255;

		spr->palette.colors[i].r = intensity;
		spr->palette.colors[i].g = intensity;
		spr->palette.colors[i].b = intensity;
	}

	spr->palette.count = n_colors;
}

int applyMinMax(int value, int min, int max)
{
	if (value > max)
		return max;
	if (value < min)
		return min;
	return value;
}

void drawConcentricPlasma(sprite_t *spr)
{
	int x,y,color;
	double d,dx,dy;
	float divisor = 4;

	for (y=0; y<spr->h; y++) {
		for (x=0; x<spr->w; x++) {
			dx = x - spr->w/2 + 0.5;
			dy = y - spr->h/2 + 0.5;
			d = sqrt(dx*dx+dy*dy) / divisor;

			color = (int)d % spr->palette.count; // spr->palette.count / 2 + (spr->palette.count / 2) * sin(d / divisor);
			color = applyMinMax(color, 0, spr->palette.count - 1);

			sprite_setPixel(spr, x, y, color);
		}
	}
}

int main(int argc, char **argv)
{
	int opt;
	int w = DEFAULT_WIDTH, h = DEFAULT_HEIGHT, colors = DEFAULT_NUM_COLORS;
	char *e;
	const char *out_filename = NULL;
	sprite_t *working_image;

	while ((opt = getopt(argc, argv, "hvW:H:o:c:")) != -1) {
		switch (opt)
		{
			case '?': return -1;
			case 'h': printHelp(); return 0;
			case 'v': g_verbose = 1; break;

			case 'c':
					colors = strtol(optarg, &e, 0);
					if (e == optarg) {
						fprintf(stderr, "Invalid value\n");
						return -1;
					}
					if (colors < 2) {
						fprintf(stderr, "At least two colors are required)\n");
						return -1;
					}
					if (colors > 256) {
						fprintf(stderr, "Too many colors (max: 256)\n");
						return -1;
					}

					break;

			case 'W':
					w = strtol(optarg, &e, 0);
					if (e == optarg) {
						fprintf(stderr, "Invalid value\n");
						return -1;
					}
					break;

			case 'H':
					h = strtol(optarg, &e, 0);
					if (e == optarg) {
						fprintf(stderr, "Invalid value\n");
						return -1;
					}
					break;

			case 'o':
				out_filename = optarg;
				break;
		}
	}

	if (!out_filename) {
		fprintf(stderr, "No output filename specified\n");
		return -1;
	}

	working_image = allocSprite(w,h,colors,0);

	generateGrayscalePalette(colors, working_image);
	drawConcentricPlasma(working_image);

//	printSprite(working_image);

	sprite_savePNG(out_filename, working_image, 0);
	freeSprite(working_image);

	return 0;
}
