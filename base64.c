/* $Id: base64.c,v 1.3 2002/04/08 01:56:18 te Exp $ */

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
 * Tamer Embaby <tsemba@menanet.net>, 1-11-2000
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef UNIX
# include <unistd.h>
#endif
#include <sys/stat.h>
#ifdef _WINDOWS
# include <io.h>
#endif

#define ARRAY_LEN(x)            (sizeof(x)/sizeof(x[0]))

#define BASE64_PAD_CHAR         '='

/* Line length for output base64 encoded file */
#define LINELEN                 72

/* Postfix for output base64 encoded file */
#define DEFAULT_ENC_FILE_POSTFIX        ".b64"

/* Postfix for output base64 decoded file */
#define DEFAULT_DEC_FILE_POSTFIX        ".orig"

#define CRLF                    "\r\n"

static int to_stdout = 0;

static char encode_table[64] = {
        /*  0 */        'A',
        /*  1 */        'B',
        /*  2 */        'C',
        /*  3 */        'D',
        /*  4 */        'E',
        /*  5 */        'F',
        /*  6 */        'G',
        /*  7 */        'H',
        /*  8 */        'I',
        /*  9 */        'J',
        /* 10 */        'K',
        /* 11 */        'L',
        /* 12 */        'M',
        /* 13 */        'N',
        /* 14 */        'O',
        /* 15 */        'P',
        /* 16 */        'Q',
        /* 17 */        'R',
        /* 18 */        'S',
        /* 19 */        'T',
        /* 20 */        'U',
        /* 21 */        'V',
        /* 22 */        'W',
        /* 23 */        'X',
        /* 24 */        'Y',
        /* 25 */        'Z',
        /* 26 */        'a',
        /* 27 */        'b',
        /* 28 */        'c',
        /* 29 */        'd',
        /* 30 */        'e',
        /* 31 */        'f',
        /* 32 */        'g',
        /* 33 */        'h',
        /* 34 */        'i',
        /* 35 */        'j',
        /* 36 */        'k',
        /* 37 */        'l',
        /* 38 */        'm',
        /* 39 */        'n',
        /* 40 */        'o',
        /* 41 */        'p',
        /* 42 */        'q',
        /* 43 */        'r',
        /* 44 */        's',
        /* 45 */        't',
        /* 46 */        'u',
        /* 47 */        'v',
        /* 48 */        'w',
        /* 49 */        'x',
        /* 50 */        'y',
        /* 51 */        'z',
        /* 52 */        '0',
        /* 53 */        '1',
        /* 54 */        '2',
        /* 55 */        '3',
        /* 56 */        '4',
        /* 57 */        '5',
        /* 58 */        '6',
        /* 59 */        '7',
        /* 60 */        '8',
        /* 61 */        '9',
        /* 62 */        '+',
        /* 63 */        '/'
};

static int decode_table[256];

int base64_encode(char *,int,char **);
int base64_decode(char *,int,char **);
int base64_encode_file(char *);
int base64_decode_file(char *);
void init_base64_conv_tables(void);
static int __read(int,char *,int);

void
init_base64_conv_tables()
{
        int i;

        for (i = 0; i < 255; i++)
                decode_table[i] = 0x80;

        for (i = 'A'; i <= 'Z'; i++)
                decode_table[i] = 0 + (i - 'A');

        for (i = 'a'; i <= 'z'; i++)
                decode_table[i] = 26 + (i - 'a');

        for (i = '0'; i <= '9'; i++)
                decode_table[i] = 52 + (i - '0');

        decode_table['+'] = 62;
        decode_table['/'] = 63;
        decode_table['='] = 0;
        return;
}

/*
 * So, M$-Windowz handles `read' differetly! you request read() 64 bytes
 * Windows *always* return less than 64 (randomly).
 */
