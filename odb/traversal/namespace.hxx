// file      : odb/traversal/namespace.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_NAMESPACE_HXX
#define ODB_TRAVERSAL_NAMESPACE_HXX

#include <odb/semantics/namespace.hxx>
#include <odb/traversal/elements.hxx>

namespace traversal
{
  struct namespace_: scope_template<semantics::namespace_> {};
}

#endif // ODB_TRAVERSAL_NAMESPACE_HXX
