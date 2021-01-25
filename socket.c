/* $Id: socket.c,v 1.2 2003/07/20 19:04:02 te Exp $ */ 

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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/param.h>

#if defined (HAVE_SSL)
# include <openssl/ssl.h>
# include <openssl/err.h>

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
# error "Must use OpenSSL 0.9.6 or later"
#endif

int	ssl_startup(int,char *);
void	ssl_exit();
SSL_CTX	*ssl_initialize_ctx(int,char *);
void	ssl_error(char *);
BIO	*ssl_init_error_object(void);
SSL_CTX	*ssl_get_ctx(void);
int	ssl_auth_server(SSL *,char *);

SSL	*ssl_conn_new(SSL_CTX *,int);
void	ssl_conn_free(SSL *);

static	int ssl_initialized;
static	BIO *ssl_err_object;
static	SSL_CTX *ssl_context;

#endif	/* HAVE_SSL */

#include <fh.h>
#include <http.h>
#include <url.h>

#define DEFAULT_TIMEOUT		3
#define WAIT_READ		1
#define WAIT_WRIT		2

extern	int resolv(const char *,struct in_addr *);

int
sock_startup(ssl_meth, ssl_calist)
	int	ssl_meth;
	char	*ssl_calist;
{

#if defined (HAVE_SSL)
	(void)ssl_startup(ssl_meth, ssl_calist);
#endif
	return (0);
}

int
sock_exit()
{

#if defined (HAVE_SSL)
	ssl_exit();
#endif
	return (0);
}

int
sock_wait(sio, wait)
	struct	sio_buf *sio;
	int	wait;
{
	int ret, fd;
	fd_set fdset, *wfds, *rfds;
	struct timeval timo;

	fd = sio->sio_sk;
	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);

	if (wait == WAIT_READ) {
		rfds = &fdset;
		wfds = NULL;
	} else if (wait == WAIT_WRIT) {
		rfds = NULL;
		wfds = &fdset;
	} else {
		return (-1);
	}

	timo.tv_sec = DEFAULT_TIMEOUT;
	timo.tv_usec = 0;

again:
	ret = select(fd + 1, rfds, wfds, NULL, &timo);
	if (ret < 0 && errno == EINTR)
		goto again;
	return (ret);
}

int
sock_connect(sio, u, p)
	struct	sio_buf *sio;
	struct	url_struct *u;		/* URL of remote host */
	struct	proxy *p;		/* Proxy server */
{
	int fd, err;
	struct sockaddr_in dest;

	dest.sin_family = AF_INET;
	dest.sin_port = htons((u_short)u->port);
	bzero(&dest.sin_zero, 8);

	if (p == NULL) {
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "Finding (%s) ...\n", u->host);
#endif
		if (resolv(u->host, &dest.sin_addr) < 0) {
			fprintf(stderr, "sock_connect: resolv: %s: host unknown\n",
			    u->host);
			return (-1);
		}
	} else {
		dest.sin_port = htons((short)p->p_port);
		memcpy(&dest.sin_addr, &p->p_addr, sizeof(struct in_addr));
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "Connecting through HTTP proxy %s port "
			    "%d\n", p->p_host, p->p_port);
#endif
	}

#if defined(_WINDOWS)
	if ((fd = socket(AF_INET, SOCK_STREAM, 0) ) == INVALID_SOCKET) {
#else
	if ((fd = socket(AF_INET, SOCK_STREAM, 0) ) < 0) {
#endif
		fprintf(stderr, "sock_connect: socket: %s\n", strerror(errno));
		return (-1);
	}

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "Connecting to (%s) ...\n", u->host);
#endif

	if ((err = connect(fd, (struct sockaddr *)&dest,
	    sizeof(struct sockaddr))) < 0) {	/* XXX Windows error code */
		fprintf(stderr, "sock_connect: connect: %s\n", strerror(errno));
		sock_close(sio);
		return (-1);
	}
	sio->sio_sk = fd;
