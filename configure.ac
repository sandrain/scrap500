dnl
dnl Please refer to COPYING for the license.
dnl

AC_PREREQ([2.69])
AC_INIT([scrap500], [1.0.0], [sandrain@gmail.com])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign tar-pax subdir-objects])
AM_SILENT_RULES([yes])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_LIBTOOL

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_C_INLINE

# Checks for library functions.
AC_CHECK_FUNCS([strdup strrchr strtoul])

AC_CONFIG_FILES([Makefile
                 src/Makefile])

# Checks for libraries.
AC_ARG_WITH(pkgconfigdir,
            [  --with-pkgconfigdir=DIR      pkgconfig file in DIR @<:@LIBDIR/pkgconfig@:>@],
            [pkgconfigdir=$withval],
            [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST(pkgconfigdir)

PKG_CHECK_MODULES([SQLITE3], [sqlite3], ,
                  AC_MSG_ERROR([sqlite3 is required to build hpssix.]))

PKG_CHECK_MODULES([CURL], [libcurl], ,
                  AC_MSG_ERROR([cURL is required to build hpssix.]))

PKG_CHECK_MODULES([LIBXML], [libxml-2.0], ,
                  AC_MSG_ERROR([libxml2 is required to build hpssix.]))

AC_OUTPUT
