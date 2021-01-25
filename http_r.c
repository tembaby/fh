/* $Id: http_r.c,v 1.16 2003/07/26 21:07:41 te Exp $ */

/*
 * Copyright (c) 2002, 2003 Tamer Embaby <tsemba@menanet.net>
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

#if !defined (_WINDOWS)
# if defined (__OpenBSD__)
#  include <db.h>
# endif
#
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>		/* gethostbyname(), inet_*() */
# include <unistd.h>
# include <sys/queue.h>
#endif /* !_WINDOWS */

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

/* Analyze/Retrieve HTTP Response header/Message body */
static int 	http_get_hdr_entity(struct http_request *,char **);
static int 	hdr_http_response(char *,char **,short *);
static int 	hdr_vrfy_contnt_type(const char *,struct thr_context *);
static int 	hdr_contnt_length(const char *,struct thr_context *);
static int 	hdr_vrfy_contnt_enc(const char *,struct thr_context *);
static time_t 	hdr_expires(const char *,struct thr_context *);
#if 0
static void 	hdr_if_modified_since(time_t,char **);
#endif
static time_t 	hdr_last_modified(const char *,struct thr_context *);
static char 	*hdr_location(const char *,struct thr_context *);
static int 	http_contents(struct thr_context *,int,int,int *);

/* Socket ops */
static int	http_fetch_file(struct thr_context *,int);

int		comp_htreq(char *,int,int,u_short,char const *,
			char const *,int,int,int,struct proxy *);
extern int	http_contents_chunked(struct thr_context *,int);

static time_t	http_time_t(char *);
extern char	*skiplws(char *);

struct http_request *
newhttpreq()
{
	struct http_request *http;

	if ((http = malloc(sizeof(struct http_request))) == NULL) {
		fprintf(stderr, "newhttpreq: out of dynamic memory\n");
		exit(EXIT_FAILURE);
	}
	memset(http, 0, sizeof(struct http_request));
	return (http);
}

/*
 * Http routines.
 */

#define HDR_DEFAULT_LENGTH		512
/*
 * Fetches an HTTP header product-token. Returns:
 * . HDR_EMPTY if empty header (indicates end of header portion)
 * . HDR_OK when success
 * . HDR_ERR on error
 * . HDR_CONCLOS if connection closed (server closed connection)
 * . HDR_MEM on memory leak
 */
static int
http_get_hdr_entity(http, hdr)
	struct	http_request *http;
	char	**hdr;
{
	char c;
	int res, size, curr, cr;

	curr = cr = 0;
	size = HDR_DEFAULT_LENGTH;
	*hdr = malloc(HDR_DEFAULT_LENGTH);
	if (*hdr == NULL) {
		fprintf(stderr, "not enough memory\n");
		return (HDR_MEM);
	}

	for (;;) {
		res = buf_getch(&http->sio, &c, RETR_BUF);
		if (res <= 0)
			return (res == 0 ? HDR_CONCLOS : HDR_ERR);

		if (curr >= size) {
			size <<= 1;
			*hdr = realloc(*hdr, size);
			if (*hdr == NULL) {
				fprintf(stderr, "not enough memory\n");
				return (HDR_MEM);
			}
		}

		(*hdr)[curr] = c;

		if (c == '\r')
			cr = 1;

		if (c == '\n') {
			if (curr == 0 || curr == 1) /* Empty header */
				return (HDR_EMPTY);
			res = buf_getch(&http->sio, &c, PEEK_BUF);
			if (res <= 0)
				return (res == 0 ? HDR_CONCLOS : HDR_ERR);
			if (c == ' ' || c == '\t')
				continue;
			(*hdr)[curr] = '\0';
			if (cr)
				(*hdr)[curr - 1] = '\0';
			break;
		}
		curr++;
	}
	return (HDR_OK);
}

/*
 * Parses HTTP response Status line, returns -1 if HTTP ver is not supported,
 * 0 if line is malformed, status code and the arg `phrase' points to the
 * reason phrase.
 */
