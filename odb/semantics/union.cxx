// file      : odb/semantics/union.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/union.hxx>

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

        // union_
        //
        {
          type_info ti (typeid (union_));
          ti.add_base (typeid (type));
          ti.add_base (typeid (scope));
          insert (ti);
        }
      }
    } init_;
  }
}
