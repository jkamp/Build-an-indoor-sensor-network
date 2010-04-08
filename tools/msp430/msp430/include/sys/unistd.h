
/*
 * Copyright (c) 2003 Dmitry Dicky diwil@mail.ru
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
 * $Id: unistd.h,v 1.2 2003/02/07 12:56:30 diwil Exp $
 */

#ifndef	__UNISTD_H__
#define	__UNISTD_H__

/* linker defined vars */
extern int 	_etext;
extern int	__data_start;
extern int	_edata;
extern int	__bss_start;
extern int	__bss_end;
extern int	__stack;
extern int	__noinit_start;
extern int	__noinit_end;
extern int	__data_start_rom;
extern int	__noinit_start_rom;
extern int	__noinit_end_rom;


/* init routines from libgcc */
extern void __attribute__((__noreturn__))	_reset_vector__(void);
extern void __attribute__((__noreturn__))	_copy_data_init__(void);
extern void __attribute__((__noreturn__))	_clear_bss_init__(void);

/* fill up sections. returns data size copied */
extern	int	_init_section__(int section);

#define MSP430_SECTION_BSS	0
#define MSP430_SECTION_DATA	1
#define	MSP430_SECTION_NOINIT	2

#endif