static int
hdr_http_response(hdr, phrase, htver)
	char	*hdr, **phrase;
	short	*htver;
{
	char *s;
	int mjr, mnr, status;

	if ((s = hdr) == NULL)
		return (0);

	if (strncmp(hdr, "HTTP/", 5) != 0)
		return (0);

	s += 5, mjr = mnr = 0;
	while (isdigit(*s))
		mjr = 10 * mjr + (*s++ - '0');

	s++;
	while (isdigit(*s))
		mnr = 10 * mnr + (*s++ - '0');

	/*
	 * Accept HTTP version 1.0 or 1.1
	 * XXX: Please change this to more flixable way (argument).
	 */
	if (mjr != 1 && (mnr != 0 || mnr != 1) )
		return (-1);
	*htver = MK_VER(mjr, mnr);

	s++, status = 0;
	while(isdigit(*s))
		status = 10 * status + (*s++ - '0');

	*phrase = ++s;
	return (status);
}

/*
 * Acceptable `Content-Types'. Return 0 if not Content-Type header, -1
 * if content-type is not supported, & 1 if Ok
 *
 * XXX review supported types.
 */
static const char *content_types[] = {
	"text/html",
	"text/plain"
};

static int
hdr_vrfy_contnt_type(hdr, hctx)
	const	char *hdr;
	struct	thr_context *hctx;
{
	char *s;
	static const int len = 12 + 1;
	int i;

	if ((s = (char *)hdr) == NULL)
		return (0);

	if (strncasecmp(hdr, HDR_CONTENT_TYPE":", len) != 0)
		return (0);

	s += len;		/* + skip the column */
	s = skiplws(s);

	for (i = 0; i < sizeof(content_types)/sizeof(content_types[0]); i++) {
		if (strncmp(s, content_types[i], strlen(content_types[i])) == 0)
			return (1);
	}
	return (0);
}

/* Get `Content-Length' value if any */
static int
hdr_contnt_length(hdr, hctx)
	const	char *hdr;
	struct	thr_context *hctx;
{
	int n = 0;
	static const int len = 14 + 1;
	char *s;

	if ((s = (char *)hdr) == NULL)
		return (-1);

	if (strncasecmp(hdr, HDR_CONTENT_LENGTH":", len) != 0)
		return (-1);

	s += len;		/* + skip the column */
	s = skiplws(s);
	while (isdigit(*s))
		n = 10 * n + (*s++ - '0');
	/*
	 * -1 designate peer didn't send Content-Length header.
	 */
	hctx->tc_httpr->hdrs.content_length = (n >= 0 ? n : -1);
	return (hctx->tc_httpr->hdrs.content_length);
}

/* Ignore documents with content-encoding */
static int
hdr_vrfy_contnt_enc(hdr, hctx)
	const	char *hdr;
	struct	thr_context *hctx;
{
	char *s;
	static const int len = 16 + 1;

	if ((s = (char *)hdr) == NULL)
		return (0);

	if (strncasecmp(hdr, HDR_CONTENT_ENCODING":", len) == 0)
		return (1);
	return (0);
}

/* Check `Expires:'. Retruns time_t represinting the time string */
static time_t
hdr_expires(hdr, hctx)
	const	char *hdr;
	struct	thr_context *hctx;
{
	char *s;
	static const int len = 7 + 1;

	if ((s = (char *)hdr) == NULL)
		return (-1);

	if (strncasecmp(hdr, HDR_EXPIRES":", len) != 0)
		return (-1);

	s += len;
	s = skiplws(s);
	hctx->tc_httpr->hdrs.expires= http_time_t(s);
	return (hctx->tc_httpr->hdrs.expires);
}

#if 0
/* Compose If-Modified-Since header field */
static void
hdr_if_modified_since(since, buf)
	time_t since;
	char **buf;
{
	static char m[43];

	memset(m, 0, 43);
	strcpy(m, HDR_IF_MODIFIED": ");
	strcat(m, ctime(&since) );         /* ctime() return \n with time str */
	m[43] = '\0';
	*buf = m;
	return;
}
#endif

/* Check `Last-Modified:'. return time_t value if any */
static time_t
hdr_last_modified(hdr, hctx)
	const	char *hdr;
	struct	thr_context *hctx;
{
	char *s;
	static const int len = 13 + 1;

	if ((s = (char *)hdr) == NULL)
		return ((time_t)-1);

