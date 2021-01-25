/* $Id: url.c,v 1.7 2003/07/26 21:09:01 te Exp $ */

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
 * Sat Dec 16 23:02:32 EET 2000
 */

/*
 * URL parser
 *
 * Various help from wget source code.
 *
 * (C programmers do it between the bits.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#if defined(UNIX)
# include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>		/* malloc, strtol */
#include <limits.h>		/* LONG_* */
#include <sys/types.h>		/* stat */

#include <url.h>
#include <fh.h>

extern int strncon(char *,int,int);

int get_scheme_id(char *,int);
struct url_struct *ualloc(void);
char *encode_string(char *);
char *decode_string(char *);
inline int protoport(int,int);

void ufree(struct url_struct *);
int geturl(char *,char *,char **);
struct url_struct * parse_url(char *);

/*
 * Unsafe characters for URLs as per RFC 1738:
 * Non-graphic chars: 0x80-0xff
 * Unsafe chars: SP < > " # % { } | \ ^ ~ [ ] `
 * Octets represents control chars: 0x00-0x1f, 0x7f
 */
#define UNSAFE_CHARS	" <>\"#%{}|\\^~[]`"
#define NONGRAPHIC_CHAR(c)	((unsigned char)(c) >= 0x80)
#define CTRL_CHAR(c)	((c) <= 0x1f || (c) == 0x7f)
#define CHAR_CLEAN(c)	(!NONGRAPHIC_CHAR(c) && !CTRL_CHAR(c) &&\
		!strchr(UNSAFE_CHARS, c))

/* ASCII to Hexadecimal conversion. */
#define ASCII_TO_HEX(c)		((c >= '0' && c <= '9') ? (c - '0') :	\
		((tolower(c) - 'a') + 10))
#define HEX_TO_ASCII(c)		(c < 10 ? (c + '0') : ((c  - 10) + 'a'))

/* Illegal characters in scheme. */
#define SCHEME_LEGAL_CHAR(x)	(isalnum(x) || (x) != '+' ||\
	       	(x) != '.' || (x) != '-')

static scheme_struct schemes[] = {
	{SCHEME_HTTP,	"http",		4,	80,	sch_http},
	{SCHEME_FTP,	"ftp",		3,	21,	sch_ftp},
	{SCHEME_FILE,	"file",		4,	NOPORT,	sch_file},
	{SCHEME_TELNET,	"telnet",	6,	23,	sch_telnet},
	{SCHEME_HTTPS,	"https",	5,	443,	sch_https},
	{-1,		NULL,		0,	0, 	NULL}
};

/* Try to match the given URL against available Internet schemes.
 * Returnning scheme ID or -1 when failure. */
int
get_scheme_id(url, len)
	char *url;
	int len;
{
	scheme_struct *sch = &schemes[0];

	while (sch->scheme) {
		if (!strncasecmp(sch->scheme, url, len) )
				return sch->id;
		sch++;
	}
	return -1;
}

/* Allocate url_struct. */
struct url_struct *
ualloc()
{
	struct url_struct * url;

	if ((url = malloc(sizeof(struct url_struct) ) ) == NULL)
		return NULL;
	memset(url, 0, sizeof(struct url_struct) );
	url->port = NOPORT;
	return url;
}

void
ufree(url)
	struct url_struct * url;
{

	if (url->url)
		free(url->url);
	if (url->scheme)
		free(url->scheme);
	if (url->passwd)
		free(url->passwd);
	if (url->user)
		free(url->user);
	if (url->host)
		free(url->host);
	if (url->path)
		free(url->path);
	if (url->params)
		free(url->params);
	if (url->query)
		free(url->query);
	if (url->frag)
		free(url->frag);
	free(url);
	return;
}

/*
 * Replace unsafe characters with the corresponding hexadecimal value
 * preceded by the character %, suitable for use in URLs.
 *
 * This returns malloc()'d buffer containing encoded string as mostly
 * the resultant string won't fit in the original buffer. (if we encoded
 * something)
 */
char *
encode_string(s)
	char *s;
{
	int len = strlen(s), totlen = 0;
	int ndirty = 0;
	char * buf, * bp;
	char * p = s;

	if (!s)
		return s;

	/* Count the size of the needed buffer. */
	while (*p) {
		if (!CHAR_CLEAN(((unsigned char) *p)))
			ndirty++;
		p++;
	}

	totlen = len + (ndirty * 2);
	buf = malloc(totlen + 1);
	if (!buf) {
		fprintf(stderr, "memleak: %s %d\n", __FILE__, __LINE__);
		return s;
	}

	p = s;
	bp = buf;

	while (*p) {
		if (!CHAR_CLEAN(*p)) {
			*bp++ = '%';
			*bp++ = HEX_TO_ASCII(((*p) >> 4));
			*bp++ = HEX_TO_ASCII(((*p) & 0x0f));
			p++;
			continue;
		}
		*bp++ = *p++;
	}

	*bp = 0;
	return buf;
}

