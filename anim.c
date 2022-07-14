#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "anim.h"
#include "flic.h"
#include "globals.h"
#ifdef WITH_GIF_SUPPORT
#include "gif_lib.h"
#endif

#define FRAME_REALLOC_COUNT 2

int anim_addFrame(animation_t *anim, sprite_t *frame)
{
	// Check if there is space first. Initially, allocated_frames is zero,
	// so it will be like malloc.
	if (anim->num_frames >= anim->allocated_frames) {
		anim->frames = realloc(anim->frames, (anim->num_frames + FRAME_REALLOC_COUNT) * sizeof(anim->frames));
		if (!anim->frames) {
			perror("Cound not allocate space for frames\n");
			return -1;
		}
		anim->allocated_frames += FRAME_REALLOC_COUNT;
	}

	anim->frames[anim->num_frames] = frame;
	anim->num_frames++;

	return 0;
}

animation_t *anim_create(void)
{
	animation_t *anim;

	anim = calloc(1, sizeof(animation_t));
	if (!anim) {
		perror("Could not allocate animation structure\n");
	}

	return anim;
}

#ifdef WITH_GIF_SUPPORT
static int anim_addFramesFromGIF(animation_t *anim, const char *filename)
{
	GifFileType *gf;
	GifColorType *color;
	sprite_t *sprite = NULL;
	int err = 0;
	int i,j;
	int y;
	int disposal;

	gf = DGifOpenFileName(filename, &err);
	if (!gf) {
		if (err == D_GIF_ERR_NOT_GIF_FILE) {
			return -1;
		}
		fprintf(stderr, "Could not open gif: %s\n", GifErrorString(err));
		return -1;
	}

	if (GIF_OK != DGifSlurp(gf)) {
		fprintf(stderr, "Could not read gif: %s\n", GifErrorString(err));
		goto error;
	}

	if (gf->ImageCount < 1) {
		fprintf(stderr, "No images in GIF!?\n");
		goto error;
	}

	anim->w = gf->SWidth;
	anim->h = gf->SHeight;

	disposal = DISPOSE_BACKGROUND;

	for (j=0; j<gf->ImageCount; j++) {
//		printf("GIF frame %d:\n", j+1);

		// Create sprite with same size as GIF
		sprite = allocSprite(gf->SWidth, gf->SHeight, gf->SColorMap->ColorCount, 0);
		if (!sprite) {
			fprintf(stderr, "failed to allocate sprite\n");
			goto error;
		}

		// Disposal of previous frame
		if (disposal == DISPOSE_BACKGROUND) {
			memset(sprite->pixels, gf->SBackGroundColor, sprite->w * sprite->h);
		} else if (disposal == DISPOSE_PREVIOUS) {
			if (j == 0) {
				fprintf(stderr, "Warning: Unexpected GIF image disposal method. Cannot restore to previous contents on the first frame! Filling with background color...");
				memset(sprite->pixels, gf->SBackGroundColor, sprite->w * sprite->h);
			} else {
				memcpy(sprite->pixels, anim->frames[anim->num_frames-1]->pixels, sprite->w * sprite->h);
			}
		}

		if (gf->SavedImages[j].ExtensionBlockCount > 0) {
			for (i=0; i<gf->SavedImages[j].ExtensionBlockCount; i++) {
				ExtensionBlock *eb = &gf->SavedImages[j].ExtensionBlocks[i];
				GraphicsControlBlock gcb;
				if (eb->Function == GRAPHICS_EXT_FUNC_CODE) {

//					printf("  Gfx ext block: {\n");
					if (GIF_OK == DGifSavedExtensionToGCB(gf, j, &gcb)) {
						disposal = gcb.DisposalMode;
//						printf("    Disposal: %d\n", gcb.DisposalMode);
//						printf("    DelayTime: %d\n", gcb.DelayTime);
//						printf("    Transparent color: %d\n", gcb.TransparentColor);
						if (gcb.TransparentColor >= 0) {
							sprite->transparent_color = gcb.TransparentColor;
							sprite->flags |= SPRITE_FLAG_USE_TRANSPARENT_COLOR;
						}
						if (anim->delay == 0) {
							anim->delay = gcb.DelayTime * 10;
						}
					}
					else {
//						printf("    ERROR!\n");
					}
//					printf("  }\n");

				} else {
//					printf("  Ext block func 0x%02x\n", eb->Function);
				}
			}
		}

		// Copy colormap
		color = gf->SColorMap->Colors;
		for (i=0; i<gf->SColorMap->ColorCount; i++,color++) {
			palette_setColor(&sprite->palette, i, color->Red, color->Green, color->Blue);
		}

		// Copy image
		for (i=0,y = gf->SavedImages[j].ImageDesc.Top; y < gf->SavedImages[j].ImageDesc.Top + gf->SavedImages[0].ImageDesc.Height; y++,i++) {
			memcpy(sprite->pixels + gf->SavedImages[j].ImageDesc.Left + y * sprite->w,
					gf->SavedImages[j].RasterBits + gf->SavedImages[j].ImageDesc.Width * i,
					gf->SavedImages[j].ImageDesc.Width);
		}

		anim_addFrame(anim, sprite);
		sprite = NULL;

	}

	if (GIF_OK != DGifCloseFile(gf, &err)) {
		fprintf(stderr, "Warning: Could not close gif: %s\n", GifErrorString(err));
	}

	return 0;

error:
	if (gf) {
		if (GIF_OK != DGifCloseFile(gf, &err)) {
			fprintf(stderr, "Could not close gif: %s\n", GifErrorString(err));
		}
	}
	if (sprite) {
		freeSprite(sprite);
	}

	return -1;
}
#endif // WITH_GIF_SUPPORT

