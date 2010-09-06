# file      : configure.ac
# author    : Boris Kolpackov <boris@codesynthesis.com>
# copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
# license   : GNU GPL v2; see accompanying LICENSE file

AC_PREREQ(2.60)
AC_INIT([odb], [__value__(version)], [odb-users@codesynthesis.com])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([odb/version.hxx])

AM_INIT_AUTOMAKE([-Wall -Werror foreign nostdinc subdir-objects dist-bzip2 dist-zip tar-ustar])

LT_INIT([disable-static])

AC_CANONICAL_HOST

# Check for C++ compiler and use it to compile the tests.
#
AC_PROG_CXX
AC_LANG(C++)

# See if we are building static plugin.
#
static_plugin="$enable_static"

AS_IF([test x$static_plugin = xyes],
  AC_DEFINE([STATIC_PLUGIN], [1], [Building static plugin.]))

# Check for plugin support in GCC unless we are building a static plugin.
#
AS_IF([test x$static_plugin = xno], GCC_PLUGIN)

#
#
AC_ARG_WITH(
  [gxx-name],
  [AC_HELP_STRING([--with-gxx-name=NAME], [g++ binary to embed in the driver])],
  [case $withval in
     no)
       gxx_name=
       ;;
     yes)
       gxx_name=$CXX
       ;;
     *)
       gxx_name="$withval"
       ;;
   esac],
  [gxx_name=$CXX])

AS_IF(
  [test x$gxx_name != x],
  [AC_DEFINE_UNQUOTED([GXX_NAME], ["$gxx_name"], [g++ binary.])])


# Create the libtool executable so that we can use it in further tests.
#
LT_OUTPUT


# Check for libcutl.
#
LIBCUTL([],[AC_MSG_ERROR([libcutl is not found; consider using --with-libcutl=DIR])])


# Check if we should disable rpath.
#
DISABLE_RPATH

# Output.
#
AC_CONFIG_HEADERS([odb/config.h])
AC_CONFIG_FILES([__path__(config_files)])
AC_OUTPUT
