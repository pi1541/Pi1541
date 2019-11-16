#ifndef types_h
#define types_h

#include <stddef.h>
#include <uspi/types.h>
#include "integer.h"

typedef unsigned long long	u64;

typedef signed long long	s64;

typedef enum {
        LCD_UNKNOWN,
        LCD_1306_128x64,
        LCD_1306_128x32,
        LCD_1106_128x64,
} LCD_MODEL;

typedef enum {
	EXIT_UNKNOWN,
	EXIT_RESET,
	EXIT_CD,
	EXIT_KEYBOARD,
	EXIT_AUTOLOAD
} EXIT_TYPE;

#ifndef Max
#define Max(a,b)	(((a) > (b)) ? (a) : (b))
#endif

#ifndef Min
#define Min(a,b)	(((a) < (b)) ? (a) : (b))
#endif

#endif
