// file      : odb/semantics/elements.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

namespace semantics
{
  // typedefs
  //
  inline typedefs::type_type& typedefs::
  type () const
  {
    return dynamic_cast<type_type&> (named ());
  }
}