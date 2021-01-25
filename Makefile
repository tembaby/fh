#
# $Id: Makefile,v 1.15 2003/07/20 19:13:54 te Exp $
#

OBJS		= base64.o getopt.o http_r.o main.o misc.o schemes.o url.o \
		  getthrs.o http.o ht_chnk.o ocache.o rate.o siob.o resume.o \
		  getline.o socket.o
TARGET		= fh
DEBUG 		=
UCFLAGS 	= -Wno-format -DUNIX -DDEBUG -DDOWNLOAD_RATE \
		  -DSCHEME_HTTP_HACK -DHAVE_REGEX -DHAVE_SSL
INSTALLDIR	= /home/te/bin
TARBALL		= fh.tar.gz
HDRDEP		= http.h fh.h
XLIBS		= -pthread -lssl -lcrypto

# External files (get via: make import).
EXT_ROOT	= $(C_ROOT)/comm
EXT_DIRS	= win32
EXT_FILES	= getline.c win32/utypes.h win32/bsd_list.h

.include <unix.prog.c.mk>
