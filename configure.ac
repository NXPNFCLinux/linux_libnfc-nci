#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libnfc_nci_linux], [1.0])
AM_INIT_AUTOMAKE([foreign subdir-objects color-tests])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

# Init Libtool
LT_INIT()
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/halimpl/pn54x/utils/phNxpConfig.cpp])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/time.h unistd.h])
AC_CHECK_HEADERS([fcntl.h sys/ioctl.h termios.h])
AC_CHECK_HEADERS([malloc.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_TYPE_INT16_T
AC_TYPE_INT8_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
_AC_FUNC_MALLOC_IF


AC_CHECK_FUNCS([gettimeofday memmove memset strerror strstr])
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([dlopen])

AC_CONFIG_FILES([Makefile])

AC_ARG_ENABLE([pn7120],
[  --enable-pn7120    set chip version to PN7120],
[case "${enableval}" in
  yes) pn7120=true ;;
  no)  pn7120=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-pn7120]) ;;
esac],[pn7120=false])

AM_CONDITIONAL([CHIP_SEL_NOK], [
COUNT=0
if [ "$pn7120" = "true" ]; then
((COUNT++))
fi
if [test "$COUNT" -gt 1]; then
AC_MSG_ERROR(Can not enable multiple chip type)
fi])

AM_CONDITIONAL([CHIP_SEL_ONE], [if [$pn7120]; then
AC_MSG_NOTICE([Selected chip type is PN7120])
else
AC_MSG_NOTICE([Selected chip type is PN7120])
pn7120=true
fi])

AM_CONDITIONAL([PN7120], [test x$pn7120 = xtrue])

AC_OUTPUT
