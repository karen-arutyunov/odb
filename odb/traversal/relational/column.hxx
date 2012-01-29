// file      : odb/traversal/relational/column.hxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_RELATIONAL_COLUMN_HXX
#define ODB_TRAVERSAL_RELATIONAL_COLUMN_HXX

#include <odb/semantics/relational/column.hxx>
#include <odb/traversal/relational/elements.hxx>

namespace traversal
{
  namespace relational
  {
    struct column: node<semantics::relational::column> {};
  }
}

#endif // ODB_TRAVERSAL_RELATIONAL_COLUMN_HXX
