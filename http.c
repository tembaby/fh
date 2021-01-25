/* $Id: http.c,v 1.17 2003/07/20 19:04:01 te Exp $ */

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
 */

#if defined(UNIX)
# include <sys/types.h>
# include <unistd.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#include <fh.h>
#include <url.h>
#include <http.h>

extern int	inittime(struct timeval *);
extern long	usecs();
extern struct	timeval zero;
extern char	*skiplws(char *);

int
getoutputfile(buf, len, requri, host)
	char	*buf;		/* Buffer to write output */
	int	len;		/* Buffer length */
	char	*requri;
	char	*host;
{
	char *s;

	if (strlen(requri) > 0) {
		s = requri + (strlen(requri) - 1);
		do {
			if (*s == '/')
				break;
		} while (s != requri && *(s--));
	} else
		s = "root";
	if (*s == '/')
		s++;
	snprintf(buf, len, "%s-%s", host, s);
	return 0;
}

/* XXX NEW */

/*
 * Create siingle thr_context structure describing the file to be
 * downloaded and the pass it to http_get_r().
 */
int
http_single(http)
	struct http_request *http;
{
	int fd, trying_1_0_ver;
	long usec;
	struct thr_context hsctx;

	inittime(&zero);
	usec = usecs();
	trying_1_0_ver = 0;

	memset(&hsctx, 0, sizeof(struct thr_context) );

	hsctx.tc_seq = 0;
	hsctx.tc_httpr = http;
	hsctx.tc_fmeth = FETCH_SINGLE;

	http->hr_meth = HTTP_GET;
	http->hr_htver = MK_VER(1, 1);

	(void)getoutputfile(hsctx.tc_tfil, sizeof(hsctx.tc_tfil),
	    http->hr_url->path != NULL ? http->hr_url->path : "",
	    http->hr_url->host);

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "http_single: Fetching %s from %s\n",
		    http->hr_url->path ? http->hr_url->path : "ROOT",
		    http->hr_url->host);
#endif

	if ((fd = creat(hsctx.tc_tfil, S_IRUSR | S_IWUSR) ) < 0) {
		fprintf(stderr, "http_single%d: %s: creat:"
		    " %s\n", hsctx.tc_seq, hsctx.tc_tfil, strerror(errno) );
		return (-1);
	}

	hsctx.tc_ofd = fd;
again:
	if (http_get_r(&hsctx) < 0) {
		if (hsctx.tc_retstat == HTERR_HTTP &&
		    hsctx.tc_vererr != 0 &&
		    trying_1_0_ver == 0) {
			++trying_1_0_ver;
			printf("HTTP 1.1 error; trying again "
			    "with version 1.0\n");
			http->hr_htver = MK_VER(1, 0);
			sio_reset(&http->sio);
			goto again;
		}
		herr_done(&hsctx);
		bclose(fd);
		return (-1);
	}

	usec = usecs() - usec;

	if (bench_time != 0 || debug > 0)
		fprintf(stderr, "finished in %lu %s\n",
		    usec < 1000 ? usec/1000 : usec/1000000,
		    usec < 1000 ? "milli-seconds" : "seconds");

	bclose(fd);
	return (0);
}
