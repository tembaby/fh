#### $Id: unix.prog.c.mk,v 1.10 2002/10/11 21:53:33 te Exp $ ####
# Tamer Embaby <tsemba@menanet.net> 
#
# Generic makefile for C projects.
#

.include <base.mk>

DISTRIB_EXTERNAL_FILES += unix.prog.c.mk

.if defined(VFILE)
all		: $(VFILE) compile
.else
all		: compile
.endif

compile		: sub_dirs_build $(TARGET)

.c.o		: $(HDRDEP)
	$(CC) $(CFL) $<

$(TARGET)	: $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(XLIBS)

.include <common.mk>
