// file      : odb/traversal/relational/key.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_TRAVERSAL_RELATIONAL_KEY_HXX
#define ODB_TRAVERSAL_RELATIONAL_KEY_HXX

#include <odb/semantics/relational/key.hxx>
#include <odb/traversal/relational/elements.hxx>

namespace traversal
{
  namespace relational
  {
    template <typename T>
    struct key_template: node<T>
    {
    public:
      virtual void
      traverse (T& k)
      {
        contains (k);
      }

      virtual void
      contains (T& k)
      {
        contains (k, *this);
      }

      virtual void
      contains (T& k, edge_dispatcher& d)
      {
        iterate_and_dispatch (k.contains_begin (), k.contains_end (), d);
      }
    };

    struct key: key_template<semantics::relational::key> {};
  }
}

#endif // ODB_TRAVERSAL_RELATIONAL_KEY_HXX
