/* $Id: ht_chnk.c,v 1.9 2003/07/20 19:04:01 te Exp $ */

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/types.h>

#if defined (_WINDOWS)
# include <bsd_list.h>
#else
# include <unistd.h>
# include <sys/queue.h>
# include <sys/socket.h>
#endif

#include <fh.h>
#include <url.h>
#include <http.h>

#define NO_MEM	(-1)

/*#define HT_CHUNK_DEBUG*/

/*
 * RFC 2068, HTTP/1.1, January 1997, For:
 *
 *  Transfer-Encoding: chunked
 *
 *     Chunked-Body   = *chunk
 *                      "0" CRLF
 *                      footer
 *                      CRLF
 *
 *     chunk          = chunk-size [ chunk-ext ] CRLF
 *                      chunk-data CRLF
 *
 *     hex-no-zero    = <HEX excluding "0">
 *
 *     chunk-size     = hex-no-zero *HEX
 *     chunk-ext      = *( ";" chunk-ext-name [ "=" chunk-ext-value ] )
 *     chunk-ext-name = token
 *     chunk-ext-val  = token | quoted-string
 *     chunk-data     = chunk-size(OCTET)
 *
 *     footer         = *entity-header
 */

int
rchnksiz(buf, len, siz)
	register const char *buf;
	register int len;
	int *siz;
{
	char *p;

	p = (char *)buf;

	if (*p == 0x30)
		return (0);

	while (*p != ';' && *p != '\r' && (p - buf) < len) {
		p++;
	}

	/* Illegal buffer? */
	if (*p != ';' && *p != '\r') {
#if defined (HT_CHUNK_DEBUG)
		fprintf(stderr, "p != ';' && p != '\\r' p=[%c] "
		    "p-buf=%d, len=%d\n", *p, p-buf, len);
#endif
		return (-1);
	}

	*siz = strtol(buf, NULL, 16);
	if (errno == ERANGE) {
		return (-1);
	}

#if defined (HT_CHUNK_DEBUG)
	fprintf(stderr, "rchnksiz: size=%d, nbytes=%d\n", *siz, p - buf);
#endif
	return (p - buf);
}

struct chunk {
	TAILQ_ENTRY(chunk) c_link;
	char	*c_buf;		/* Contents */
	long	c_siz;
	u_char	c_dirty;	/* Is this chunk used? */
	char	*c_ext;		/* Chunk-ext */
	char	*c_pos;		/* Where to append */
	int	c_allocsz;	/* Malloc()d size */
};
TAILQ_HEAD(chunk_t, chunk);

/* Maximum total size of chunks before issuing a flush */
#define SOFT_THRESHOLD_SIZE	(131072)
/* Maximum number of chunks received before forcing a flush */
#define HARD_THRESHOLD_COUNT	(128)
struct chunk_list {
	int	cl_totsiz;	/* So far */
	u_short	cl_nrchnks;	/* # of chunks allocated */
	int	cl_softthrs;	/* Threshold of total size (can be exceeded) */
	u_short	cl_hardthrc;	/* Threshold of chunks count */
	struct	chunk_t cl_chunks;
	struct	chunk *cl_rp;	/* Next pointer to `reuse' chunk */
#define CL_REUSE_BUFFERS	0x0001
#define CL_FLUSH_BUFFERS	0x0002
	u_int	cl_flags;
	char	**cl_footer;
};

/*
 * Reuse the chunk buffers again after flushing them to disk.
 * This is to avoid the overhead of malloc()ating/freeing the
 * chunk buffers again.
 */

#define C_GET(chp,clist) do { \
	if ((clist)->cl_flags & CL_REUSE_BUFFERS) { \
		if ((clist)->cl_rp == NULL) { \
			(clist)->cl_rp = TAILQ_FIRST(&((clist)->cl_chunks)); \
		} else { \
			(chp) = TAILQ_NEXT((clist)->cl_rp, c_link); \
			if ((chp) == NULL) { \
				(clist)->cl_flags |= CL_FLUSH_BUFFERS; \
				(clist)->cl_rp = NULL; \
			} \
		} \
		(chp) = (clist)->cl_rp; \
	} else { \
		(chp) = c_new(); \
	} \
} while (0)

