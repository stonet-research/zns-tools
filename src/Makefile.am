## Makefile.am

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -O2 -Wall -Wextra -g -Wunused-parameter
sbin_PROGRAMS = zonemap

CFILES = main.c filemap.c

HFILES = filemap.h $(top_srcdir)/include/zmap.h

zonemap_SOURCES = ${CFILES} ${HFILES}
zonemap_LDADD = $(top_srcdir)/lib/libzmap.la