/* 
 * Replace triplet of the form %XX where XX are hexadecimal numbers with
 * the corresponding US-ASCII character (may be non-printable). 
 */
char *
decode_string(s)
	char *s;
{
	char *p = s;
	char *saved = s;

	if (!s)
		return s;

	while (*p) {
		if (*p == '%' && isxdigit(*(p + 1)) && isxdigit(*(p + 2)) ) {
			*s = (ASCII_TO_HEX(*(p + 1)) << 4) |
				(ASCII_TO_HEX(*(p + 2)));
			p += 3;
			s++;
			continue;
		}
		*s++ = *p++;
	}

	*s = 0;
	return saved;
}

/* XXX: move somewhere else (PROTO) */
inline int
protoport(port, schid)
	int port, schid;
{

	if (port == NOPORT || port == LONG_MIN || port == LONG_MAX)
		if (schid >= 0)
			return schemes[schid].def_port;
	return port;
}

/* Macro for parse_url uschverfy_t function pointer. */
#define ACCEPT(comp, comp_id) do { \
	if (accept) \
		if ((*accept)(comp, comp_id) < 0) \
			return 0; \
} while (0)

/*
 * Parses a URL into it's components
 *
 *   <scheme>://<net_loc>/<path>;<params>?<query>#<fragment>
 *
 * Where <net_loc>:
 *
 *   <user>:<password>@<host>:<port>
 *
 * It's strictly complies with Internet RFC 1738, though some behaviour
 * may ommited by undefining STRICT_RFC1738.
 *
 * This function is lengthy, (approx. 300 lines), It could be rewritten
 * with a lot of helper functions. But it's straight forward, so I
 * may not rewrite it.
 *
 * strcpy() is used only in safe situations like copying into buffer
 * that malloc()'d using the length of source operand of strcpy():
 *         len = strlen(src);
 *         dest = malloc(len + 1);
 *         strcpy(dest, src);
 *
 * Ref.: RFC 1738, RFC 1808.
 *
 * XXX: uschverfy_t parameter is not approperiate here, it will be better
 * to get this function pointer after resolving scheme type (after
 * get_scheme_id().
 */
struct url_struct *
parse_url(urlstrp)
	char *urlstrp;
{
	int uslen;
	int len, ilen;
	int schid;
#if defined (SCHEME_HTTP_HACK)
	int deds = 0;		/* don't exepect double slash */
#endif
	char *usp, *usp_saved;	/* a copy of URL string to work on/free() */
	char *p;
	struct url_struct *url;
	uschverfy_t accept;

	accept = NULL;
	uslen = strlen(urlstrp);

	usp = malloc(uslen + 1);
	if (usp == NULL) {
		fprintf(stderr, "parse_url: not enough memory\n");
		return NULL;
	}

	strcpy(usp, urlstrp);
	usp[uslen] = 0;
	usp_saved = usp;

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "parse_url: URL [%s]\n", usp_saved);
#endif

	if ((url = ualloc() ) == NULL) {
		fprintf(stderr, "parse_url: not enough memory\n");
		free(usp_saved);
		return NULL;
	}

	url->url = strdup(urlstrp);
	if (url->url == NULL) {
		fprintf(stderr, "parse_url: not enough memory\n");
		free(usp_saved);
		ufree(url);
		return NULL;
	}

	/*
	 * 1- Match the fragment.
	 */
	if ((p = strrchr(usp, '#') ) != NULL) {
		/* Cut the fragment off the URL. */
		*p++ = 0;
		if ((len = strlen(p) ) != 0) {
			if ((url->frag = malloc(len + 1) ) == NULL) {
				fprintf(stderr, "parse_url: not enough "
				    "memory\n");
				free(usp_saved);
				ufree(url);
				return NULL;
			}

			strcpy(url->frag, p);
			url->frag[len] = 0;
			SET_FL(url->u.frag);
			/*ACCEPT(url->frag, US_FRAG);*/
		}
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "parse_url: frag (%s)\n", url->frag);
#endif
	}

	/*
	 * 2- The scheme.
	 */
	p = usp;
	if (SCHEME_LEGAL_CHAR(*p) ) {
		while (*p++ != 0) {
			if (*p == ':') {
				len = p - usp;
				if ((url->scheme = malloc(len + 1) ) == NULL) {
					fprintf(stderr, "parse_url: not enough"
					    " memory\n");
					free(usp_saved);
					ufree(url);
					return NULL;
				}

				strncpy(url->scheme, usp, len);
				url->scheme[len] = 0;
				/* Cut off the scheme plus the colon. */
				*(p = usp + len) = 0;
				usp = ++p;
				ACCEPT(url->scheme, US_SCHEME);/* XXX useless */
				break;
			}

			if (!SCHEME_LEGAL_CHAR(*p) ) {
				fprintf(stderr,
					"Illegal charcater in scheme (%c), "
					"Not matching scheme.\n", *p);
				break;
			}
		}
	}

	/* Get the scheme ID if scheme is supported. */
	schid = url->scheme ?
	    get_scheme_id(url->scheme, strlen(url->scheme) ) : -1;

