## Makefile.am

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -O2 -Wall -Wextra -g -Wunused-parameter
sbin_PROGRAMS = zns.fpbench

zns_fpbench_SOURCES = fpbench.c fpbench.h 
zns_fpbench_LDADD = $(top_srcdir)/lib/libzmap.la $(top_srcdir)/lib/libf2fs.la
