// file      : odb/traversal/union-template.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_UNION_TEMPLATE_HXX
#define ODB_TRAVERSAL_UNION_TEMPLATE_HXX

#include <traversal/elements.hxx>
#include <traversal/union.hxx>
#include <semantics/union-template.hxx>

namespace traversal
{
  struct union_template: scope_template<semantics::union_template> {};

  struct union_instantiation: scope_template<semantics::union_instantiation>
  {
    virtual void
    traverse (type&);

    virtual void
    instantiates (type&);

    virtual void
    instantiates (type&, edge_dispatcher&);
  };
}

#endif // ODB_TRAVERSAL_UNION_TEMPLATE_HXX