#if defined (SCHEME_HTTP_HACK)
	if (schid == -1) {
		schid = SCHEME_HTTP;
		deds = 1;
	}
#endif

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "parse_url: scheme is [%s], id=%d\n",
		    url->scheme, schid);
#endif

	if (schid != -1)
		accept = schemes[schid].schcom;
	url->proto = schid;

	/*
	 * We delay ACCEPT'ing frag until we know the scheme ID to
	 * properly link with scheme handler.
	 */
	ACCEPT(url->frag, US_FRAG);

	/*
	 * 3- The network location <net_loc>.
	 */
	if ((IS_SLASH(*(usp + 0) ) && IS_SLASH(*(usp + 1) ) )
#if defined (SCHEME_HTTP_HACK)
	    || deds != 0
#endif
			) {
#if defined (SCHEME_HTTP_HACK)
		if (deds == 0)
#endif
			usp += 2; /* Skip the // */

		if ((p = strchr(usp, '/') ) )
			len = p - usp;
		else
			len = strlen(usp);

		/* XXX DEBUG p location */
		if (len == 0) {
			free(usp_saved);
			ufree(url);
			fprintf(stderr, "parse_url: <net_loc> len = 0\n");
			return NULL;
		}

		if (strncon(usp, len, '@') ) {
			/*
			 * Try matching user:passwd from the
			 * user:passwd@host:port, Passwd, or port
			 * could be empty (plus preceding :).
			 * Host could be empty on `file' scheme.
			 */
			p = usp;
			while (*p) {
				if (*p == ':' || *p == '@') {
					/* User. */
					ilen = p - usp;
					if (ilen != 0) {
						if ((url->user =
						    malloc(ilen +  1) ) ==
						    NULL) {
							fprintf(stderr, 
							    "parse_url: not "
							    "enough memory\n");
							free(usp_saved);
							ufree(url);
							return NULL;
						}
						strncpy(url->user, usp, ilen);
						url->user[ilen] = 0;
						SET_FL(url->u.user);
						/* Advance. */
						usp += ilen;
						ACCEPT(url->user, US_USER);
					}
					break;
				}
				p++;
			}
#ifdef STRICT_RFC1738
			/*
			 * The @ sign in <net_loc> without user name is
			 * considered illegal.
			 * Undefine STRICT_RFC1738 to allow syntax like
			 * <ftp://@host> for Anonymous FTP login with
			 * default program passwrod or
			 * <ftp://:pass@host> for Anonymous FTP login
			 * with supplied password.
			 *
			 * XXX: Does the above behaviour make sense?
			 */
			if (url->u.user == 0) {
				free(usp_saved);
				ufree(url);
				fprintf(stderr,"DEBUG: @ with no user!\n");
				return NULL;
			}
#endif

			/* Passwd, which is optional?. */
			if (*usp == ':') {
				if ((p = ++usp) != 0) {
					do {
						if (*p == '@')
							break;
					} while (*(++p) );
				}

				/* Did we find any '@' ? */
				if (*p == '@') {
					if ((ilen = p - usp) != 0) {
						url->passwd = malloc(ilen + 1);
						if (!url->passwd) {
							fprintf(stderr, 
							    "parse_url: not "
							    "enough memory\n");
							free(usp_saved);
							ufree(url);
							return NULL;
						}

						strncpy(url->passwd, usp,
						    ilen);
						url->passwd[ilen] = 0;
						SET_FL(url->u.passwd);
						/* Advance*/
						usp += ilen;
						ACCEPT(url->passwd, US_PASSWD);
					}
#ifdef STRICT_RFC1738
					/* Do we allow <user:@> ? */
					else {
						/* user:@ is illegal. */
						fprintf(stderr, "Illegal syntax"
							" %s:@\n", url->user);
						free(usp_saved);
						ufree(url);
						return NULL;
					}
				}
#endif
			} /* Begins with colon. */

			/*
			 * By now we matched <user> or <user:passwd>
			 * and we have to advance usp since *usp == '@'
			 */
#ifdef DEBUG
			if (debug)
				fprintf(stderr, "parse_url: matched user=%s, "
				    "pass=%s\n", url->user, url->passwd);
#endif
			usp++;
		} /* <net_loc> contains `@'. */

#ifdef DEBUG
		if (debug)
			fprintf(stderr, "parse_url: parsing <host:port> at"
			    " %s\n", usp);
#endif

		/*
		 * Now continue with the host:port whether or not we
		 * matched user:passwd.
		 */
		p = usp;
		/* Host ends with [: / NUL] */
		for (;;) {
			if (*p == ':' || *p == '/' || *p == 0) {
				ilen = p - usp;
				if (ilen != 0) {
					url->host = malloc(ilen + 1);
					if (!url->host) {
						fprintf(stderr, "parse_url: "
						    "not enough memory\n");
						free(usp_saved);
						ufree(url);
						return NULL;
					}
					strncpy(url->host, usp, ilen);
					url->host[ilen] = 0;
					usp += ilen;
					ACCEPT(url->host, US_HOST);
				} else
					break;

				/* Now try the port. Ends with [/ NUL] */
				if (*p == ':' && url->host != NULL) {
					p = ++usp;
					for (;;) {
						if (*p == '/' || !*p) {
							ilen = p - usp;
							break;
						}
						p++;
					}
					url->port = strtol(usp, NULL, 0);
					url->port = protoport(url->port, schid);
					ACCEPT(&url->port, US_PORT);
					usp += ilen;
				}
				break;
			}
			p++;
		} /* Host */
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "parse_url: host (%s), port (%d)\n",
			    url->host, url->port);
