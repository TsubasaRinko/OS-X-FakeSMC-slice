#
# Until I can remove the dependency on the appkit,
# we'll just keep the generated files in this directory
# and install them directly, rather than generating them again.
#

DIR = util
include ../MakePaths.dir

VPATH = $(OBJROOT):$(SYMROOT)

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
LOCALBIN = $(DSTROOT)/usr/local/bin

OPTIM = -Os -Oz
CFLAGS = $(RC_CFLAGS) $(OPTIM) -Wmost -Werror -g
LDFLAGS =
CFILES = machOconv.c
ALLSRC = $(CFILES) $(MFILES) $(HFILES) $(EXPORT_HFILES)

PROGRAMS = machOconv bdmesg

OUTFILES = $(PROGRAMS)

DIRS_NEEDED = $(OBJROOT) $(SYMROOT) $(LANGDIR)

all embedtheme: $(DIRS_NEEDED) $(PROGRAMS)

machOconv: machOconv.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFINES) -o $(SYMROOT)/$(@F) machOconv.o

bdmesg: bdmesg.o
	$(CC) $(CFLAGS) $(LDFLAGS) -framework IOKit -framework CoreFoundation -mmacosx-version-min=10.5 -o $(SYMROOT)/$(@F) bdmesg.o

include ../MakeInc.dir

#dependencies
-include $(OBJROOT)/Makedep