	if (strncasecmp(s, HDR_LAST_MODIFIED":", len) != 0)
		return ((time_t)-1);

	s += len;
	s = skiplws(s);
	hctx->tc_httpr->hdrs.last_modified = http_time_t(s);
	return (hctx->tc_httpr->hdrs.content_length);
}

/* Check `Location:'. return pointer to start of URL */
static char *
hdr_location(hdr, hctx)
	const	char *hdr;
	struct	thr_context *hctx;
{
	char *s;
	static const int len = 8 + 1;

	if ((s = (char *)hdr) == NULL)
		return (NULL);

	if (strncasecmp(s, HDR_LOCATION":", len) != 0)
		return (NULL);

	s += len;
	s = skiplws(s);
	hctx->tc_httpr->hdrs.location = strdup(s);
	return (hctx->tc_httpr->hdrs.location);
}

/*
 * See if content is chunk encoded.
 */
static int
hdr_xfer_encode(hdr, hctx)
	char	*hdr;
	struct	thr_context *hctx;
{
	int chunk = 0;
	static int len = 17 + 1;
	char *s;

	if (hdr == NULL)
		return (0);

	if (strncasecmp(hdr, HDR_TRANSFER_ENCODING":", len) != 0)
		return (-1);

	s = hdr + len;
	s = skiplws(s);
#define CHAR_IS(s,r)	(((s) != 0) && ((s) == (r)))
	if (CHAR_IS(s[0], 'c') && CHAR_IS(s[1], 'h') &&
	    CHAR_IS(s[2], 'u') && CHAR_IS(s[3], 'n') &&
	    CHAR_IS(s[4], 'k') && CHAR_IS(s[5], 'e') &&
	    CHAR_IS(s[6], 'd') )
		chunk = 1;

	s += 7;
	s = skiplws(s);
	if (*s == 0 && chunk != 0) {
		hctx->tc_httpr->hdrs.xfer_encoding = XFER_CHUNKED;
		return (0);
	}
	return (-1);

}

/*
 * Retrieve document body. Called after reception of CRLF CRLF (empty header).
 * Compares number of bytes received with the one expected as in
 * `Content-Length' field, filling the pointer `diff' with the difference
 * between expected length and the actual bytes received.
 * fd is the file we are write the http document to.
 *
 * Download rate is only printed for single download connection, since
 * multiple download connection download-rate will be printed in the
 * status-thread.
 */
static int
http_contents(hctx, fd, expected_length, diff)
	struct	thr_context *hctx;
	int	fd;
       	int	expected_length;
       	int	*diff;		/* diff between expected and fetched size */
{
	int rbytes, wbytes, doc_len = 0;
	int sockfd;
	int fmeth;		/* fetch method */
	char buf[BUFFER_SIZE];
	struct http_request *http;
	struct sio_buf *sio;
#if defined (DOWNLOAD_RATE)
	u_long r_time;
	int r_bdiff;
	char r_buf[BUFSIZ];
	char r_perc[BUFSIZ];
#endif

	fmeth = hctx->tc_fmeth;
	http = hctx->tc_httpr;
	sio = &http->sio;
	sockfd = sio->sio_sk;

	/* 
	 * Activate output buffer caching 
	 */
	brealize(fd);

	/*
	 * Write any data that was received by previous sock_recv()s
	 * while negotiating HTTP headers.
	 */
	doc_len = buf_flush(sio, fd);
	if (doc_len < 0) {
		hctx->tc_retstat = HTERR_LOCIO;
		return (-1);
	}
	
	for (;;) {
#if defined (DOWNLOAD_RATE)
		if (fmeth == FETCH_SINGLE && conn_speed != 0) {
			r_time = timems();
			r_bdiff = doc_len;
		}
#endif

		/*
		 * Clear the timeout flag before every recv() on socket.
		 * This flag is set internally by the sock_*() routines.
		 */
		sio->sio_timo = 0;

		rbytes = sock_recv(sio, buf, BUFFER_SIZE);
		
		/*
		 * Make sure the connection is closed not timed out.
		 */
		if (rbytes == 0 && sio->sio_timo == 0)
			break;
		if (rbytes < 0) {
			fprintf(stderr, "http_contents: ERROR: net I/O error "
			    "%s\n", strerror(errno) );
			hctx->tc_retstat = HTERR_RECVIO;
			return (-1);
		}

		/* 
		 * Count the number of bytes recieved so far.
		 */
		doc_len += rbytes;
		hctx->tc_cursiz = doc_len;

#if defined (DOWNLOAD_RATE)
		if (fmeth == FETCH_SINGLE && conn_speed != 0) {
			r_time = timems() - r_time;
			r_bdiff = doc_len - r_bdiff;
			if (sio->sio_timo != 0)
				strcpy(r_buf, "-- stalled --");
			else
				(void)retr_rate(r_bdiff, r_time, r_buf, 
				    sizeof(r_buf) );
			printf("%s(%s) %s (%s)                           \r", 
			    sio->sio_secured != 0 ? "+" : "",
			    http->hr_url->path ? http->hr_url->path : "ROOT", 
			    r_buf, 
			    retr_percentage(doc_len, expected_length,
			    r_perc, sizeof(r_perc)));
			fflush(stdout);
		}
#endif

		/*
		 * Continue downloading if we were timed out.
		 */
		if (sio->sio_timo != 0)
			continue;

		wbytes = bwrite(fd, buf, rbytes);
		if (wbytes != rbytes) {
			hctx->tc_retstat = HTERR_LOCIO;
			return (-1);
		}
	}

#if defined (DOWNLOAD_RATE)
	if (fmeth == FETCH_SINGLE && conn_speed != 0) {
		printf("\n");
	}
#endif
	
	if (expected_length != -1)
		*diff = doc_len - expected_length;
#ifdef DEBUG
	if (debug != 0) {
		fprintf(stderr, "doc_body%d: received %d bytes of document"
		    " data\n", hctx->tc_seq, doc_len);
	}
#endif
	return (doc_len);
}

