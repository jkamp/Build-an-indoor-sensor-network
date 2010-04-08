/*
 * Copyright (c) 2001,2002 Dmitry Dicky diwil@eis.ru
 *                         Chris Liechti cliechti@users.sourceforge.net
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: signal.h,v 1.15 2006/11/15 14:34:57 coppice Exp $
 */

#if !defined(__SIGNAL_H_)
#define __SIGNAL_H_

#include <iomacros.h>

#if defined(__MSP430X__)
#define INTERRUPT_VECTOR_SLOTS      32
#define RESET_VECTOR                62
#else
#define INTERRUPT_VECTOR_SLOTS      16
#define RESET_VECTOR	            30
#endif

#if !defined(_GNU_ASSEMBLER_)

#define Interrupt(x) void __attribute__((interrupt (x)))
#define INTERRUPT(x) void __attribute__((interrupt (x)))
#define interrupt(x) void __attribute__((interrupt (x)))

#define wakeup  __attribute__((wakeup))
#define Wakeup  __attribute__((wakeup))
#define WAKEUP  __attribute__((wakeup))

#define enablenested    __attribute__((signal))
#define EnableNested    __attribute__((signal))
#define ENABLENESTED    __attribute__((signal))

/* Enable/Disable interrupts */
#define eint()  __asm__ __volatile__("eint"::)
#define dint()  __asm__ __volatile__("dint"::)

/* IAR compatibility functions */
#define _EINT()	                eint()
#define __enable_interrupt()    eint()
#define _DINT()                 dint()
#define __disable_interrupt()   dint()

#define INTERRUPT_VECTORS \
	void *InterruptVectors[INTERRUPT_VECTOR_SLOTS] __attribute__ ((section(".vectors")))

/* Declare interrupt service routine with no interrupt vector assigned. */
#define NOVECTOR	    0xFF

#define _RESET() void __attribute__ ((naked)) _reset_vector__(void)

#define UNEXPECTED() interrupt (NOVECTOR) _unexpected_(void)

#else   /*_GNU_ASSEMBLER_ / assember definitions*/

/* Double macro trick to achieve that the x gets expanded*/
#define interrupt(x) xinterrupt(x)
#define xinterrupt(x) vector_##x: .global vector_##x

/* Direct generation of the labels is impossible, so just replace the dummy ones
   with the real ones through those defs:*/
#if INTERRUPT_VECTOR_SLOTS == 32
#define vector_0  vector_ffc0
#define vector_2  vector_ffc2
#define vector_4  vector_ffc4
#define vector_6  vector_ffc6
#define vector_8  vector_ffc8
#define vector_10 vector_ffca
#define vector_12 vector_ffcc
#define vector_14 vector_ffce
#define vector_16 vector_ffd0
#define vector_18 vector_ffd2
#define vector_20 vector_ffd4
#define vector_22 vector_ffd6
#define vector_24 vector_ffd8
#define vector_26 vector_ffda
#define vector_28 vector_ffdc
#define vector_30 vector_ffde
#define vector_32 vector_ffe0
#define vector_34 vector_ffe2
#define vector_36 vector_ffe4
#define vector_38 vector_ffe6
#define vector_40 vector_ffe8
#define vector_42 vector_ffea
#define vector_44 vector_ffec
#define vector_46 vector_ffee
#define vector_48 vector_fff0
#define vector_50 vector_fff2
#define vector_52 vector_fff4
#define vector_54 vector_fff6
#define vector_56 vector_fff8
#define vector_58 vector_fffa
#define vector_60 vector_fffc
#define vector_62 vector_fffe
#else
#define vector_0  vector_ffe0
#define vector_2  vector_ffe2
#define vector_4  vector_ffe4
#define vector_6  vector_ffe6
#define vector_8  vector_ffe8
#define vector_10 vector_ffea
#define vector_12 vector_ffec
#define vector_14 vector_ffee
#define vector_16 vector_fff0
#define vector_18 vector_fff2
#define vector_20 vector_fff4
#define vector_22 vector_fff6
#define vector_24 vector_fff8
#define vector_26 vector_fffa
#define vector_28 vector_fffc
#define vector_30 vector_fffe
#endif

#endif /*_GNU_ASSEMBLER_*/

#endif
