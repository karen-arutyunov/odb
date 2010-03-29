// file      : odb/traversal/template.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_TEMPLATE_HXX
#define ODB_TRAVERSAL_TEMPLATE_HXX

#include <traversal/elements.hxx>
#include <semantics/template.hxx>

namespace traversal
{
  struct instantiates: edge<semantics::instantiates>
  {
    instantiates ()
    {
    }

    instantiates (node_dispatcher& n)
    {
      node_traverser (n);
    }

    virtual void
    traverse (type&);
  };

  struct template_: node<semantics::template_> {};

  struct instantiation: node<semantics::instantiation>
  {
    virtual void
    traverse (type&);

    virtual void
    instantiates (type&);

    virtual void
    instantiates (type&, edge_dispatcher&);
  };

  struct type_template: node<semantics::type_template> {};

  struct type_instantiation: node<semantics::type_instantiation>
  {
    virtual void
    traverse (type&);

    virtual void
    instantiates (type&);

    virtual void
    instantiates (type&, edge_dispatcher&);
  };
}

#endif // ODB_TRAVERSAL_TEMPLATE_HXX
