## Makefile.am

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -O2 -Wall -Wextra -g -Wunused-parameter
sbin_PROGRAMS = zonemap

CFILES = control.c \
		  zonemap.c

HFILES = control.h \
		  zonemap.h

zonemap_SOURCES = ${CFILES} ${HFILES}
