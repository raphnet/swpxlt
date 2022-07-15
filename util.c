#include "util.h"

uint8_t clamp8bit(int val)
{
	if (val < 0)
		return 0;
	if (val > 255)
		return 255;
	return val;
}


