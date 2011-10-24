// file      : odb/traversal/relational/foreign-key.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_RELATIONAL_FOREIGN_KEY_HXX
#define ODB_TRAVERSAL_RELATIONAL_FOREIGN_KEY_HXX

#include <odb/semantics/relational/foreign-key.hxx>
#include <odb/traversal/relational/key.hxx>

namespace traversal
{
  namespace relational
  {
    struct foreign_key: key_template<semantics::relational::foreign_key> {};
  }
}

#endif // ODB_TRAVERSAL_RELATIONAL_FOREIGN_KEY_HXX
