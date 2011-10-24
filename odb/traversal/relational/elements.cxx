// file      : odb/traversal/relational/elements.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/traversal/relational/elements.hxx>

namespace traversal
{
  namespace relational
  {
    void names::
    traverse (type& e)
    {
      dispatch (e.nameable ());
    }
  }
}
