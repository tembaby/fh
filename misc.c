/* $Id: misc.c,v 1.1.1.2 2002/04/08 01:52:32 te Exp $ */

/*
 * Copyright (c) 2002 Tamer Embaby <tsemba@menanet.net>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sun Jan 21 23:19:34 EET 2001
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WINDOWS)
# include <windows.h>
# define inline __inline
# define strncasecmp(s,d,n) 	stricmp(s,d)
#endif

/* LWS, Linear White Space.  As per RFC 2068. SP|TAB|LF */
char *
skiplws(str)
	char *str;
{
	char *s = str;

	while (*s && (*s == ' ' || *s == '\t' || *s == '\n' ) )
		s++;
	return s;
}

/* Based on the source of strstr(3) */
char *
strncasestr(s, find, slen)
	char *s, *find;
	int slen;
{
	char c, sc;
	register char *sp = s;
	int findlen;

	if ((c = *find++)) {
		findlen = strlen(find);
		do {
			do {
				sc = *sp++;
				if ((slen - (sp - s)) < findlen)
					return NULL;
			} while (toupper(c) != toupper(sc));
		} while (strncasecmp(sp, find, findlen) );
		sp--;
	}
	return sp;
}

/* Locate first occurence of character in buffer. Doesn't use NUL,
 * instead it uses the supplied buffer length `len'. */
inline char *
strnchr(s, c, len)
	char *s;
	int c, len;
{
	char *p = s;

	for (;;) {
		if (p - s > len)
			break;
		if (*p == c)
			return p;
		p++;
	}
	return NULL;
}

/* Checks if string `s' of length `len' contains character `c' */
int
strncon(s, len, c)
	char *s;
	int len, c;
{
	int i;

	for (i = 0; i < len; i++) {
		if (*s++ == c)
			return 1;
	}
	return 0;
}

int
snstrcpy(src, len, dst)
	char *src, **dst;
	int len;
{

	*dst = malloc(len + 1);
	if (!*dst)
		return 0;

	strncpy(*dst, src, len);
	(*dst)[len] = 0;
	return 1;
}
