// file      : odb/traversal/union.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_UNION_HXX
#define ODB_TRAVERSAL_UNION_HXX

#include <traversal/elements.hxx>
#include <semantics/union.hxx>

namespace traversal
{
  struct union_: scope_template<semantics::union_> {};
}

#endif // ODB_TRAVERSAL_UNION_HXX
