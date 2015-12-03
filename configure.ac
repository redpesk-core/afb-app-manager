#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([wgtpkg], [1.0], [wgtpkg@iot.bzh])
AM_INIT_AUTOMAKE
#AM_INIT_AUTOMAKE([-Wall -Wno-pointer-sign])
AC_CONFIG_SRCDIR([wgtpkg-files.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
#AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lcrypto':
#AC_CHECK_LIB([crypto], [d2i_X509])
# FIXME: Replace `main' with a function in `-lxml2':
#AC_CHECK_LIB([xml2], [xmlSaveFormatFileEnc])
# FIXME: Replace `main' with a function in `-lzip':
#AC_CHECK_LIB([zip], [zip_open])

PKG_CHECK_MODULES([ZIP], [libzip >= 0.11])
PKG_CHECK_MODULES([XML2], [libxml-2.0])
PKG_CHECK_MODULES([OPENSSL], [openssl])
PKG_CHECK_MODULES([XMLSEC], [xmlsec1 xmlsec1-openssl])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit memmove mkdir realpath rmdir stpcpy strrchr strtoul])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
