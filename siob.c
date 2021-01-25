/* $Id: siob.c,v 1.3 2003/07/20 19:04:02 te Exp $ */

/*
 * NOTE: Much of the code in buf_*(), hdr_*(), & http_get_hdr_entity() is
 * taken form/similar to wget-1.4.5
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#if defined (UNIX)
# include <unistd.h>
# include <sys/socket.h>
#endif

#include <fh.h>

/*
 * Used to read a character from a socket, it uses internal buffer to speed
 * the operation. Can be used to peek/retrive the buffer.
 */
int
buf_getch(sio, c, how)
	struct sio_buf *sio;
	char *c;
	int how;
{
	int nbytes;

	if (sio->sio_left != 0) {
		if (how == PEEK_BUF)
			*c = *(sio->sio_pos);
		else {	/* RETR_BUF */
			sio->sio_left--;
			*c = *(sio->sio_pos++);
		}
	} else {
		sio->sio_pos = sio->sio_buf;
		nbytes = sock_recv(sio, sio->sio_buf, BUFFER_SIZE);
		if (nbytes <= 0) {
			fprintf(stderr, "recv: errno=%d: %s\n", errno,
			    strerror(errno) );
			return (nbytes);
		}
		if (how == PEEK_BUF) {
			sio->sio_left = nbytes;
			*c = *(sio->sio_pos);
		} else {
			sio->sio_left = nbytes - 1;
			*c = *(sio->sio_pos++);
		}
	}
	return (1);
}

int
buf_dirty(sio)
	struct sio_buf *sio;
{

	return (sio->sio_left > 0);
}

/*
 * Flushes the rest of the socket buffer into a file discriptor.
 */
int
buf_flush(sio, fd)
	int fd;
	struct sio_buf *sio;
{
	int nbytes;

	if (!buf_dirty(sio) )
		return (0);
	nbytes = write(fd, sio->sio_pos, sio->sio_left);
	if (nbytes < 0)
		return (nbytes);	/* Error */
	if (nbytes != sio->sio_left)
		return (-1);
	sio->sio_pos = sio->sio_buf;
	sio->sio_left = 0;
	return (nbytes);
}

/*
 * Reset all fields in sio_buf structure; it's OK to just wipe
 * out everything.
 */
void
sio_reset(sio)
	struct	sio_buf *sio;
{
	
	memset(sio, 0, sizeof(struct sio_buf));
	return;
}
