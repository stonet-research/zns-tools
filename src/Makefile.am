## Makefile.am

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -O2 -Wall -Wextra -g -Wunused-parameter
sbin_PROGRAMS = zns.fiemap zns.segmap zns.fsinfo zns.fpbench

zns_fiemap_SOURCES = fiemap.c fiemap.h
zns_fiemap_LDADD = $(top_srcdir)/lib/libzns-tools.la $(top_srcdir)/lib/libf2fs.la

zns_segmap_SOURCES = segmap.c segmap.h
zns_segmap_LDADD = $(top_srcdir)/lib/libzns-tools.la $(top_srcdir)/lib/libf2fs.la

zns_fsinfo_SOURCES = fs_info.c fs_info.h
zns_fsinfo_LDADD = $(top_srcdir)/lib/libzns-tools.la $(top_srcdir)/lib/libf2fs.la

zns_fpbench_SOURCES = fpbench.c fpbench.h 
zns_fpbench_LDADD = $(top_srcdir)/lib/libzns-tools.la $(top_srcdir)/lib/libf2fs.la