/* This actually doesn't belong here */
int
resolv(host, in)
	const	char *host;
	struct	in_addr *in;
{
	struct hostent *h;

#if defined (UNIX)
	if (inet_aton(host, in) == 0) {
#elif defined (_WINDOWS)
	if ((in->s_addr = inet_addr(host)) == INADDR_NONE) {
#endif
		if ((h = gethostbyname(host)) == NULL)
			return (-1);
		memcpy(&in->s_addr, h->h_addr, sizeof(in->s_addr));
	}
	return (0);
}

/*
 * Deals with single HTTP request represented by thr_conext
 * structure.
 * 
 * TODO: Error codes:
 * Interrupted, 
 * receiving IO during communication, 
 * error in connect, 
 * local IO,
 * HTTP header error, 
 * received entity != size advertized
 */
int
http_get_r(hctx)
	struct thr_context *hctx;
{
	int	sock, ofd;
	struct	http_request *hr;

	ofd = hctx->tc_ofd;
	hr = hctx->tc_httpr;

	if ((sock = sock_connect(&hr->sio, hr->hr_url, hr->hr_proxy)) < 0) {
		hctx->tc_retstat = HTERR_CONNECT;
		return (-1);
	}

	if (http_fetch_file(hctx, ofd) < 0) {
		sock_close(&hr->sio);
		return (-1);
	}

	/* clean up */
	(void)sock_close(&hr->sio);
	return (0);
}

/*
 * Converts date format as in RFC1123, RFC 850, & asctime() to time_t
 * Borrowed from wget-1.4.5
 *
 * XXX: This function is not reentrant.
 */
static time_t
http_time_t(s)
	char	*s;
{
#if !defined (_WINDOWS)
	struct tm t;

	/* RFC1123: E.g., Sun, 06 Nov 1994 08:49:37 GMT. */
	if (strptime(s, "%a, %d %b %Y %T", &t))
		return mktime(&t);
	/* RFC850: E.g., Sunday, 06-Nov-94 08:49:37 GMT. */
	if (strptime(s, "%a, %d-%b-%y %T", &t))
		return mktime(&t);
	/* asctime: E.g., Sun Nov 6 08:49:37 1994. */
	if (strptime(s, "%a %b %d %T %Y", &t))
		return mktime(&t);
#endif
	return ((time_t)-1);
}

/*
 * Compose HTTP request.
 */
int
comp_htreq(buf, buflen, method, htvern, requri, host, prepend_slash,
		off_start, off_end, proxy)
	char	*buf;
       	int	buflen;
	int	method;		/* HTTP method; GET, POST, etc. */
	u_short	htvern;		/* HTTP version used */
	char	const *requri;	/* Request URI */
	char	const *host;
	int	prepend_slash;
	struct	proxy *proxy;
{
	int totlen;
	char *meth, *htver;
	char *authp;
	char range[BUFSIZ];
	char auth[BUFSIZ];
	/* XXX */
	extern int base64_encode(char *,int,char **);

