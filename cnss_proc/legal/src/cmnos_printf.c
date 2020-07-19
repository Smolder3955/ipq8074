/*
* Copyright (c)2014 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
* $ATH_LICENSE_TARGET_C$
*/
/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "osapi.h"
#if defined(CONFIG_MBOX_SUPPORT)
#include "hw/mbox_reg.h"
#endif
//#define DEBUG_NEW_PRINTF
/*
 * Macros for converting digits to letters and vice versa
 */
#define	to_digit(c)	((c) - '0')
#define is_digit(c)	((unsigned)to_digit(c) <= 9)
#define	to_char(n)	((n) + '0')

#if defined(A_SIMOS_DEVHOST)
#include "stdarg.h"
#else	// A_SIMOS_DEVHOST
#if __XCC__ < 8000
#include "stdarg.h"
#ifdef va_list
#undef va_list
#endif
#define va_list __gnuc_va_list
#else
#define va_list __builtin_va_list
#define va_arg __builtin_va_arg
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_copy __builtin_va_copy
#endif
#endif	// A_SIMOS_DEVHOST

/*
 * Flags used during conversion.
 */
#define	ALT		0x001		/* alternate form */
#define	LADJUST		0x004		/* left adjustment */
#define	LONGDBL		0x008		/* long double */
#define	LONGINT		0x010		/* long integer */
#define	LLONGINT	0x020		/* long long integer */
#define	SHORTINT	0x040		/* short integer */
#define	ZEROPAD		0x080		/* zero (as opposed to blank) pad */
#define	FPT		0x100		/* Floating point number */
#define	GROUPING	0x200		/* use grouping ("'" flag) */
					/* C99 additional size modifiers: */
#define	SIZET		0x400		/* size_t */
#define	PTRDIFFT	0x800		/* ptrdiff_t */
#define	INTMAXT		0x1000		/* intmax_t */
#define	CHARINT		0x2000		/* print char using int format */

LOCAL void
cmnos_write_char(char **pbuf_start, char *buf_end, char c)
{
    if (pbuf_start) {
      if ( *pbuf_start < buf_end) {
         *(*pbuf_start) = c;
         ++(*pbuf_start);
      }
    } else {
        if (c == '\n') {
            A_PUTC('\r');
            A_PUTC('\n');
        } else if (c == '\r') {
        } else {
          A_PUTC(c);
        }
    }
}

LOCAL void (*_putc)(char **pbuf_start, char *buf_end,char c) = cmnos_write_char;

/*
 * Convert an unsigned long to ASCII for printf purposes, returning
 * a pointer to the first character of the string representation.
 * Octal numbers can be forced to have a leading zero; hex numbers
 * use the given digits.
 */
LOCAL char *
__ultoa(unsigned long long val, char *endp, int base, int octzero, const char *xdigs)
{
	char *cp = endp;

	/*
	 * Handle the three cases separately, in the hope of getting
	 * better/faster code.
	 */
	switch (base) {
	case 10:
		if (val < 10) {	/* many numbers are 1 digit */
			*--cp = to_char(val);
			return (cp);
		}
		do {
			*--cp = to_char(val % 10);
			val /= 10;
		} while (val != 0);
		break;
		
	case 8:
		do {
			*--cp = to_char(val & 7);
			val >>= 3;
		} while (val);
		if (octzero && *cp != '0')
			*--cp = '0';
		break;

	case 16:
		do {
			*--cp = xdigs[val & 15];
			val >>= 4;
		} while (val);
		break;

    case 2:
        do {
			*--cp = to_char(val & 1);
			val >>= 1;
		} while (val);
		break;
	default:			/* oops */
		break;
	}
	return (cp);
}

/* Return successive characters in a format string. */
char
cmnos_fmt_next_char(const char **fmtptr)
{
    char ch;

#if defined(CONFIG_FMTSTR_UNPATCHABLE)
    ch = A_SAFE_BYTE_READ(*fmtptr);
#else
    ch = **fmtptr;
#endif

    (*fmtptr)++;

    return ch;
}

/* Return current characters in a format string. */
char
cmnos_fmt_cur_char(const char **fmtptr)
{
    char ch;

#if defined(CONFIG_FMTSTR_UNPATCHABLE)
    ch = A_SAFE_BYTE_READ(*fmtptr);
#else
    ch = **fmtptr;
#endif

    return ch;
}


/*
 * The size of the buffer we use as scratch space for integer
 * conversions, among other things.  We need enough space to
 * write a uintmax_t in octal (plus one byte).
 */
#define	BUF	(sizeof(long long)*8)

/*
 * Non-MT-safe version
 */
