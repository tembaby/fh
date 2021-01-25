#### $Id: base.mk,v 1.9 2003/06/05 14:03:55 te Exp $ ####
# Tamer Embaby <tsemba@menanet.net> 
#

# C Compiler. XXX
CC		= cc

# The directory that contains master make file (current directory.)
BASEDIR		= $$HOME/mk

# The C source tree root
C_ROOT		= $$HOME/c

# External files that should be distributed with every release of program,
# this is a list of iles without leading directory names e.g.,
#	somthing.inc mk.exe.inc
# These files must originally reside in $(BASEDIR).
BASE_EXT	= base.mk common.mk
DISTRIB_EXTERNAL_FILES += $(BASE_EXT)

# Debug flag.  To include debugging information or to optimize code.
.if defined(DEBUG)
DEBUG		= -g
.else
DEBUG		= -O3
.endif

# Header file(s) that will make whole project rebuild.
.if !defined(HDRDEP)
HDRDEP		= 
.endif

# Default C compile flags.
CFL		= -Wall $(DEBUG) -c -I.

# Extra compile flag(s) defined by the user.
.if defined(UCFLAGS)
CFL		+= $(UCFLAGS)
.endif

# Default tarball name.
.ifndef (TARBALL)
TARBALL		= $(TARGET).tar.gz
.endif

# Default directory name for uploads.
# Default directory name for directory to archive (tarballs.)
.ifndef (PROGDIR)
PROGDIR		= $(TARGET)
.endif

# Command to import external sources base on two variables:
# EXT_ROOT: root directory to append to each component to be 
#	    imported.
# EXT_FILES: components to be imported.
.ifndef (IMPORT_CMD)
IMPORT_CMD	= /bin/cp
.endif

# Default args to default import command.
.ifndef (IMPORT_ARGS)
IMPORT_ARGS	= -i
.endif

# Program to generate ChangeLog files.
.ifndef (CL_BIN)
CL_BIN		= $$HOME/bin/cvs2cl.pl
.endif

# Its arguments: run cvs2cl.pl --help for explaination.
.ifndef (CL_ARGS)
CL_ARGS		= -r -w -P -U $$HOME/.cvs2cl.users >/dev/null 2>&1
.endif

# Basic falgs for tarball command
TAR_ARGS	= cpzf