	/* XXX Next check is somehow ugly. */
	if (off_start == 0 && off_end == 0)
		range[0] = 0;
	else {
		snprintf(range, sizeof(range), "Range: bytes=%d-%d\r\n",
		    off_start, off_end);
	}

	/* See if we need to authenticate ourself to the proxy server */
	if (proxy != NULL) {
		switch (proxy->p_authtype) {
		case P_AUTH_BASIC:
			snprintf(auth, sizeof(auth), "%s:%s", proxy->p_usr,
			    proxy->p_passwd);
			base64_encode(auth, strlen(auth), &authp);
			snprintf(auth, sizeof(auth), 
			    "Proxy-Authorization: %s\r\n", authp);
			free(authp);
			break;
		default:
			auth[0] = 0;
		}
	} else
		auth[0] = 0;

	switch (method) {
		case HTTP_GET:
			meth = "GET";
			break;
		case HTTP_HEAD:
			meth = "HEAD";
			break;
		default:
			fprintf(stderr,
			    "comp_htreq: Unknown HTTP method (%d)\n", method);
			return (-1);
	}

	if (HTVER_MJR(htvern) != 1) {
		fprintf(stderr, "comp_htreq: version < 1.0 not supported\n");
		return (-1);
	}

	switch (HTVER_MNR(htvern)) {
		case 0:
			htver = HTTP_1_0;
			break;
		case 1:
			htver = HTTP_1_1;
			break;
		default:
			fprintf(stderr,
			    "comp_htreq: version > 1.1 not supported\n");
			return (-1);
	}

