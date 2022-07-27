#ifndef _sprite_transform_h__
#define _sprite_transform_h__

#include "sprite.h"

void sprite_scaleNearWH(const sprite_t *src, sprite_t *dst, int w, int h);
void sprite_scaleNear(const sprite_t *src, sprite_t *dst, double factor);
void sprite_scale2x(const sprite_t *src, sprite_t *dst);
void sprite_scale3x(const sprite_t *src, sprite_t *dst);

void sprite_rotate(const sprite_t *src, sprite_t *dst, double angle);

#endif
