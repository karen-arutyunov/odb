// file      : odb/semantics/derived.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/derived.hxx>

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

        // derived_type
        //
        {
          type_info ti (typeid (derived_type));
          ti.add_base (typeid (type));
          insert (ti);
        }

        // qualifies
        //
        {
          type_info ti (typeid (qualifies));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // qualifier
        //
        {
          type_info ti (typeid (qualifier));
          ti.add_base (typeid (derived_type));
          insert (ti);
        }

        // points
        //
        {
          type_info ti (typeid (points));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // pointer
        //
        {
          type_info ti (typeid (pointer));
          ti.add_base (typeid (derived_type));
          insert (ti);
        }

        // references
        //
        {
          type_info ti (typeid (references));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // reference
        //
        {
          type_info ti (typeid (reference));
          ti.add_base (typeid (derived_type));
          insert (ti);
        }

        // contains
        //
        {
          type_info ti (typeid (contains));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // array
        //
        {
          type_info ti (typeid (array));
          ti.add_base (typeid (derived_type));
          insert (ti);
        }
      }
    } init_;
  }
}
