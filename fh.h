/* $Id: fh.h,v 1.11 2003/07/26 21:08:10 te Exp $ */

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

#if !defined(_FH_H)
# define _FH_H

#define VERSION			"FH v1.1.2-MT-SSL ("##__DATE__##")"

#if defined (_WINDOWS)
# include <utypes.h>
# include <winsock.h>
# include <windows.h>
# include <io.h>

# if !defined (_POSIX_)
#  define _POSIX_
# endif
# include <limits.h>
# undef _POSIX_

# define bzero(s,l) 		memset(s,0,l)
# define S_IRUSR		_S_IREAD
# define S_IWUSR		_S_IWRITE
# define snprintf		_snprintf
# define inline			__inline
# define strncasecmp(s,d,n) 	strncmp(s,d,n)
# define MIN			min

typedef HANDLE	pthread_t;
typedef char	*caddr_t;

#else /* !_WINDOWS */
# include <netinet/in.h>
# include <sys/types.h>
#endif

#include <url.h>

#define BUFFER_SIZE		(1 << 12)	/* 4K */
#define BUFDIRTY		1
#define BUFFLUSHED		0
#define PEEK_BUF		0x1
#define RETR_BUF		0x0

#if defined (HAVE_SSL)
# define METHOD_SSL_V2		1
# define METHOD_SSL_V3		2
# define METHOD_TLS_V1		3
#endif

/* Socket I/O buffer */
struct sio_buf {
	int	sio_sk;		/* Read socket */
	char	sio_buf[BUFFER_SIZE];
	char	*sio_pos;
	int	sio_left;
	int	sio_timo;	/* Last failure was a timeout condition */
	int	sio_secured;	/* Are we using secure connection? */
#if defined (HAVE_SSL)
	void	*sio_sslctx;	/* SSL context */
	void	*sio_sslconn;	/* SSL connection */
#endif
} sio;

struct proxy {
	char	*p_host;
	struct	in_addr p_addr;
	u_short	p_port;
#define P_AUTH_NONE	0
#define P_AUTH_BASIC	1
	u_char	p_authtype;
	/* Proxy authentication user name and password */
	char	*p_usr;
	char	*p_passwd;
};

extern	int debug;
extern	int bench_time;
extern	int conn_speed;
extern	int multi;
extern	int thrs_num;
extern	int ssl_method;
extern	char *ssl_calist;
extern	int ssl_auth_peer;

/* ocache.c */
int	brealize(int);
int	bwrite(int,const void *,size_t);
int	bclose(int);
int	bflush(int);
int	bsetbufsiz(int,int);

/* rate.c */
char	*retr_rate(long,long,char *,int);
char	*retr_percentage(int,int,char *,int);
u_long	timems(void);

/* siob.c: Buffered socket I/O */
int 	buf_getch(struct sio_buf *,char *,int);
int 	buf_dirty(struct sio_buf *);
int 	buf_flush(struct sio_buf *,int);
void	sio_reset(struct sio_buf *);

/* getline.c */
char	*getline(FILE *);

/* socket.h */
int	sock_startup(int,char *);
int	sock_exit(void);
int	sock_wait(struct sio_buf *,int);
int	sock_connect(struct sio_buf *,struct url_struct *,struct proxy *);
int	sock_recv(struct sio_buf *,void *,size_t);
int	sock_send(struct sio_buf *,void *,size_t);
int	sock_close(struct sio_buf *);

#endif _FH_H /* !_FH_H */
