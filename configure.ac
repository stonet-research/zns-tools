AC_PREREQ([2.68])

AC_INIT([zonemap], [0.1], [nicktehrany1@gmail.com])

AC_PREFIX_DEFAULT(/usr)
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AM_SILENT_RULES([yes])

AC_USE_SYSTEM_EXTENSIONS

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_pattern_allow([AM_PROG_AR])
LT_INIT

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for header files.
AC_CHECK_HEADER(linux/blkzoned.h, [],
		[AC_MSG_ERROR([Couldn't find linux/blkzoned.h])])
AC_CHECK_HEADER(linux/fiemap.h, [],
            [AC_MSG_ERROR([Couldn't find linux/fiemap.h])])
AC_CHECK_HEADER(linux/fs.h, [],
		[AC_MSG_ERROR([Couldn't find linux/fs.h])])

AC_CHECK_MEMBER([struct blk_zone.capacity],
		[AC_DEFINE(HAVE_BLK_ZONE_REP_V2, [1],
			   [includes zone capacity])],
		[AC_MSG_ERROR([missing zone capacity. Kernel 5.12+ required])], 
        [[#include <linux/blkzoned.h>]])

AC_CONFIG_FILES([
    Makefile
    man/Makefile
    lib/Makefile
    src/Makefile
])

AC_OUTPUT
