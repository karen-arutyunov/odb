// file      : odb/semantics/elements.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

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
