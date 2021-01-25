/* $Id: ocache.c,v 1.5 2003/07/20 19:04:02 te Exp $ */

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
 * Output cache system.
 * It copies buffer to be written to disk to internal buffers associated
 * with a file discriptor, accepting many `bwrite' calls and buffers requests
 * until a water mark is crossed, then at next chance it writes all buffers
 * associated with file discriptor to disk.
 *
 * A file discriptor used with output cache should be used for writing only.
 *
 * Output cache calls to be used:
 *
 * brealize(int fd);
 *
 * bwrite(int fd, const void *buffer, size_t nbytes);
 *
 * bclose(int fd);
 *
 * bflush(int fd);
 *
 * bsetbufsiz(int fd, int newsize);
 */

#include <stdio.h>
#if defined (UNIX)
# include <unistd.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#if defined (_WINDOWS)
# include <io.h>
# include <utypes.h>
#endif

/*#define OUTPUT_CACHE_BUFFER	0*/

/* Number of open files */
#define NOFILES			40

/* Optimal default cache buffer size */
#define OCBUFSIZDEF		65536

extern int debug;

struct file_cache {
	int	fc_fild;	/* File discriptor */
	int	fc_obufsiz;	/* Cache buffer size */
	int	fc_cbufsiz;	/* Current cache size */
	int	fc_flags;
#define F_USED		0x0001
#define F_CLEAN		0x0002
	u_char	*fc_buffer;
	u_char	*fc_fpos;
};

struct output_cache {
	struct	file_cache oc_files[NOFILES];
	size_t	oc_nbxfer;	/* # of bytes written on all discriptors */
};

static struct output_cache ocache;

int	brealize(int);
int	bwrite(int,const void *,size_t);
int	bclose(int);
int	bflush(int);
int	bsetbufsiz(int,int);

#define BRELE(fp)	memset(fp, 0, sizeof(struct file_cache) )
#define BFREE(fp)	free(fp->fc_buffer)

int
brealize(fd)
	int	fd;
{
	register struct file_cache *fp;

#if defined (OUTPUT_CACHE_BUFFER) && OUTPUT_CACHE_BUFFER == 0
	return (0);
#endif

	if (fd >= NOFILES)
		return (-1);

	fp = &ocache.oc_files[fd];
	if (fp->fc_flags & F_USED) {
		fprintf(stderr, "brealize: %d: requested more than once\n", fd);
		return (-1);
	}

	fp->fc_flags |= F_USED;
	fp->fc_flags |= F_CLEAN;

	fp->fc_fild = fd;
	fp->fc_obufsiz = OCBUFSIZDEF;
	fp->fc_cbufsiz = 0;

	if ((fp->fc_buffer = malloc(fp->fc_obufsiz)) == NULL) {
		fprintf(stderr, "brealize: %d: cannot allocate %d bytes\n",
		    fd, fp->fc_obufsiz);
		BRELE(fp);
		return (-1);
	}

	fp->fc_fpos = fp->fc_buffer;

#ifdef DEBUG
	if (debug > 0)
		printf("brealize: %d on output buffer cache\n", fd);
#endif
	return (0);
}

int
bwrite(fd, buffer, nbytes)
	int	fd;
	const	void *buffer;
	size_t	nbytes;
{
	register struct file_cache *fp;

#if defined (OUTPUT_CACHE_BUFFER) && OUTPUT_CACHE_BUFFER == 0
	return (write(fd, buffer, nbytes));
#endif

	if (fd >= NOFILES)
		return (write(fd, buffer, nbytes));

	fp = &ocache.oc_files[fd];

	if ((fp->fc_flags & F_USED) == 0) {
		if (brealize(fd) < 0)
			return (write(fd, buffer, nbytes));
	}

	if (fp->fc_cbufsiz + (int)nbytes > fp->fc_obufsiz) {
		if (bflush(fd) < 0)
			return (-1);
	}

	memcpy(fp->fc_fpos, buffer, nbytes);
	fp->fc_fpos += nbytes;
	fp->fc_cbufsiz += nbytes;
	fp->fc_flags &= ~F_CLEAN;
	ocache.oc_nbxfer += nbytes;
	return (nbytes);
}

int
bclose(fd)
	int	fd;
{
	register struct file_cache *fp;

#if defined (OUTPUT_CACHE_BUFFER) && OUTPUT_CACHE_BUFFER == 0
	return (close(fd));
#endif

	if (fd >= NOFILES)
		return (close(fd));

	fp = &ocache.oc_files[fd];

	if ((fp->fc_flags & F_USED) == 0)
		return (-1);

	if ((fp->fc_flags & F_CLEAN) == 0)
		bflush(fd);

	close(fp->fc_fild);

	BFREE(fp);
	BRELE(fp);
	return (0);
}

int
bflush(fd)
	int	fd;
{
	register struct file_cache *fp;

	if (fd >= NOFILES)
		return (-1);

	fp = &ocache.oc_files[fd];

	if ((fp->fc_flags & F_USED) == 0)
		return (-1);

	if (fp->fc_flags & F_CLEAN)
		return (0);

	if (write(fp->fc_fild, fp->fc_buffer, fp->fc_cbufsiz) < 0) {
		fprintf(stderr, "bflush: %d: write: %s\n", fp->fc_fild,
		    strerror(errno) );
		return (-1);
	}

	fp->fc_cbufsiz = 0;
	fp->fc_fpos = fp->fc_buffer;
	return (0);
}

int
bsetbufsiz(fd, newsize)
	int	fd, newsize;
{
	register u_char *obuf;
	register struct file_cache *fp;

	if (fd >= NOFILES)
		return (-1);

	fp = &ocache.oc_files[fd];

	if ((fp->fc_flags & F_USED) == 0)
		return (-1);

	if (newsize < fp->fc_obufsiz)
		bflush(fd);

	fp->fc_obufsiz = newsize;
	obuf = realloc(fp->fc_buffer, newsize);
	if (obuf == NULL) {
		fprintf(stderr, "bsetbufsiz: %d: cannot reallocate %d bytes\n",
		    fd, fp->fc_obufsiz);
		return (-1);
	}

	fp->fc_buffer = obuf;
	fp->fc_fpos = fp->fc_buffer + fp->fc_cbufsiz;
	return (0);
}
