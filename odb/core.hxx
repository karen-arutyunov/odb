// file      : odb/core.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_CORE_HXX
#define ODB_CORE_HXX

#include <odb/pre.hxx>

#ifdef ODB_COMPILER
#  define ODB_PRAGMA_IMPL(x) _Pragma (#x)
#  define ODB_PRAGMA(x) ODB_PRAGMA_IMPL (odb x)
#else
#  define PRAGMA_ODB(x)
#endif

#include <odb/forward.hxx>

#include <odb/post.hxx>

#endif // ODB_CORE_HXX