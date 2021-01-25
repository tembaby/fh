/* $Id: http.h,v 1.16 2003/07/20 19:04:01 te Exp $ */

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

#ifndef _HTTP_H
# define _HTTP_H

/*
 * Source: HTTP 1.0/1.1 RFC1945 / RFC2068.
 * HTTP reply codes.
 */

/* Informational 1XX */
#define HTTP_CONTINUE		100 /* Continue */
#define HTTP_SWITCH		101 /* Switching protocol */

/* Successful. */
#define HTTP_OK			200 /* recv, undrstood, aceptd 		*/
#define HTTP_CREATED		201 /* resource created. Response to POST, not needed */

#define HTTP_ACCEPTED		202 /* accepted				*/
#define HTTP_NONAUTHINFO	203 /* Non-Authoritative informtion	*/
#define HTTP_NOCONTENT		204 /* no content			*/
#define HTTP_PARTIALCONTENT	206 /* Partial content			*/

/* Redirection. */
#define HTTP_MULTI		300 /* multiple choices. not used by 1.0*/
#define HTTP_MOVDPERM		301 /* moved permenantly		*/
#define HTTP_MOVDTEMP		302 /* moved temporarily		*/
#define HTTP_NOTMODIFIED	304 /* not modified. in responce to If-Modified-Since header field	*/
#define HTTP_USEPROXY		305 /* proxy given in `Location' field  */

/* Client errors. */
#define HTTP_EBADREQUEST	400 /* bad request			*/
#define HTTP_EUNAUTHORIZED	401 /* unauthorized			*/
#define HTTP_PAYMENTREQ		402 /* payment required			*/
#define HTTP_EFORBIDDEN		403 /* forbidden			*/
#define HTTP_ENOTFOUND		404 /* not found			*/
#define HTTP_NOTALLOWED		405 /* method not allowed		*/
#define HTTP_NOTACCEPT		406 /* not acceptable			*/
#define HTTP_PROXAUTHREQ	407 /* proxy auth required		*/
#define HTTP_TIMEOUT		408 /* request time out (IMPORTANT)	*/
#define HTTP_GONE		410 /* gone				*/
#define HTTP_LENGTHREQ		411 /* length required			*/
#define HTTP_URITOOLARGE	414 /* request URI too large		*/

/* Server errors. */
#define HTTP_EINTERNALERR	500 /* internal server error		*/
#define HTTP_ENOTIMPLEMENTED	501 /* request not implemented		*/
#define HTTP_EBADGATEWAY	502 /* bad gateway			*/
#define HTTP_EUNAVAILABLE	503 /* unavailable (server loaded)	*/

/* Header fields */
/* ETag: section 3.11-14.20/RFC2068 */
#define HDR_ETAG		"ETag"
/* Content-Type: section (12) */
#define HDR_CONTENT_TYPE	"Content-Type"
/* Content-Length: section */
#define HDR_CONTENT_LENGTH	"Content-Length"
/* Accept-Ranges: {none|bytes} IGN */
#define HDR_ACCEPT_RANGES	"Accept-Renges"
/* Connection: {close|Keep-Alive} IGN */
#define HDR_CONNECTION		"Connection"
/* Content-Encoding: {gzip|compress} IGN doc */
#define HDR_CONTENT_ENCODING	"Content-Encoding"
/* Transfer-Encoding: chunk (HTTP/1.1) */
#define HDR_TRANSFER_ENCODING	"Transfer-Encoding"
/* Expires: section 14.21 */
#define HDR_EXPIRES		"Expires"
/* Host: HTTP/1.1 section 14.23 */
#define HDR_HOST		"Host"
/* If-Modified-Since: section 14.24 */
#define HDR_IF_MODIFIED		"If-Modified-Since"
/* Last-Modified: */
#define HDR_LAST_MODIFIED	"Last-Modified"
/* Location: 3XX responces. section 14.30 */
#define HDR_LOCATION		"Location"
#define USR_AGENT		"User-Agent"

/* HTTP versions */
#define HTTP_1_0		"HTTP/1.0"
#define HTTP_1_1		"HTTP/1.1"
#define MK_VER(j,m)		((short)((j<<8)|m))
#define HTVER_MJR(v)		((v>>8)&0xff)
#define HTVER_MNR(v)		((char)v)

/*
 * Error code returned from
 * 	http_get_hdr_entity()
 * to
 * 	http_get_headers()
 */
enum {
	HDR_OK,
	HDR_ERR,
	HDR_CONCLOS,
	HDR_MEM,
	HDR_EMPTY
};

/*
 * Error code returned from
 * 	http_get_headers()
 * to
 * 	http_fetch_file()
 * 	http_get_info()
 */
