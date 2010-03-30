// file      : odb/semantics/unit.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/unit.hxx>

namespace semantics
{
  // type info
  //
  namespace
  {
    struct init
    {
      init ()
      {
        using compiler::type_info;

        // unit
        //
        {
          type_info ti (typeid (unit));
          ti.add_base (typeid (namespace_));
          insert (ti);
        }
      }
    } init_;
  }
}