LOCAL int
cmnos_vprintf(void (*putc)(char **pbs, char *be, char c), char **pbuf_start, char *buf_end, const char *fmt0, va_list ap)
{
	const char *fmt;		/* format string */
	int ch;			/* character from fmt */
	int n;		/* handy integer (short term usage) */
	char *cp;		/* handy char pointer (short term usage) */
	int flags;		/* flags as above */
	int ret;		/* return value accumulator */
	int width;		/* width from format (%8d), or 0 */
	int prec;		/* precision from format; <0 for N/A */
	char sign;		/* sign prefix (' ', '+', '-', or \0) */

	long long ulval;		/* integer arguments %[diouxX] */
	int base;		/* base for [diouxX] conversion */
	int dprec;		/* a copy of prec if [diouxX], 0 otherwise */
	int realsz;		/* field size expanded by dprec, sign, etc */
	int size;		/* size of converted field or string */
	int prsize;             /* max size of printed field */
	const char *xdigs;     	/* digits for %[xX] conversion */
	char buf[BUF];		/* buffer with space for digits of uintmax_t */
	char ox[2];		/* space for 0x; ox[1] is either x, X, or \0 */
	va_list orgap;          /* original argument pointer */
	int pad;        /* pad */     

	static const char xdigs_lower[16] = "0123456789abcdef";
	static const char xdigs_upper[16] = "0123456789ABCDEF";

	/*
	 * Get the argument indexed by nextarg.   If the argument table is
	 * built, use it to get the argument.  If its not, get the next
	 * argument (and arguments must be gotten sequentially).
	 */
#define GETARG(type) va_arg(ap, type)

	/*
	 * To extend shorts properly, we need both signed and unsigned
	 * argument extraction methods.
	 */
#define	SARG() \
	(flags&LLONGINT ? GETARG(long long): \
	    flags&LONGINT ? GETARG(long) : \
		flags&SHORTINT ? (long)(short)GETARG(int) : \
		flags&CHARINT ? (long)(signed char)GETARG(int) : \
		(long)GETARG(int))
#define	UARG() \
	(flags&LLONGINT ? GETARG(unsigned long long): \
	    flags&LONGINT ? GETARG(unsigned long) : \
		flags&SHORTINT ? (unsigned long)(unsigned short)GETARG(int) : \
		flags&CHARINT ? (unsigned long)(unsigned char)GETARG(int) : \
		(unsigned long)GETARG(unsigned int))

	fmt = (char *)fmt0;
	va_copy(orgap, ap);
	ret = 0;

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {		
		for (cp = (char *)fmt; (ch = cmnos_fmt_cur_char(&fmt)) != '\0' && ch != '%'; ch = cmnos_fmt_next_char(&fmt)){
			putc(pbuf_start, buf_end, ch);
            ret++;
		}
		
		if (ch == '\0')
			goto done;
		cmnos_fmt_next_char(&fmt);		/* skip over '%' */

		flags = 0;
		dprec = 0;
		width = 0;
		prec = -1;
		sign = '\0';
		ox[1] = '\0';

rflag:		ch = cmnos_fmt_next_char(&fmt);
reswitch:	switch (ch) {
		case ' ':
			/*-
			 * ``If the space and + flags both appear, the space
			 * flag will be ignored.''
			 *	-- ANSI X3J11
			 */
			if (!sign)
				sign = ' ';
			goto rflag;
		case '-':
			flags |= LADJUST;
			goto rflag;
		case '+':
			sign = '+';
			goto rflag;
		case '.':
			ch = cmnos_fmt_next_char(&fmt);			
			prec = 0;
			while (is_digit(ch)) {
				prec = 10 * prec + to_digit(ch);
				ch = cmnos_fmt_next_char(&fmt);
			}
			goto reswitch;
		case '0':
			/*-
			 * ``Note that 0 is taken as a flag, not as the
			 * beginning of a field width.''
			 *	-- ANSI X3J11
			 */
			flags |= ZEROPAD;
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = cmnos_fmt_next_char(&fmt);
			} while (is_digit(ch));
			width = n;
			goto reswitch;
		case 'l':
			if (flags & LONGINT) {
				flags &= ~LONGINT;
				flags |= LLONGINT;
			} else
				flags |= LONGINT;
			goto rflag;
		case 'C':
			flags |= LONGINT; /* Do not support WCHAR */
			/*FALLTHROUGH*/
		case 'c':
			*(cp = buf) = GETARG(int);
			size = 1;			
			sign = '\0';
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
			ulval = SARG();
			if (ulval < 0) {
				ulval = -ulval;
				sign = '-';
			}
			base = 10;
			goto number;
		case 'p':
			/*-
			 * ``The argument shall be a pointer to void.  The
			 * value of the pointer is converted to a sequence
			 * of printable characters, in an implementation-
			 * defined manner.''
			 *	-- ANSI X3J11
			 */
			ulval = (int)GETARG(void *);
			base = 16;
			xdigs = xdigs_lower;
			ox[1] = 'x';
			goto nosign;
		case 'S':
		case 's':
            if((cp = GETARG(char *)) == NULL){
				cp = "<null>";
            }
			size = 0;
            while (cp[size] != '\0') size++;			
			sign = '\0';
			break;
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
			ulval = UARG();
			base = 8;
			goto nosign;			
		case 'B':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'b':
			ulval = UARG();
			base = 2;
			goto nosign;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
			ulval = UARG();
			base = 10;
			goto nosign;
		case 'X':
			xdigs = xdigs_upper;
			goto hex;
		case 'x':
			xdigs = xdigs_lower;
hex:

			ulval = UARG();
			base = 16;
			/* leading 0x/X only if non-zero */
			if (flags & ALT && ulval != 0)
				ox[1] = ch;

			/* unsigned conversions */
nosign:			sign = '\0';
			/*-
			 * ``... diouXx conversions ... if a precision is
			 * specified, the 0 flag will be ignored.''
			 *	-- ANSI X3J11
			 */
number:			if ((dprec = prec) >= 0)
				flags &= ~ZEROPAD;

			/*-
			 * ``The result of converting a zero value with an
			 * explicit precision of zero is no characters.''
			 *	-- ANSI X3J11
			 *
			 * ``The C Standard is clear enough as is.  The call
			 * printf("%#.0o", 0) should print 0.''
			 *	-- Defect Report #151
			 */
			cp = buf + BUF;
			if (ulval != 0 || prec != 0 ||
			    (flags & ALT && base == 8)){
				cp = __ultoa(ulval, cp, base,
				    flags & ALT, xdigs);
			}
			size = buf + BUF - cp;

			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
			/* pretend it was %c with argument ch */
			cp = buf;
			*cp = ch;
			size = 1;
			sign = '\0';
			break;
		}

		/*
		 * All reasonable formats wind up here.  At this point, `cp'
		 * points to a string which (if not flags&LADJUST) should be
		 * padded out to `width' places.  If flags&ZEROPAD, it should
		 * first be prefixed by any sign or other prefix; otherwise,
		 * it should be blank padded before the prefix is emitted.
		 * After any left-hand padding and prefixing, emit zeroes
		 * required by a decimal [diouxX] precision, then print the
		 * string proper, then emit zeroes required by any leftover
		 * floating precision; finally, if LADJUST, pad with blanks.
		 *
		 * Compute actual size, so we know how much to pad.
		 * size excludes decimal prec; realsz includes it.
		 */
		realsz = dprec > size ? dprec : size;
		if (sign)
			realsz++;
		if (ox[1])
			realsz += 2;

		prsize = width > realsz ? width : realsz;
        pad =  width - realsz;

		/* right-adjusting blank padding */
		if ((flags & (LADJUST|ZEROPAD)) == 0){
			while(pad-- > 0){
                putc(pbuf_start, buf_end, ' ');
			}
		}

		/* prefix */
		if (sign){
			putc(pbuf_start, buf_end, sign);
			pad--;
		}

		if (ox[1]) {	/* ox[1] is either x, X, or \0 */
			ox[0] = '0';
			putc(pbuf_start, buf_end, ox[0]);
			pad--;
			putc(pbuf_start, buf_end, ox[1]);
			pad--;
		}

		/* right-adjusting zero padding */
		if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD){
			while(pad-- > 0){
                putc(pbuf_start, buf_end, '0');
			}
		}

		while (size-- > 0) {
            ch = *cp++;
            (*putc)(pbuf_start, buf_end,ch);
        }

		/* left-adjusting padding (always blank) */
		if (flags & LADJUST){
			while(pad-- > 0){
                putc(pbuf_start, buf_end, ' ');
			}
		}

		/* finally, adjust ret */
		ret += prsize;
	}
done:

	return (ret);
	/* NOTREACHED */
}

LOCAL int
cmnos_printf(const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);

    if (A_SERIAL_ENABLED()) {
        ret = cmnos_vprintf(_putc,NULL,NULL, fmt, ap);
    } else {
        ret = 0;
    }

    va_end(ap);

    return (ret);
}

LOCAL int
cmnos_snprintf(char *str, unsigned int size,const char *fmt, ...)
{
    va_list ap;
    int ret;
    char *buf_start = str;

    va_start(ap, fmt);

    ret = cmnos_vprintf(_putc,&buf_start,buf_start+size, fmt, ap);

    va_end(ap);

    return (ret);
}

LOCAL void
cmnos_printf_init(void)
{
}

void
cmnos_printf_module_install(struct printf_api *tbl)
{
    tbl->_printf_init        = cmnos_printf_init;
    tbl->_printf             = cmnos_printf;
    tbl->_snprintf           = cmnos_snprintf;
}