	totlen = snprintf(buf, buflen,
			"%s %s%s %s\r\n"
			"User-Agent: " VERSION "\r\n"
			"Host: %s\r\n"
			/*"Accept-Encoding: gzip\r\n"*/
			"Connection: close\r\n"
			"%s"
			"%s"
			"\r\n",
			meth, prepend_slash ? "/" : "",
			requri, htver, host, range, auth);
	buf[totlen] = '\0';
	return (totlen);
}

int
http_get_headers(hctx, reqstat, reqver, matchfn, data)
	struct	thr_context *hctx;
	int	reqstat;		/* Required HTTP response */
	hdr_match_t matchfn;		/* Callback */
	void	*data;			/* Passed to match function */
	short	reqver;			/* HTTP version required */
{
	short htver;
	int rc;
	int http_resp_hdr, status;
	char *hdr, *phrase;
	struct http_request *http;

	http_resp_hdr = 0;
	http = hctx->tc_httpr;

	http->hdrs.xfer_encoding = XFER_PLAIN;

	for (;;) {
		rc = http_get_hdr_entity(http, &hdr);
		if (rc == HDR_MEM || rc == HDR_ERR ||
		    rc == HDR_CONCLOS) {
			fprintf(stderr, "http_hdrs%d: header error: %d\n",
			    hctx->tc_seq, rc);
			return (HDR_ERROR);
		}

		/* Check the status line */
		if (http_resp_hdr == 0) {
			http_resp_hdr++;

			status = hdr_http_response(hdr, &phrase, &htver);
			if (status != reqstat) {
				fprintf(stderr,
				    "http_hdrs%d: ERROR: HTTP/%d, %s."
				    "  Expected %d\n", hctx->tc_seq, status,
				    phrase, reqstat);
				return (HDR_STATUS);
			}

			if (reqver != htver) {
				fprintf(stderr,
				    "http_hdrs%d: VERSION: requested %d.%d"
				    " got %d.%d\n", hctx->tc_seq,
				    HTVER_MJR(reqver), HTVER_MNR(reqver),
				    HTVER_MJR(htver), HTVER_MNR(htver) );
				return (HDR_VER);
			}
		}

		/* The header ends on only CRLF */
		if (rc == HDR_EMPTY) {
			free(hdr);
			return (HDR_OK);
		}

#ifdef DEBUG
		if (debug) {
			fprintf(stderr, "http_hdrs%d: header: %s\n",
			    hctx->tc_seq, hdr);
		}
#endif

		if (matchfn(hdr, data) < 0) {
			fprintf(stderr, "http_hdrs%d: invalid header: %s\n",
			    hctx->tc_seq, hdr);
			return (HDR_REFUSED);
		}

		free(hdr);
	}
	/* NOTREACHED */
	return (HDR_OK);
}

int
hdr_match_fn(hdr, data)
	const	char *hdr;
	void	*data;
{
	struct thr_context *hctx;

	hctx = (struct thr_context *)data;

	/* Content-Length */
	hdr_contnt_length(hdr, hctx);

	/* Last-modified */
	hdr_last_modified(hdr, hctx);

	/* Expires */
	hdr_expires(hdr, hctx);

	/* Location */
	hdr_location(hdr, hctx);

	hdr_xfer_encode(hdr, hctx);

	/* Content-Encoding */
	if (hdr_vrfy_contnt_enc(hdr, hctx) ) {
		fprintf(stderr, "hdr_match_fn%d: XXX Unsupported"
		    " object format (%s)\n", hctx->tc_seq, hdr);
	}

	/* Content-Type */
	if (hdr_vrfy_contnt_type(hdr, hctx) == -1) {
		fprintf(stderr, "hdr_match_fn%d: XXX Unsuported "
		    "Content-Type (%s)", hctx->tc_seq, hdr);
	}
	return (0);
}

/*
 * Do the actual fetching of HTTP file (object).  This function calls
 * either http_contents() or http_contents_chunked() based on transfer
 * encoding type to actually download the object.  This is down after
 * parsing HTTP headers from remote end.
 */
static int
http_fetch_file(hctx, ofd)
	struct	thr_context *hctx;
	int	ofd;
{
	int sockfd;
	int doclen;
	int msg_len, nbytes, diff;
	int err = 0, need_slash = 0, reqstat;
	char *msg;
	struct url_struct *u;
	struct http_request *hr;
	char requri[1024];
	char nmsg[2048];

	hr = hctx->tc_httpr;
	sockfd = hr->sio.sio_sk;
	u = hr->hr_url;

	/* Check to see if we already downloaded everything */
	if (hctx->tc_fmeth == FETCH_MULTIPLE) {
		if (hctx->tc_offset.start ==
		    hctx->tc_offset.end + 1) { /* End is zero based */
#if 0
			printf("%d: Ya sha2i !\n", hctx->tc_seq);
#endif
			return (0);
		}
	}

	if (hr->hr_proxy != NULL) {
		/*
		 * If we have proxy server we pass request URI as the
		 * requested URL itself otherwise we pass the path.
		 */
		strncpy(requri, u->url, sizeof(requri));
		requri[sizeof(requri)  - 1] = '\0';
	} else {
		need_slash = 1;
		if (u->path != NULL) {
			/* 
			 * Request URI: path;params?query 
			 */
			snprintf(requri, sizeof(requri), 
			    "%s%s%s%s%s", u->path, 
			    u->params != NULL ? ";" : "",
			    u->params != NULL ? u->params : "",
			    u->query != NULL ? "?" : "",
			    u->query != NULL ? u->query : "");
		} else
			requri[0] = '\0';
	}

/*
	printf("http_fetch_file%d: starting from off %d to off %d\n",
	    hctx->tc_seq, hctx->tc_offset.start, hctx->tc_offset.end);
*/

	msg_len = comp_htreq(nmsg, sizeof(nmsg), hr->hr_meth,
	    hr->hr_htver, requri, u->host, need_slash, hctx->tc_offset.start,
	    hctx->tc_offset.end, hctx->tc_httpr->hr_proxy);
	msg = nmsg;

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "http_fetch_file%d: HTTP request:\n--BEGIN--\n"
		    "%s\n--END--\n", hctx->tc_seq, msg);
#endif

