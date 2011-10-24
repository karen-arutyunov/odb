// file      : odb/traversal/relational/model.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_RELATIONAL_MODEL_HXX
#define ODB_TRAVERSAL_RELATIONAL_MODEL_HXX

#include <odb/semantics/relational/model.hxx>
#include <odb/traversal/relational/elements.hxx>

namespace traversal
{
  namespace relational
  {
    struct model: scope_template<semantics::relational::model> {};
  }
}

#endif // ODB_TRAVERSAL_RELATIONAL_MODEL_HXX