struct chunk_list *
cl_new()
{
	struct chunk_list *clp;

	if ((clp = malloc(sizeof(struct chunk_list) ) ) == NULL) {
		fprintf(stderr, "cl_new: out of dynamic memory\n");
		exit(1);
	}
	memset(clp, 0, sizeof(struct chunk_list) );
	clp->cl_softthrs = SOFT_THRESHOLD_SIZE;
	clp->cl_hardthrc = HARD_THRESHOLD_COUNT;
	TAILQ_INIT(&clp->cl_chunks);
	return (clp);
}

struct chunk *
c_new()
{
	struct chunk *chp;

	if ((chp = malloc(sizeof(struct chunk) ) ) == NULL) {
		fprintf(stderr, "c_new: out of dynamic memory\n");
		return (NULL);
	}
	memset(chp, 0, sizeof(struct chunk) );
	return (chp);
}

int
c_add(chp, buf, siz)
	struct chunk *chp;
	char *buf;
	int siz;
{
	char *rbuf;

	if (chp->c_siz == 0) {
		chp->c_allocsz = siz * 2;
		chp->c_buf = malloc(chp->c_allocsz);
		if (chp->c_buf == NULL) {
			return (NO_MEM);
		}
		chp->c_pos = chp->c_buf;
		memcpy(chp->c_buf, buf, siz);
		chp->c_pos += siz;
		chp->c_siz = siz;
	} else if (siz + chp->c_siz <= chp->c_allocsz) {
		/*
		 * Will fit in chunk buffer without
		 * reallocating more memory.
		 */
		memcpy(chp->c_pos, buf, siz);
		chp->c_pos += siz;
		chp->c_siz += siz;
	} else	{
		/*
		 * Won't fit in existing chunk buffer and we have
		 * to reallocate (expand) memory.
		 */
		chp->c_allocsz += siz * 2;
		if ((rbuf = realloc(chp->c_buf, chp->c_allocsz) ) == NULL) {
			return (NO_MEM);
		}
		chp->c_buf = rbuf;
		chp->c_pos = chp->c_buf + chp->c_siz;
		memcpy(chp->c_pos, buf, siz);
		chp->c_pos += siz;
		chp->c_siz += siz;
	}
	chp->c_dirty++;
	return (chp->c_siz);
}

int
clflush(clp, fd)
	struct chunk_list *clp;
{
	int nerr, totsz;
	register struct chunk *chp;

	totsz = nerr = 0;

	TAILQ_FOREACH(chp, &clp->cl_chunks, c_link) {
		if (chp->c_dirty == 0)
			break;

		if (bwrite(fd, chp->c_buf, chp->c_siz) < 0) {
			nerr = -1;
		}
		totsz += chp->c_siz;

		/* De-initialize chunk buffer */
		chp->c_dirty = 0;
		chp->c_siz = 0;
		chp->c_allocsz = 0;
		if (chp->c_buf)
			free(chp->c_buf);
		chp->c_buf = NULL;
		chp->c_pos = NULL;
	}

	/* Mark chunks list as clean */
	clp->cl_flags &= ~CL_FLUSH_BUFFERS;
	clp->cl_nrchnks = 0;
	clp->cl_rp = NULL;
	return (nerr < 0 ? nerr : totsz);
}

/* Read more data from socket */
#define MORE_DATA(sio,size) do { \
	(sio)->sio_timo = 0; \
	(sio)->sio_left = sock_recv(sio, (sio)->sio_buf, size); \
	(sio)->sio_pos = (sio)->sio_buf; \
} while (0)

/* Insert new chunk in chunks list */
#define INSERT_CHUNK(chp,clist) do { \
	if (TAILQ_FIRST(&((clist)->cl_chunks)) == NULL) \
		TAILQ_INSERT_HEAD(&((clist)->cl_chunks), chp, c_link); \
	else \
		TAILQ_INSERT_TAIL(&((clist)->cl_chunks), chp, c_link); \
	(clist)->cl_nrchnks++; \
} while (0)

