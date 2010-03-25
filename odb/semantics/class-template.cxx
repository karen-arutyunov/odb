// file      : odb/semantics/class-template.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <semantics/class-template.hxx>

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

        // class_template
        //
        {
          type_info ti (typeid (class_template));
          ti.add_base (typeid (type_template));
          ti.add_base (typeid (scope));
          insert (ti);
        }

        // class_instantiation
        //
        {
          type_info ti (typeid (class_instantiation));
          ti.add_base (typeid (class_));
          ti.add_base (typeid (type_instantiation));
          insert (ti);
        }
      }
    } init_;
  }
}