#endif
	} /* <net_loc> */

	/*
	 * 4- The query information.
	 */
	if ((p = strchr(usp, '?') ) ) {
		p++;
		len = strlen(p);
		if (len) {
			if ((url->query = malloc(len + 1) ) == NULL) {
				fprintf(stderr, "parse_url: not "
				    "enough memory\n");
				free(usp_saved);
				ufree(url);
				return NULL;
			}
			strcpy(url->query, p);
			url->query[len] = 0;
			SET_FL(url->u.query);
			ACCEPT(url->query, US_QUERY);
		}
		/* And cut off the query information. */
		*(--p) = 0;
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "parse_url: query (%s)\n", url->query);
#endif
	}

	/* 5- The parameters. */
	if ((p = strchr(usp, ';') ) ) {
		p++;
		len = strlen(p);
		if (len) {
			if ((url->params = malloc(len + 1) ) == NULL) {
				fprintf(stderr, "parse_url: not "
				    "enough memory\n");
				free(usp_saved);
				ufree(url);
				return NULL;
			}
			strcpy(url->params, p);
			url->params[len] = 0;
			SET_FL(url->u.params);
			ACCEPT(url->params, US_PARAMS);
		}
		/* Cut off the parameters. */
		*(--p) = 0;
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "parse_url: parameters (%s)\n",
			    url->params);
#endif
	}

	/* 6- The rest is the path. */
	if (*usp == '/') {
		SET_FL(url->u.slash_leading_path);
		usp++;
	}

	len = strlen(usp);
	if (len) {
		if ((url->path = malloc(len + 1) ) == NULL) {
			fprintf(stderr, "parse_url: not enough memory\n");
			free(usp_saved);
			ufree(url);
			return NULL;
		}
		strcpy(url->path, usp);
		url->path[len] = 0;
		ACCEPT(url->path, US_PATH);
	}

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "parse_url: path (%s)\n", url->path);
#endif

	/*
	 * XXX: I decode these values here so they get printed in logs
	 * normally. They should be encoded before recombining the whole
	 * URL again for HTTP (whatever) requests. Plus params, and query
	 * sould be encoded also.
	 */
	url->path = decode_string(url->path);
	url->user = decode_string(url->user); /* XXX */
	url->passwd = decode_string(url->passwd); /* XXX */

	/* Fixing the port if no port number was present in URL string. */
	url->port = protoport(url->port, schid);
#ifdef DEBUG
	if (debug)
		fprintf(stderr, "parse_url: %s fixed port number (%d)\n",
		    url->scheme, url->port);
#endif
	free(usp_saved);
	return url;
}