#define HDR_ERROR	(-1)
#define HDR_STATUS	(-2)
#define HDR_VER		(-3)
#define HDR_REFUSED	(-4)

/*
 * Error codes used in communication between
 * 	http_fetch_file/http_get_r()
 * 	http_get_info()
 * to their callers.
 *
 * This is returned in thr_context structure.
 *
 * - Non-errornous
 * flush (ocache) buffers
 * write info to resume later (if necessary)
 *
 * - Errornous
 * remove local files created
 * remove local directory created
 *
 * These values are only set by: http_fetch_file()/http_get_info()
 * and are applicable only if the (the above two routines) returns
 * -1 to their callers.
 */
#define HTERR_INTR	1	/* Interrupted */
#define HTERR_RECVIO	2	/* Receiving I/O during communication */
#define HTERR_CONNECT	3	/* Cannot connect/send message to peer */
#define HTERR_LOCIO	4	/* I/O while writing in file */
#define HTERR_HTTP	5	/* HTTP errors, 404 etc. */
#define HTERR_SIZSHRT	6	/* size short for received entity */

/* Maximum number of downloading threads */
#define MAX_GET_THREADS		8

#define CHUNK_PREFIX		"http-"

#include <fh.h>

struct http_request {
	struct url_struct *hr_url;
	struct proxy *hr_proxy;
	char *hr_redirect;		/* XXX OBSOLETE */
	u_short hr_htver;		/* HTTP version */
#define HTTP_GET		1
#define HTTP_HEAD		2
	u_short hr_meth;		/* HTTP method */
	struct sio_buf sio;

	struct hdr_fields {
		long content_length;
		char *content_type;
		char *location;
#define CONTENT_ENC_NONE	0
#define CONTENT_ENC_GZIP	1
#define CONTENT_ENC_COMPRESS	2
		u_char content_encoding;
#define XFER_PLAIN		10
#define XFER_CHUNKED		11
		u_char xfer_encoding;		/* Transfer Encoding */
		time_t last_modified;
		time_t expires;
	} hdrs;
};

/* Constants used to determine the number of downlowding threads */
#define MINIMUM_SIZE		350000
#define HALF_MEGA		524288
#define ONE_MEGA		1048576

#if defined (UNIX)
# include <sys/param.h>
# include <pthread.h>
#endif

struct thr_context {
	int	tc_seq;		/* Sequential number */
	pthread_t tc_id;	/* Thread handle */
	int	tc_totsiz;	/* Total number of bytes to fetch */
	int	tc_cursiz;	/* # of bytes fetched so far */
#define FETCH_SINGLE		1
#define FETCH_MULTIPLE		2
	int	tc_fmeth;	/* Fetch method; single/multiple thread */
	int	tc_retstat;	/* One of HTERR_* code (if unsuccessful) */
	int	tc_ofd;		/* Discriptor to write received data to */
	int	tc_vererr;	/* Error while fetching HTTP 1.1.; try 1.0 */

#define TC_HTTP_PARTIAL		0x0001
#define TC_HTTP_COMPLETE	0x0002
	u_int	tc_flags;

	struct offset {
		int start;
		int end;
	} tc_offset;

	char	tc_tfil[PATH_MAX];	/* Temporary file name for this chunk */
	struct	http_request *tc_httpr;

#define GT_AVAIL	0
#define GT_RUNNING	1
	u_int	tc_stat;
};

struct resume {
	u_int	rs_size;	/* Dowanloaded object total size in bytes */
	int	rs_thrcnt;	/* Number of downloading threads */
	char	*rs_url;
	struct ranges {
		int start;
		int end;
		int cstart;	/* The start offset in continue session */
	} rs_ranges[MAX_GET_THREADS];

	/* These fields will not be saved */
	char	*rs_dir;
};

struct resume_pair {
	struct	resume rp_s;	/* Resume info to be saved */
	struct	resume rp_r;	/* ... read from file */
};
#define RS_SAVE(rp)	(&((rp)->rp_s))
#define RS_READ(rp)	(&((rp)->rp_r))

typedef int (*hdr_match_t)(const char *,void *);

/* Services. */
struct	http_request *newhttpreq(void);
int 	http_get_r(struct thr_context *);
int 	http_get(struct http_request *);
int	http_get_info(struct thr_context *);
int	http_contents_chunked(struct thr_context *,int);
int	http_single(struct http_request *);
void	herr_done(struct thr_context *);
/* resume.c */
int	resume_save(struct resume *);
int	resume_read(struct resume *);
int	resume_consistent(struct resume_pair *);
void	resume_dprint(struct resume_pair *);

#endif /* !_HTTP_H */
