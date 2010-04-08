#ifndef __LIMITS_H
#define __LIMITS_H

#define MB_LEN_MAX	1		/* Longest multi-byte character */
#define CHAR_BIT	8		/* number of bits in a char */
#define CHAR_MAX	127		/* maximum char value */
#define CHAR_MIN	(-128)		/* mimimum char value */
#define SHRT_MAX	32767		/* maximum (signed) short value */
#define SHRT_MIN	(-32768)	/* minimum (signed) short value */

#define __LONG_MAX__ 2147483647L
#undef LONG_MIN
#define LONG_MIN (-LONG_MAX-1)
#undef LONG_MAX
#define LONG_MAX __LONG_MAX__


#define UCHAR_MAX	255		/* maximum unsigned char value */
#define UCHAR_MIN	0		/* minimum unsigned char value */
#define USHRT_MAX	0xffff		/* maximum unsigned short value */
#define USHRT_MIN	0		/* minimum unsigned short value */
#define ULONG_MAX	0xfffffffful	/* maximum unsigned long value */
#define ULONG_MIN	0		/* minimum unsigned long value */

#define SCHAR_MAX	127		/* maximum signed char value */
#define SCHAR_MIN	(-128)		/* minimum signed char value */
#define INT_MAX		32767		/* maximum (signed) int value */
#define INT_MIN		(-32768)	/* minimum (signed) int value */
#define UINT_MAX	0xffff		/* maximum unsigned int value */
#define UINT_MIN	0		/* minimum unsigned int value */

#define ULONG_LONG_MAX	0xffffffffffffffffull

#ifndef RAND_MAX
#define RAND_MAX	INT_MAX
#endif

#endif

