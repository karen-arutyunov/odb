// file      : odb/traversal/relational/index.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_RELATIONAL_INDEX_HXX
#define ODB_TRAVERSAL_RELATIONAL_INDEX_HXX

#include <odb/semantics/relational/index.hxx>
#include <odb/traversal/relational/key.hxx>

namespace traversal
{
  namespace relational
  {
    struct index: key_template<semantics::relational::index> {};
  }
}

#endif // ODB_TRAVERSAL_RELATIONAL_INDEX_HXX