#define BUF_ADV(displacement) do { \
	buf += displacement; \
	blen -= displacement; \
} while (0)

int
http_contents_chunked(hctx, ofd)
	int ofd;
	struct thr_context *hctx;
{
	register int blen, ncwrite;
	int chnksz, nerr;
	int nbytes, doclen;
	int fmeth;
	struct chunk_list *clp;
	struct chunk *chp;
	register char *buf;
	struct http_request *http;
	struct sio_buf *sio;
#if defined (DOWNLOAD_RATE)
	u_long r_time;
	int r_bdiff;
	char r_buf[BUFSIZ];
	char r_perc[BUFSIZ];
#endif

	clp = cl_new();
	http = hctx->tc_httpr;
	fmeth = hctx->tc_fmeth;
	sio = &http->sio;
	doclen = 0;
	nerr = 0;

	/* Exhaust SIO buffer */
	buf = sio->sio_pos;
	blen = sio->sio_left;

	/* Activate output buffer cache for this ``fd'' */
	brealize(ofd);

#if defined (HT_CHUNK_DEBUG)
	fprintf(stderr, "chnk: \nStarting HTTP chunked input\n\n");
	fprintf(stderr, "chnk: buf 0x%x, buf_left %d\n", (caddr_t)buf, blen);
#endif