static int
__read(int fd, char *buffer, int count)
{
        int nread;
        int i;
        char ch;

        nread = read(fd, buffer, count);
        if (nread <= 0)
                return nread;

        if (nread < count) {
                while (nread < count) {
                        i = read(fd, &ch, 1);
                        if (i <= 0)
                                return nread + i;

                        buffer[nread++] = ch;
                }
        }
        return nread;
}

#define GETBYTE(byte)   do {                                    \
                                        byte = *(pos++);        \
                        } while (0)

#define STOBYTE(indx) do { \
                                if (indx == 0x7f) \
                                        (*outbuf)[j++] = BASE64_PAD_CHAR; \
                                else \
                                        (*outbuf)[j++] = encode_table[indx]; \
                   } while (0)

/*
 * Input buffer must be multiple of three or else the buffer will be padded
 * with '='.
 */
int
base64_encode(char *inbuf, int inbuf_len, char **outbuf)
{
        unsigned char buf[3];
        unsigned char u1, u2, u3, u4;
        unsigned char *pos = inbuf;
        int i, j, outbuf_len, padding_two = 0, padding_one = 0;

        outbuf_len = inbuf_len << 1;

        *outbuf = (char *) malloc(outbuf_len);
        if (!*outbuf)
                return -1;

        for (j = 0, i = 0; i < inbuf_len; i += 3) {
                if ((i + 3) >= inbuf_len) {
                        if (i + 2 == inbuf_len)
                                padding_one = 1;
                        else if (i + 1 == inbuf_len)
                                padding_two = padding_one = 1;
                }

                GETBYTE(buf[0]);
                GETBYTE(buf[1]);
                GETBYTE(buf[2]);

                /*
                 * RFC-1521
                 * The encoding process represents 24-bit groups of input
                 * bits as output strings of 4 encoded characters. Proceeding
                 * from left to right, a 24-bit input group is formed by
                 * concatenating 3 8-bit input groups. These 24 bits are then
                 * treated as 4 concatenated 6-bit groups, each of which is
                 * translated into a single digit in the base64 alphabet.
                 * When encoding a bit stream via the base64 encoding, the
                 * bit stream must be presumed to be ordered with the
                 * most-significant-bit first.
                 *
                 * That is, the first bit in the stream will be the
                 * high-order bit in the first byte, and the eighth bit will
                 * be the low-order bit in the first byte, and so on.
                 */
                u1 = buf[0] >> 2;
                u2 = ((buf[0] & 0x3) << 4) | (buf[1] >> 4);
                u3 = ((buf[1] & 0xf) << 2) | ((buf[2] >> 6) & 0x3);
                u4 = buf[2] & 0x3f;

                STOBYTE(u1);
                STOBYTE(u2);
                STOBYTE(padding_two ? 0x7f : u3);
                STOBYTE(padding_one ? 0x7f : u4);
        }
        return j;
}

#define DECBYTE(hold,pos) \
do { \
	hold = decode_table[(int)inbuf[pos]] & 0x3f; \
} while (0)


/*
 * input buffer _must_ be mutiple of four size (YEAH!)
 * doesn't handle \r\n in input buffer
 *
 * 1) Convert the character to it's Base64 decimal value.
 * 2) Convert this decimal value into binary.
 * 3) Squash the 6 bits of each character into one big string of binary digits.
 * 4) Split this string up into groups of 8 bits (starting from right to left).
 * 5) Convert each 8-bit binary value into a decimal number.
 * 6) Convert this decimal value into its US-ASCII equivalent.
 */
int
base64_decode(char *inbuf, int inbuf_len, char **outbuf)
{
        unsigned char u1, u2, u3, u4;
        int i, j;

        *outbuf = (char *) malloc(inbuf_len);
        if (!*outbuf)
                return 0;

        for (j = i = 0; i < inbuf_len; i += 4, j += 3) {
                DECBYTE(u1, i);
                DECBYTE(u2, i + 1);
                DECBYTE(u3, i + 2);
                DECBYTE(u4, i + 3);

                (*outbuf)[j + 0] = (u1 << 2) | (u2 >> 4);
                (*outbuf)[j + 1] = (u2 << 4) | (u3 >> 2);
                (*outbuf)[j + 2] = (u3 << 6) | u4 ;
        }

        if (u4 == 0) {
                j--;
                if (u3 == 0)
                        j--;
        }

        return j;
}

