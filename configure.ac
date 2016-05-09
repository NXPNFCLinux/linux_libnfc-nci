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

AC_ARG_ENABLE([pn7150],
[  --enable-pn7150    set chip version to PN7150],
[case "${enableval}" in
  yes) pn7150=true ;;
  no)  pn7150=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-pn7150]) ;;
esac],[pn7150=false])

AC_ARG_ENABLE([pn7120],
[  --enable-pn7120    set chip version to PN7120],
[case "${enableval}" in
  yes) pn7120=true ;;
  no)  pn7120=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-pn7120]) ;;
esac],[pn7120=false])

AC_ARG_VAR(openssldir, help-string)

AC_ARG_ENABLE([llcp1_3],
[  --enable-llcp1_3    enable LLCP1.3 (secured P2P)],
[case "${enableval}" in
  yes) llcp1_3=true ;;
  no)  llcp1_3=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-llcp1_3]) ;;
esac],[llcp1_3=false])

AM_CONDITIONAL([CHIP_SEL_NOK], [
COUNT=0
if [ "$pn7150" = "true" ]; then
((COUNT++))
fi
if [ "$pn7120" = "true" ]; then
((COUNT++))
fi
if [test "$COUNT" -gt 1]; then
AC_MSG_ERROR(Can not enable multiple chip type)
fi])

AM_CONDITIONAL([LLCP1_3_ENABLED], [if [$llcp1_3]; then
AC_MSG_NOTICE([LLCP1.3 support is enabled (secure P2P)])
if ["${ac_cv_env_openssldir_set}" = "set"]; then
AC_MSG_NOTICE([openssldir path set])
else
AC_MSG_ERROR(path to openssldir not set (required when LLCP1.3 support is enabled))
fi
else
AC_MSG_NOTICE([LLCP1.3 support is disabled (no secure P2P)])
fi])

AM_CONDITIONAL([CHIP_SEL_ONE], [if [$pn7150]; then
AC_MSG_NOTICE([Selected chip type is PN7150])
elif [$pn7120]; then
AC_MSG_NOTICE([Selected chip type is PN7120])
else
AC_MSG_NOTICE([Selected chip type is PN7120])
pn7120=true
fi])

AM_CONDITIONAL([LLCP1_3],  [test x$llcp1_3  = xtrue])

AM_CONDITIONAL([PN7150], [test x$pn7150 = xtrue])

AM_CONDITIONAL([PN7120], [test x$pn7120 = xtrue])

AC_OUTPUT
