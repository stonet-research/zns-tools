## Makefile.am

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -O2 -Wall -Wextra -g -Wunused-parameter
sbin_PROGRAMS = zns.fibmap

zns_fibmap_SOURCES = fibmap.c fibmap.h
zns_fibmap_LDADD = $(top_srcdir)/lib/libzmap.la
