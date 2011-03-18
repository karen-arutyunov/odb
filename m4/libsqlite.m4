dnl file      : m4/libsqlite.m4
dnl author    : Boris Kolpackov <boris@codesynthesis.com>
dnl copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
dnl license   : GNU GPL v2; see accompanying LICENSE file
dnl
dnl LIBSQLITE([ACTION-IF-FOUND[,ACTION-IF-NOT-FOUND]])
dnl
dnl
AC_DEFUN([LIBSQLITE], [
libsqlite_found=no

AC_MSG_CHECKING([for libsqlite3])

save_LIBS="$LIBS"
LIBS="-lsqlite3 $LIBS"

CXX_LIBTOOL_LINK_IFELSE(
AC_LANG_SOURCE([[
#include <sqlite3.h>

int
main ()
{
  sqlite3* handle;
  sqlite3_open_v2 ("", &handle, 0, 0);
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2 (handle, "", 0, &stmt, 0);
  sqlite3_finalize (stmt);
  sqlite3_close (handle);
}
]]),
[
libsqlite_found=yes
])

if test x"$libsqlite_found" = xno; then
  LIBS="$save_LIBS"
fi

if test x"$libsqlite_found" = xyes; then
  AC_MSG_RESULT([yes])
  $1
else
  AC_MSG_RESULT([no])
  $2
fi
])dnl
