// file      : odb/traversal/enum.cxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/traversal/enum.hxx>

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
