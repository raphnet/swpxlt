#ifndef _util_h__
#define _util_h__

#include <stdint.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

uint8_t clamp8bit(int val);

#endif // _util_h__
