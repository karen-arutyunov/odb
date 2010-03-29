// file      : odb/traversal/enum.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <traversal/enum.hxx>

namespace traversal
{
  void enumerates::
  traverse (type& e)
  {
    dispatch (e.enumerator ());
  }

  //
  //
  void enum_::
  traverse (type& e)
  {
    enumerates (e);
  }

  void enum_::
  enumerates (type& e)
  {
    enumerates (e, *this);
  }

  void enum_::
  enumerates (type& e, edge_dispatcher& d)
  {
    iterate_and_dispatch (e.enumerates_begin (), e.enumerates_end (), d);
  }
}
