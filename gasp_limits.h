#ifndef GASP_LIMITS_H
#define GASP_LIMITS_H

#ifdef __cplusplus
#	include <limits>
#	include <float>
#elif defined( __linux__ )
#	include <linux/limits.h>
#	include <float.h>
#else
#	include <limits.h>
#	include <float.h>
#endif

#define TOVAL(M) (M##0)
#define ISDEF TOVAL

#define ANYDEF(M) (0\
	| TODEF(__##M##__) | TODEF(__##M)\
	| TODEF(_##M##_) | TODEF(_##M) | TODEF(M))

#ifndef SIZEOF_LLONG
#	if TOVAL(__SIZEOF_LLONG__)
#		define SIZEOF_LLONG __SIZEOF_LLONG__
#	elif ISDEF(__SIZEOF_LONG_LONG__)
#		define SIZEOF_LLONG __SIZEOF_LONG_LONG__
#	elif ISDEF(LLONG_MAX) || ISDEF(LONG_LONG_MAX)
#		define SIZEOF_LLONG 8
#	endif
#endif

#ifndef LLONG_WIDTH
#	if (__LLONG_WIDTH__)
#		define LLONG_WIDTH __LLONG_WIDTH__
#	elif (__LONG_LONG_WIDTH__)
#		define LLONG_WIDTH __LONG_LONG_WIDTH__
#	elif (SIZEOF_LLONG)
#		define LLONG_WIDTH (SIZEOF_LLONG * CHAR_BIT)
#	endif
#endif

#ifndef SIZEOF_LONG
#	if (__SIZEOF_LONG__)
#		define SIZEOF_LONG __SIZEOF_LONG__
#	elif (LONG_MAX) > (INT_MAX) && (INT_MAX) > (SHRT_MAX)
#		define SIZEOF_LONG 8
#	elif (LONG_MAX)
#		define SIZEOF_LONG 4
#	endif
#endif

#ifndef LONG_WIDTH
#	if (__LONG_WIDTH__)
#		define LONG_WIDTH __LONG_WIDTH__
#	elif (SIZEOF_LONG)
#		define LONG_WIDTH (SIZEOF_LONG * CHAR_BIT)
#	endif
#endif

#ifndef HAVE_8_BYTE_INT
#	if (SIZEOF_LLONG) >= 8 || (SIZEOF_LONG) >= 8
#		define HAVE_8_BYTE_INT
#		ifndef _LARGEFILE64_SOURCE
#			define _LARGEFILE64_SOURCE
#		endif
#	endif
#endif

#endif
