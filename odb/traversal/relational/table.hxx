// file      : odb/traversal/relational/table.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_RELATIONAL_TABLE_HXX
#define ODB_TRAVERSAL_RELATIONAL_TABLE_HXX

#include <odb/semantics/relational/table.hxx>
#include <odb/traversal/relational/elements.hxx>

namespace traversal
{
  namespace relational
  {
    struct table: scope_template<semantics::relational::table> {};
    struct object_table: scope_template<semantics::relational::table> {};
    struct container_table: scope_template<semantics::relational::table> {};
  }
}

#endif // ODB_TRAVERSAL_RELATIONAL_TABLE_HXX