/* What about `-' as stdout */
int
base64_encode_file(char *ifile)
{
        char ofile[256];
        int ifd, ofd/*, i = 0*/;
        struct stat stbuf;
        char *buffer;
        char tmp[64], *obuf;
        int nread, nwritten, nencoded;
        int err = 0;
	int pos, nleft, nline;
        unsigned long fsize;
#ifdef __BENCH
	clock_t jiffies;
#endif

        if (stat(ifile, &stbuf) < 0){
                fprintf(stderr, "base64: couldn't open %s\n", ifile);
                return 0;
        }

        fsize = stbuf.st_size;

        buffer = (char *)malloc(fsize);
        if (!buffer)
                return 0;

        strncpy(ofile, ifile, 256 - (strlen(DEFAULT_ENC_FILE_POSTFIX) + 1) );
        strncat(ofile, DEFAULT_ENC_FILE_POSTFIX,
			strlen(DEFAULT_ENC_FILE_POSTFIX) );
        ofile[256 - 1] = 0;

#ifdef _WINDOWS
        if ((ifd = open(ifile, O_RDONLY | O_BINARY) ) < 0) {
                fprintf(stderr, "base64: couldn't open %s\n", ifile);
                return 0;
        }
#else
        if ((ifd = open(ifile, O_RDONLY) ) < 0) {
                fprintf(stderr, "base64: couldn't open %s\n", ifile);
                return 0;
        }
#endif

        if (!to_stdout && stat(ofile, &stbuf) == 0) {
                fprintf(stderr, "base64: %s exist, replace? (N/y)_ ", ofile);
                fgets(tmp, 64, stdin);

                if (tmp[0] != '\n' && ((tmp[0] == 'y' || tmp[0] == 'Y') &&
				       	tmp[1] == '\n') )
                        ;
                else {
                        err = 1;
                        goto out2;
                }
        }

        if (!to_stdout && /*(ofd = creat(ofile, O_RDWR | O_TRUNC) )*/
                (ofd = open(ofile, O_CREAT | O_TRUNC | O_WRONLY,
			    S_IREAD | S_IWRITE) ) < 0) {
                fprintf(stderr, "base64: couldn't open %s\n", ofile);
                err = 1;
                goto out1;
        }

        if (to_stdout)
                ofd = 1;

        if ((nread = __read(ifd, buffer, fsize) ) < 0) {
                fprintf(stderr, "base64: IO error reading %s\n", ifile);
                err = 1;
                goto out1;
        }

        if ((nencoded = base64_encode(buffer, nread, &obuf) ) < 0) {
                fprintf(stderr, "base64: error encoding %s\n", ifile);
                err = 1;
                goto out1;
        }

#ifdef __BENCH
	jiffies = clock();
#endif
#if 0
        while (i < nencoded) {
                if ((nwritten = write(ofd, obuf + i, 1) ) < 0) {
                        fprintf(stderr, "base64: IO error writing %s\n", ofile);
                        err = 1;
                        goto out1;
                }

                if ((++i % LINELEN) == 0 && write(ofd, CRLF, 2) < 0) {
                        fprintf(stderr, "base64: IO error writing %s\n", ofile);
                        err = 1;
                        goto out1;
                }
        }
#else
	pos = 0;
	nleft = nencoded;
	while (nleft) {
		nleft = nencoded - pos;
		if (!nleft)
			break;
		if (nleft < LINELEN) {
			nline = nleft;
		} else
			nline = LINELEN;
		if ((nwritten = write(ofd, obuf + pos, nline) ) < 0) {
			fprintf(stderr, "base64: IO error writing %s\n", ofile);
			err = 1;
			goto out1;
		}
		write(ofd, CRLF, 2);
		pos += nwritten;
	}
#endif
	write(ofd, CRLF, 2);
#ifdef __BENCH
	jiffies = clock() - jiffies;
	fprintf(stderr, "No. of jiffies = %lu:%3.3f\n", jiffies,
		       jiffies / CLOCKS_PER_SEC);
#endif
        free(obuf);

out1:
        close(ofd);
out2:
        close(ifd);

        if (err)
                return !err;
        return 1;
}