#if defined (HAVE_SSL)
	if (u->proto == PROTO_HTTPS) {
		sio->sio_sslctx = ssl_get_ctx();
		if ((sio->sio_sslconn = 
		    ssl_conn_new(sio->sio_sslctx, sio->sio_sk)) == NULL) {
			sock_close(sio);
			return (-1);
		}
		/* Server authentication */
		if (ssl_auth_peer != 0) {
			if (ssl_auth_server(sio->sio_sslconn, u->host) < 0) {
				fprintf(stderr, "sock_connect: server %s "
				    "cannot be authenticated!\n", u->host);
				sock_close(sio);
				return (-1);
			}
		}
		++sio->sio_secured;
	}
#endif
	return (fd);
}

/*
 * The recv() system call may return -1 with errno EINTR which indicates
 * the delivery of a signal *before* any data were available.
 */
int
sock_recv(sio, buf, len)
	struct	sio_buf *sio;
	void	*buf;
	size_t	len;
{
	int ret, fd;

	ret = sock_wait(sio, WAIT_READ);
	if (ret < 0)
		return (ret);
	if (ret == 0) {
		sio->sio_timo = 1;
		return (ret);
	}
	
	fd = sio->sio_sk;
	do {
#if defined (HAVE_SSL)
		if (sio->sio_secured != 0) {
			ret = SSL_read(sio->sio_sslconn, buf, len);
		} else
#endif
			ret = recv(fd, buf, len, 0);
	} while (ret < 0 && errno == EINTR);
	return (ret);
}

/*
 * The send() system call can block if no message space available to
 * queue request.
 */
int
sock_send(sio, buf, len)
	struct	sio_buf *sio;
	void	*buf;
	size_t	len;
{
	int ret, fd;

	fd = sio->sio_sk;
	while (len > 0) {
		ret = sock_wait(sio, WAIT_WRIT);
		if (ret <= 0)		/* XXX connection stale */
			return (ret);
		
		do {
#if defined (HAVE_SSL)
			if (sio->sio_secured != 0) {
				ret = SSL_write(sio->sio_sslconn, buf, len);
			} else
#endif
				ret = send(fd, buf, len, 0);
		} while (ret < 0 && errno == EINTR);
		
		if (ret <= 0)
			return (ret);
		buf += ret;
		len -= ret;
	}
	return (ret);
}

int
sock_close(sio)
	struct	sio_buf *sio;
{
	int ret, fd;

#if defined (HAVE_SSL)
	if (sio->sio_secured != 0) {
		ssl_conn_free(sio->sio_sslconn);
	}
#endif

	fd = sio->sio_sk;
#ifdef _WINDOWS
	closesocket(fd);
#else
	ret = close(fd);
#endif
	return (ret);
}

/*
 * SSL routines.
 */

#if defined (HAVE_SSL)
int
ssl_startup(meth, calist)
	int	meth;
	char	*calist;
{

	if (ssl_initialized == 0) {
		SSL_library_init();
		SSL_load_error_strings();
		(void)ssl_init_error_object();
		ssl_context = ssl_initialize_ctx(meth, calist);
		if (ssl_context == NULL)
			return (-1);
		++ssl_initialized;
#if defined (DEBUG)
		if (debug != 0)
			printf("SSL initialized: SSL v%s (%s)\n", 
			    SHLIB_VERSION_NUMBER, OPENSSL_VERSION_TEXT);
#endif
	}
	return (0);
}

void
ssl_exit()
{

	if (ssl_initialized != 0) {
		SSL_CTX_free(ssl_context);
		--ssl_initialized;
	}
	return;
}

void
ssl_error(msg)
	char	*msg;
{
	unsigned long sslerr;
	char sslerrbuf[BUFSIZ];

	sslerr = ERR_get_error();
	if (sslerr != 0)
		ERR_error_string(sslerr, sslerrbuf);
	else
		strcpy(sslerrbuf, "FAILED!");
	BIO_printf(ssl_err_object, "%s: %s\n", msg, sslerrbuf);
	ERR_print_errors(ssl_err_object);
	return;
}