	/* Now send the HTTP request */
	nbytes = sock_send(&hr->sio, msg, msg_len);
	if (nbytes == -1 || nbytes != msg_len) {
		fprintf(stderr, "http_fetch_file: send: %d: %s\n",
		    errno, strerror(errno) );
		hctx->tc_retstat = HTERR_CONNECT;
		return (-1);
	}

	reqstat = HTTP_OK;
	if (hctx->tc_flags == TC_HTTP_PARTIAL)
		reqstat = HTTP_PARTIALCONTENT;

	err = http_get_headers(hctx, reqstat, hr->hr_htver,
	    hdr_match_fn, hctx);

	if (err != HDR_OK) {
		fprintf(stderr, "http_fetch_file: HTTP error while fetching "
		    "headers (%d)\n", err);
		hctx->tc_retstat = HTERR_HTTP;
		++hctx->tc_vererr;
		return (-1);
	}

	doclen = hr->hdrs.content_length <= 0 ? -1 : hr->hdrs.content_length;

	if (hr->hdrs.xfer_encoding == XFER_CHUNKED)
		nbytes = http_contents_chunked(hctx, ofd);
	else
		nbytes = http_contents(hctx, ofd, doclen, &diff);

	/* I/O Error? */
	if (nbytes < 0) {
		return (-1);
	}

	/* Received length not as Content-Length? */
	if (diff != 0 && hr->hdrs.content_length > 0)  {
		/* Report it */
		fprintf(stderr, "http_fetch_file%d: recieved "
		    "doc size != content-length: received=%d/"
		    "content_length=%d/diff=%d\n", hctx->tc_seq,
		    nbytes, hr->hdrs.content_length, diff);
		/*
		 * This shouldn't really happen unless we're interrupted,
		 * connection dropped, otherwise it's weird.
		 */
		if (hctx->tc_retstat == 0)
			hctx->tc_retstat = HTERR_SIZSHRT;
		return (-1);
	}
	return (0);
}

/*
 * Just uses the HEAD HTTP method to retrieve information about
 * entity.
 */

int
http_get_info(hctx)
	struct	thr_context *hctx;
{
	int msg_len, nbytes;
	int err = 0, need_slash = 0;
	int sockfd;
	char *msg;
	struct url_struct *u;
	struct http_request *http;
	char requri[1024];
	char nmsg[2048];

	http = hctx->tc_httpr;
	u = http->hr_url;

	if (sock_connect(&http->sio, http->hr_url, http->hr_proxy) < 0) {
		hctx->tc_retstat = HTERR_CONNECT;
		return (-1);
	}

	sockfd = http->sio.sio_sk;

	if (http->hr_proxy != NULL) {
		/*
		 * If we have proxy server we pass request URI as the
		 * requested URL itself otherwise we pass the path.
		 */
		strncpy(requri, u->url, sizeof(requri));
		requri[sizeof(requri)  - 1] = '\0';
	} else {
		need_slash = 1;
		if (u->path != NULL) {
			/* 
			 * Request URI: path;params?query 
			 */
			snprintf(requri, sizeof(requri), 
			    "%s%s%s%s%s", u->path, 
			    u->params != NULL ? ";" : "",
			    u->params != NULL ? u->params : "",
			    u->query != NULL ? "?" : "",
			    u->query != NULL ? u->query : "");
		} else
			requri[0] = '\0';
	}

	msg_len = comp_htreq(nmsg, sizeof(nmsg), http->hr_meth,
	    http->hr_htver, requri, u->host, need_slash, 0, 0, http->hr_proxy);
	msg = nmsg;

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "http_info: HTTP request:\n--BEGIN--\n"
		    "%s\n--END--\n", msg);
#endif

	/* Now send the HTTP request */
	nbytes = sock_send(&http->sio, msg, msg_len);
	if (nbytes == -1) {
		fprintf(stderr, "http_info: send: %d: %s\n",
		    errno, strerror(errno) );
		hctx->tc_retstat = HTERR_CONNECT;
		sock_close(&http->sio);
		return (-1);
	}

	err = http_get_headers(hctx, HTTP_OK, MK_VER(1, 1),
	    hdr_match_fn, hctx);
	sock_close(&http->sio);
	return (err);
}
