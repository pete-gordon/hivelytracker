/* These are normally defined in the Amiga's types library, but are
   defined here instead to ensure portability with minimal changes to the
   original Amiga source-code
*/

#ifndef EXEC_TYPES_H
#define EXEC_TYPES_H

#include <stdint.h>

#ifndef uint8
# define uint8  uint8_t
# define uint16 uint16_t
# define uint32 uint32_t
# define int8   uint8_t
# define int16  uint16_t
# define int32  uint32_t
#endif
typedef double			float64;
typedef char			TEXT;
typedef int			BOOL;
typedef long			LONG;
typedef unsigned long		ULONG;
#define FALSE 0
#define TRUE 1
#define CONST const

#endif