int
base64_decode_file(char *ifile)
{
        char ofile[256];
        int ifd, ofd;
        int err = 0, ndecoded, nread, nwritten, i = 0;
        char *obuf, tmp[64];
        struct stat stbuf;
        char *buffer, ch;
        unsigned long fsize;

        if (stat(ifile, &stbuf) < 0){
                fprintf(stderr, "base64: couldn't open %s\n", ifile);
                return 0;
        }

        fsize = stbuf.st_size;

        buffer = (char *) malloc(fsize);
        if (!buffer)
                return 0;

        strncpy(ofile, ifile, strlen(ifile) - strlen(DEFAULT_ENC_FILE_POSTFIX) );      /*BIGERR*/
        ofile[strlen(ifile) - strlen(DEFAULT_ENC_FILE_POSTFIX)] = 0;
        strncat(ofile, DEFAULT_DEC_FILE_POSTFIX,
			strlen(DEFAULT_DEC_FILE_POSTFIX) );
        ofile[256 - 1] = 0;

        if ((ifd = open(ifile, O_RDONLY) ) < 0) {
                fprintf(stderr, "base64: couldn't open %s\n", ifile);
                return 0;
        }

        if (!to_stdout && stat(ofile, &stbuf) == 0) {
                fprintf(stderr, "base64: %s exist, replace? (N/y)_ ", ofile);
                fgets(tmp, 64, stdin);

                if (tmp[0] != '\n' && ((tmp[0] == 'y' || tmp[0] == 'Y') &&
				       	tmp[1] == '\n') )
                        ;
                else {
                        err = 1;
                        goto out2;
                }
        }

#ifdef _WINDOWS
        if (!to_stdout &&
                (ofd = open(ofile, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY,
                        S_IREAD | S_IWRITE) ) < 0) {
                fprintf(stderr, "base64: couldn't open %s\n", ofile);
                err = 1;
                goto out1;
        }
#else
        if (!to_stdout && /*(ofd = creat(ofile, O_RDWR | O_TRUNC) )*/
                (ofd = open(ofile, O_CREAT | O_TRUNC |
			    O_WRONLY, S_IREAD | S_IWRITE) ) < 0) {
                fprintf(stderr, "base64: couldn't open %s\n", ofile);
                err = 1;
                goto out1;
        }
#endif

        if (to_stdout)
                ofd = 1;

        while ((unsigned long)i < fsize) {
                if ((nread = __read(ifd, &ch, 1) ) < 0) {
                        fprintf(stderr, "base64: IO error reading %s\n", ifile);
                        err = 1;
                        goto out1;
                }

                if (nread == 0)
                        break;

                if (ch == '\r' || ch == '\n')
                        continue;

                buffer[i++] = ch;
        }

        nread = i;

        if ((ndecoded = base64_decode(buffer, nread, &obuf) ) < 0) {
                fprintf(stderr, "base64: error decoding %s\n", ifile);
                err = 1;
                goto out1;
        }

        if ((nwritten = write(ofd, obuf, ndecoded) ) < 0) {
                fprintf(stderr, "base64: IO error writing %s\n", ofile);
                err = 1;
                goto out1;
        }

        free(obuf);

out1:
        close(ofd);
out2:
        close(ifd);

        if (err)
                return !err;
        return 1;
}
