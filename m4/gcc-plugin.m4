dnl file      : m4/gcc-plugin.m4
dnl author    : Boris Kolpackov <boris@codesynthesis.com>
dnl copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
dnl license   : GNU GPL v3; see accompanying LICENSE file
dnl
dnl GCC_PLUGIN
dnl
AC_DEFUN([GCC_PLUGIN], [
gcc_plugin_support=no

if test x"$GXX" != xyes; then
  AC_MSG_ERROR([$CXX is not a GNU C++ compiler])
fi

AC_MSG_CHECKING([whether $CXX supports plugins])

gcc_plugin_base=`$CXX -print-file-name=plugin 2>/dev/null`

if test x"$gcc_plugin_base" = xplugin; then
  AC_MSG_RESULT([no])
  AC_MSG_ERROR([$CXX does not support plugins; reconfigure GCC with --enable-plugin])
else
  AC_MSG_RESULT([yes])
  gcc_plugin_support=yes
fi

CPPFLAGS="$CPPFLAGS -I$gcc_plugin_base/include"
])dnl
