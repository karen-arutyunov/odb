// file      : odb/traversal/enum.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_ENUM_HXX
#define ODB_TRAVERSAL_ENUM_HXX

#include <traversal/elements.hxx>
#include <semantics/enum.hxx>

namespace traversal
{
  struct enumerates: edge<semantics::enumerates>
  {
    enumerates ()
    {
    }

    enumerates (node_dispatcher& n)
    {
      node_traverser (n);
    }

    virtual void
    traverse (type&);
  };

  struct enumerator: node<semantics::enumerator> {};

  struct enum_: node<semantics::enum_>
  {
    virtual void
    traverse (type&);

    virtual void
    enumerates (type&);

    virtual void
    enumerates (type&, edge_dispatcher&);
  };
}

#endif // ODB_TRAVERSAL_ENUM_HXX