	for (;;) {
		if ((nbytes = rchnksiz(buf, blen, &chnksz) ) < 0) {
			nerr = 1;
			fprintf(stderr, 
			    "chnk: ERROR: illegal chunk size descriptor\n");
			break;
		}

		if (nbytes == 0) {
			BUF_ADV(1);
			/* Last chunk; match CRLF */
			if (*buf == '\r' && *(buf+1) == '\n') {
				BUF_ADV(2);
			} else {
				nerr = 1;
				fprintf(stderr, 
				    "chnk: ERROR: last chunk doesn't end with "
				    "CRLF: 0x%02x 0x%02x\n", *buf, *(buf+1) );
			}

#if defined (HT_CHUNK_DEBUG)
			fprintf(stderr, "chnk: Last chunk (OK). buf_left=%d\n",
			    blen);
#endif

			break;
		}

		/* Skip the size */
		BUF_ADV(nbytes);

		/* If we have chunk-ext; skip it */
		if (*buf == ';') {
			while (*buf != '\r') { /* XXX buffer overrun */
				BUF_ADV(1);
			}
		}

		if (*buf == '\r' && *(buf+1) == '\n') {
			BUF_ADV(2);
		} else {
			nerr = 1;
			fprintf(stderr, "chnk: ERROR: no CRLF after "
			    "<chunk-size><chunk-ext>\n");
			break;
		}

		/* buf pointer to chnk_data, chnksiz == chunk size, buf_len */

		C_GET(chp, clp);

		/* Reused buffers exhausted */
		if (clp->cl_flags & CL_FLUSH_BUFFERS) {
			clflush(clp, ofd);
			C_GET(chp, clp);
			if (clp->cl_flags & CL_FLUSH_BUFFERS) {
				fprintf(stderr, "chnk: ERROR: C_GET required "
				    "flush twice!\n");
				exit(1);
			}
		}

		/* No memory */
		if (chp == NULL) {
			nerr = 1;
			fprintf(stderr,
			    "ERROR: http_content_chunked: out of memory\n");
			break;
		}

#if defined (HT_CHUNK_DEBUG)
		fprintf(stderr, "chnk: chunk_size=%d, buf_size=%d\n",
		    chnksz, blen);
#endif

		while (chnksz != 0) {
#if defined (DOWNLOAD_RATE)
			if (fmeth == FETCH_SINGLE && conn_speed != 0) {
				r_time = timems();
				r_bdiff = doclen;
			}
#endif

			if (blen == 0 && chnksz > 0) {
				MORE_DATA(sio, BUFFER_SIZE);
				buf = sio->sio_buf;
				blen = sio->sio_left;

				if (blen < 0) {
					nerr = 1;
					fprintf(stderr, 
					    "chnk: ERROR: net I/O error: %s\n",
					    strerror(errno));
					break;
				}

				/* Did we finish? */
				if (blen == 0 && sio->sio_timo == 0) {
					break;
				}

#if defined (HT_CHUNK_DEBUG)
				fprintf(stderr, "chnk: <more_data>=%d\n", blen);
#endif
			}

			if (sio->sio_timo == 0) {
				ncwrite = MIN(blen, chnksz);
				c_add(chp, buf, ncwrite); /* XXX */
				chnksz -= ncwrite;
				BUF_ADV(ncwrite);
			}

			/* And update the bytes received so far */
			doclen += ncwrite;
			clp->cl_totsiz = doclen;

#if defined (DOWNLOAD_RATE)
			if (fmeth == FETCH_SINGLE && conn_speed != 0) {
				r_time = timems() - r_time;
				r_bdiff = doclen - r_bdiff;
				if (sio->sio_timo != 0)
					strcpy(r_buf, "-- stalled --");
				else
					(void)retr_rate(r_bdiff, r_time, r_buf, 
					    sizeof(r_buf) );
				printf(
				    "%s[C] (%s) %s (%s)                    \r", 
				    sio->sio_secured != 0 ? "+" : "",
				    http->hr_url->path ? 
				    http->hr_url->path : "ROOT", r_buf,
				    retr_percentage(doclen, -1, r_perc, 
				    sizeof(r_perc)));
				fflush(stdout);
			}
#endif

#if defined (HT_CHUNK_DEBUG)
			fprintf(stderr, "chnk: ncwrite=%d, chunk_tot=%d, "
			    "chunk_left=%d buf_left=%d\n", ncwrite, chp->c_siz,
			    chnksz, blen);
#endif
		}

		if ((clp->cl_flags & CL_REUSE_BUFFERS) == 0)
			INSERT_CHUNK(chp, clp);

		/*
		 * Exceeded max number of chunks or exceeded
		 * max size of total chunks limit.
		 */
		if (clp->cl_nrchnks >= clp->cl_hardthrc ||
		    clp->cl_totsiz > clp->cl_softthrs) {
			clflush(clp, ofd);
			clp->cl_flags |= CL_REUSE_BUFFERS;
		}

		if (blen == 0 || 
		    blen <= 2	/* The 2 bytes required for next CRLF */
		    ) {
			MORE_DATA(sio, BUFFER_SIZE);
			buf = sio->sio_buf;
			blen = sio->sio_left;

			if (blen < 0) {
				nerr = 1;
				fprintf(stderr, 
				    "chnk: ERROR: net I/O error: %s\n",
				    strerror(errno) );
				break;
			}

			if (blen == 0) {
				break;
			}

#if defined (HT_CHUNK_DEBUG)
			fprintf(stderr, "chnk: <more_data>+=%d\n", blen);
#endif
		}

		if (*buf == '\r' && *(buf+1) == '\n') {
			BUF_ADV(2);
		} else {
			fprintf(stderr, "chnk: No CRLF after <chunk>\n");
			nerr = 1;
			break;
		}

		/*
		 * Update the thread's total number
		 * of bytes so far counter
		 */
		hctx->tc_cursiz = doclen;
	}

	/* TODO: read footer here */

#if defined (HT_CHUNK_DEBUG)
	fprintf(stderr, "chnk: <footer>:\n");
	while (blen != 0) {
		fprintf(stderr, "0x%02x ", (u_char)*buf++);
		blen--;
	}
	fprintf(stderr, "\n");

	printf("nerr=%d, nr_chunks=%d\n", nerr, clp->cl_nrchnks);
#endif

#if defined (DOWNLOAD_RATE)
	if (fmeth == FETCH_SINGLE) {
		printf("\n");
	}
#endif

	if (nerr == 0) {
		if (clp->cl_nrchnks != 0) {
			clflush(clp, ofd);
		}
	} else {
		hctx->tc_retstat = HTERR_RECVIO;
		return (-1);
	}

#ifdef DEBUG
	if (debug != 0) {
		fprintf(stderr, "ht_chnk%d: received %d bytes of document"
		    " data\n", hctx->tc_seq, doclen);
	}
#endif
	return (doclen);
}