SSL_CTX *
ssl_get_ctx()
{

	if (ssl_initialized != 0)
		return (ssl_context);
	return (NULL);
}

BIO *
ssl_init_error_object()
{

	if (ssl_err_object == NULL)
		ssl_err_object = BIO_new_fp(stderr, BIO_NOCLOSE);
	return (ssl_err_object);
}

/*
 * We don't support client authentication (yet?).
 */
SSL_CTX	*
ssl_initialize_ctx(meth, calist)
	int	meth;
	char	*calist;	/* Trusted CA list */
{
	SSL_CTX *sslctx;
	SSL_METHOD *sslmeth;
	
	switch (meth) {
	case METHOD_SSL_V2:
	case METHOD_SSL_V3:
		sslmeth = SSLv23_method();
		break;
	case METHOD_TLS_V1:
		sslmeth = TLSv1_method();
		break;
	default:
		fprintf(stderr, "ssl_initialize_ctx: unknown SSL method: %d\n",
		    meth);
		return (NULL);
	}

	if ((sslctx = SSL_CTX_new(sslmeth)) == NULL) {
		ssl_error("SSL_CTX_new");
		return (NULL);
	}

	if (calist != NULL) {
		if (SSL_CTX_load_verify_locations(sslctx, calist, 0) == 0) {
			ssl_error("SSL_CTX_load_verify_locations");
			ssl_exit();
			return (NULL);
		}
	}
	return (sslctx);
}

SSL *
ssl_conn_new(sslctx, sock)
	SSL_CTX	*sslctx;
	int	sock;
{
	SSL *conn;
	BIO *sockbio;

	if ((conn = SSL_new(sslctx)) == NULL) {
		ssl_error("SSL_new");
		return (NULL);
	}
	/* Associate SSL connection to the given socket. */
	if ((sockbio = BIO_new_socket(sock, BIO_NOCLOSE)) == NULL) {
		ssl_error("SSL_new_socket");
		ssl_conn_free(conn);
		return (NULL);
	}
	SSL_set_bio(conn, sockbio, sockbio);
	SSL_set_connect_state(conn);
	SSL_connect(conn);
	if (conn->state != SSL_ST_OK) {
		ssl_error("SSL_connect");
		ssl_conn_free(conn);
		return (NULL);
	}
#if defined (DEBUG)
	if (debug != 0) {
		fprintf(stderr, "SSL connection established.  "
		    "Connection secured.\n");
		fprintf(stderr, "Cipher: %d-bits %s %s\n", 
		    SSL_get_cipher_bits(conn, NULL),
		    SSL_get_cipher_version(conn),
		    SSL_get_cipher_name(conn));
	}
#endif
	return (conn);
}

void	
ssl_conn_free(conn)
	SSL	*conn;
{

	SSL_shutdown(conn);
	SSL_free(conn);
	return;
}

int
ssl_auth_server(conn, server)
	SSL	*conn;
	char	*server;
{
	X509 *peercrt;
	char peer_common_name[MAXHOSTNAMELEN];

	if (SSL_get_verify_result(conn) != X509_V_OK) {
		ssl_error("SSL_get_verify_result");
		return (-1);
	}
	if ((peercrt = SSL_get_peer_certificate(conn)) == NULL) {
		ssl_error("SSL_get_peer_certificate");
		return (-1);
	}
	
	X509_NAME_get_text_by_NID(X509_get_subject_name(peercrt),
	    NID_commonName, peer_common_name, sizeof(peer_common_name));
	
	if (strcasecmp(peer_common_name, server) != 0) {
		fprintf(stderr, "Peer common name `%s' dosn't match `%s'\n",
		    peer_common_name, server);
		return (-1);
	}
#if defined (DEBUG)
	if (debug != 0)
		fprintf(stderr, "Remote server %s authenticated.\n", server);
#endif
	return (0);
}
#endif
