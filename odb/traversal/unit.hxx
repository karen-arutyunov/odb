// file      : odb/traversal/unit.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_UNIT_HXX
#define ODB_TRAVERSAL_UNIT_HXX

#include <traversal/elements.hxx>
#include <semantics/unit.hxx>

namespace traversal
{
  struct unit: scope_template<semantics::unit> {};
}

#endif // ODB_TRAVERSAL_UNIT_HXX
