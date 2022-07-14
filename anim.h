#ifndef _anim_h__
#define _anim_h__

#include "sprite.h"
#include "flic.h"

typedef struct animation {
	int w,h;
	int num_frames;
	sprite_t **frames;
	int allocated_frames;
	int delay; // time between frames in ms
} animation_t;

animation_t *anim_create(void);
animation_t *anim_load(const char *filename);
void anim_free(animation_t *anim);

// This is add and forget. i.e. This just adds the sprite pointer to an array.
// do not call freeSprite. freeSprite will be called by anim_free.
int anim_addFrame(animation_t *anim, sprite_t *frame);

int anim_addFramesFromFlic(animation_t *anim, const char *filename);
int anim_addAllFramesToFlic(const animation_t *anim, FlicFile *output);

#endif // _anim_h__
