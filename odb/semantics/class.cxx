// file      : odb/semantics/class.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/class.hxx>

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

        // inherits
        //
        {
          type_info ti (typeid (inherits));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // class_
        //
        {
          type_info ti (typeid (class_));
          ti.add_base (typeid (type));
          ti.add_base (typeid (scope));
          insert (ti);
        }
      }
    } init_;
  }
}