int anim_addAllFramesToFlic(const animation_t *anim, FlicFile *output)
{
	sprite_t *screen;
	uint8_t t;
	int i, j;
	uint8_t *src, *dst;

	if (anim->num_frames < 1)
		return 0; // nothing do do

	screen = duplicateSprite(anim->frames[0]);

	for (i=0; i<anim->num_frames; i++) {
		if (anim->frames[i]->flags & SPRITE_FLAG_USE_TRANSPARENT_COLOR) {
			t = anim->frames[i]->transparent_color;
			src = anim->frames[i]->pixels;
			dst = screen->pixels;
			for (j=0; j<screen->w*screen->h; j++) {
				if (*src != t) {
					*dst = *src;
				}
				src++;
				dst++;
			}
			flic_appendFrame(output, screen->pixels, &screen->palette);

		}
		else {
			flic_appendFrame(output, anim->frames[i]->pixels, &anim->frames[i]->palette);
		}
	}

	freeSprite(screen);

	return 0;
}

int anim_addFramesFromFlic(animation_t *anim, const char *filename)
{
	FlicFile *flic;
	sprite_t *sprite;

	if (!isFlicFile(filename)) {
		return -1;
	}

	flic = flic_open(filename);
	if (!flic) {
		return -1;
	}

	if (anim->delay == 0) {
		anim->delay = flic->header.speed;
	}

	anim->w = flic->header.width;
	anim->h = flic->header.height;

	while (0 == flic_readOneFrame(flic, 0)) {
		sprite = flic_spriteFromFrame(flic);
		if (!sprite) {
			flic_close(flic);
			return -1;
		}

		anim_addFrame(anim, sprite);
	}

	flic_close(flic);

	return 0;
}

animation_t *anim_load(const char *filename)
{
	animation_t *anim;

	anim = anim_create();
	if (!anim) {
		return NULL;
	}

	if (0 == anim_addFramesFromFlic(anim, filename)) {
		return anim;
	}

#ifdef WITH_GIF_SUPPORT
	if (0 == anim_addFramesFromGIF(anim, filename)) {
		return anim;
	}
#endif

	// One of the above calls should have worked. None did, return NULL.
	anim_free(anim);

	return NULL;
}

void anim_free(animation_t *anim)
{
	int i;

	if (anim) {
		if (anim->frames) {
			for (i=0; i<anim->num_frames; i++) {
				freeSprite(anim->frames[i]);
			}

			free(anim->frames);
		}
		free(anim);
	}
}


