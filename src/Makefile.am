## Makefile.am

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -O2 -Wall -Wextra -g -Wunused-parameter
sbin_PROGRAMS = zns.fiemap zns.segmap

zns_fiemap_SOURCES = fiemap.c fiemap.h
zns_fiemap_LDADD = $(top_srcdir)/lib/libzmap.la

zns_segmap_SOURCES = segmap.c segmap.h
zns_segmap_LDADD = $(top_srcdir)/lib/libzmap.la
