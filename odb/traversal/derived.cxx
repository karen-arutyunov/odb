// file      : odb/traversal/derived.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <traversal/derived.hxx>

namespace traversal
{
  void qualifies::
  traverse (type& e)
  {
    dispatch (e.type ());
  }

  void points::
  traverse (type& e)
  {
    dispatch (e.type ());
  }

  void references::
  traverse (type& e)
  {
    dispatch (e.type ());
  }

  void contains::
  traverse (type& e)
  {
    dispatch (e.type ());
  }

  //
  //
  void qualifier::
  traverse (type& q)
  {
    qualifies (q);
  }

  void qualifier::
  qualifies (type& q)
  {
    qualifies (q, *this);
  }

  void qualifier::
  qualifies (type& q, edge_dispatcher& d)
  {
    d.dispatch (q.qualifies ());
  }

  //
  //
  void pointer::
  traverse (type& p)
  {
    points (p);
  }

  void pointer::
  points (type& p)
  {
    points (p, *this);
  }

  void pointer::
  points (type& p, edge_dispatcher& d)
  {
    d.dispatch (p.points ());
  }

  //
  //
  void reference::
  traverse (type& r)
  {
    references (r);
  }

  void reference::
  references (type& r)
  {
    references (r, *this);
  }

  void reference::
  references (type& r, edge_dispatcher& d)
  {
    d.dispatch (r.references ());
  }

  //
  //
  void array::
  traverse (type& a)
  {
    contains (a);
  }

  void array::
  contains (type& a)
  {
    contains (a, *this);
  }

  void array::
  contains (type& a, edge_dispatcher& d)
  {
    d.dispatch (a.contains ());
  }
}
