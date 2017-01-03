# file      : configure.ac
# copyright : Copyright (c) 2009-2017 Code Synthesis Tools CC
# license   : GNU GPL v3; see accompanying LICENSE file

AC_PREREQ(2.60)
AC_INIT([odb], [__value__(version)], [odb-users@codesynthesis.com])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([odb/version.hxx])

AM_INIT_AUTOMAKE([-Wall -Werror foreign nostdinc subdir-objects dist-bzip2 dist-zip tar-ustar])
m4_equote()[m4_ifdef]m4_dquote()([AM_PROG_AR], [AM_PROG_AR]) # Required by automake 1.12.

LT_INIT([disable-static])

AC_CANONICAL_HOST

# Check for C++ compiler and use it to compile the tests.
#
AC_PROG_CXX
AC_LANG(C++)

# Create the libtool executable so that we can use it in further tests.
#
LT_OUTPUT

# Test for plugin support in GCC.
#
GCC_PLUGIN

# See if we are building static plugin. Static build should only be
# used if you are building a custom version of GCC with the ODB
# plugin linked-in statically. This is primarily useful on Windows
# where GCC plugins are not (yet) supported due to dynamic loading
# limitations. In this case the headers normally come from the GCC
# build directly via CPPFLAGS.
#
if test x$static_plugin = xyes; then
  AC_WARN([Building static plugin for direct linking into GCC executable!])
  AC_DEFINE([ODB_STATIC_PLUGIN], [1], [Building static plugin.])
  plugindir='$(pkglibexecdir)'

# Otherwise, see if we should install the plugin into the GCC plugin dir.
#
elif test x$gcc_plugin_dir != xno; then
  AC_DEFINE([ODB_GCC_PLUGIN_DIR], [1], [Plugin is in GCC plugin directory.])
  plugindir=$gcc_plugin_dir

# Otherwise, try to figure out a relative path from the driver (bindir) to
# the plugin (libexecdir).
#
elif test x$static_plugin = xno; then
  # Get the expanded values for bindir and libexecdir.
  #
  if test x$exec_prefix = xNONE; then
    if test x$prefix = xNONE; then
      e_exec_prefix=$ac_default_prefix
     else
      e_exec_prefix=$prefix
    fi
  else
    e_exec_prefix=$exec_prefix
  fi

  e_pkglibexecdir=`echo "$libexecdir/$PACKAGE_NAME" | sed "s?^\\\${exec_prefix}?$e_exec_prefix?"`
  e_bindir=`echo "$bindir" | sed "s?^\\\${exec_prefix}?$e_exec_prefix?"`

  # Try to find a common prefix.
  #
  common=$e_bindir
  rel_plugindir=$e_pkglibexecdir

  while test x$common != x/; do
    suffix=`echo "$e_pkglibexecdir" | sed "s?^$common/*??"`
    if test x$suffix != x$e_pkglibexecdir; then
      # Replace all the remaining directories in bindir with ".."
      # and append the suffix.
      rel_plugindir=`echo "$e_bindir" | sed "s?^$common/*??"`
      rel_plugindir=`echo "$rel_plugindir" | sed ['s?[^/][^/]*?..?g']`
      if test x$rel_plugindir != x -a x$suffix != x; then
        rel_plugindir="$rel_plugindir/$suffix"
      else
        rel_plugindir="$rel_plugindir$suffix"
      fi
      break
    fi
    common=`AS_DIRNAME(["$common"])`
  done
  AC_DEFINE_UNQUOTED([ODB_PLUGIN_PATH], ["$rel_plugindir"], [Plugin path.])
  plugindir='$(pkglibexecdir)'
fi

AC_SUBST([plugindir])

# G++ name.
#
AC_ARG_WITH(
  [gxx-name],
  [AC_HELP_STRING([--with-gxx-name=NAME], [g++ executable name to embed in the ODB compiler driver])],
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
  [test "x$gxx_name" != x],
  [AC_DEFINE_UNQUOTED([ODB_GXX_NAME], ["$gxx_name"], [g++ binary.])])

# Default options file.
#
AC_ARG_WITH(
  [options-file],
  [AC_HELP_STRING([--with-options-file=PATH], [default options file path to embed in the driver])],
  [case $withval in
     no)
       options_file=
       ;;
     yes)
       options_file=../etc/odb/default.options
       ;;
     *)
       options_file="$withval"
       ;;
   esac],
  [options_file=])

AS_IF(
  [test "x$options_file" != x],
  [AC_DEFINE_UNQUOTED([ODB_DEFAULT_OPTIONS_FILE], ["$options_file"], [default options file path.])])

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
