// file      : odb/semantics/enum.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/enum.hxx>

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

        // enumerates
        //
        {
          type_info ti (typeid (enumerates));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // enumerator
        //
        {
          type_info ti (typeid (enumerator));
          ti.add_base (typeid (instance));
          insert (ti);
        }

        // enum_
        //
        {
          type_info ti (typeid (enum_));
          ti.add_base (typeid (type));
          insert (ti);
        }
      }
    } init_;
  }
}
