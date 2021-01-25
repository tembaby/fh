/* $Id: url.h,v 1.5 2003/07/26 21:08:38 te Exp $ */

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
 *
 * Sun Jan 21 23:04:59 EET 2001
 */

#ifndef _URL_H
# define _URL_H

#include <sys/types.h>

/* Constants defined for uschverfy_t function pointer. */
#define US_SCHEME	1
#define US_USER		2
#define US_PASSWD	3
#define US_HOST		4
#define US_PORT		5
#define US_PATH		6
#define US_PARAMS	7
#define US_QUERY	8
#define US_FRAG		9

/*
 * A callback passed to parse_url(...).
 * parse_url calls this function with every URL component matched to
 * evaluate/validate URL components.
 */
typedef int (*uschverfy_t)(void *,int);

/*
 * URL:
 *   <scheme>:<net_loc>/<path>;<params>?<query>#<fragment>
 * <net_loc>:
 *   //<user>:<password>@<host>:<port>/<url-path>
 */
typedef struct url_struct {
	int proto;
	char *url;
	char *scheme;
	/* <net_loc> */
	char *user;
	char *passwd;
	char *host;
	int port;

	char *path;
	char *params;
	char *query;
	char *frag;

	/* Some informative fields. */
	struct {
		int user 		: 1;
		int passwd 		: 1;
		int params 		: 1;
		int query 		: 1;
		int frag 		: 1;
		int slash_leading_path	: 1;
	} u;
} url_struct;

#define SET_FL(x)	(x = 1)
#define UNSET_FL(x)	(x = 0)
#define IS_SLASH(p)	((p) != 0 && (p) == '/')

/*
 * The supported URL schemes.
 */
#define SCHEME_HTTP	0
#define SCHEME_FTP	1
#define SCHEME_FILE	2
#define SCHEME_TELNET	3
#define SCHEME_HTTPS	4

#define PROTO_HTTP	SCHEME_HTTP
#define PROTO_FTP	SCHEME_FTP
#define PROTO_FILE	SCHEME_FILE
#define PROTO_TELNET	SCHEME_TELNET
#define PROTO_HTTPS	SCHEME_HTTPS

typedef struct scheme_struct {
	int id;
	char *scheme;
	size_t schlen;
	int def_port;
	uschverfy_t schcom;
} scheme_struct;

/* schemes.c */
int sch_http(void *,int);
int sch_ftp(void *,int);
int sch_file(void *,int);
int sch_telnet(void *,int);
int sch_https(void *,int);

/* The errornos port number. */
#define NOPORT		(-1)

/*
 * The extra characters in URL:
 * scheme://user:passwd@host:port/path;params?query#frag
 *       ^^^    ^      ^    ^    ^    ^      ^     ^
 */
#define EXTRA_URL_CHARS	10

/* Allow RFC 1738 semantics. */
#define STRICT_RFC1738

/* Maximum port string length. */
#define MAX_PORT	5

struct url_struct * parse_url(char *);
void ufree(struct url_struct *);

#endif /* _URL_H */
