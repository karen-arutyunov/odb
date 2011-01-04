// file      : odb/semantics/derived.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/derived.hxx>

namespace semantics
{
  qualifies::
  qualifies ()
  {
  }

  qualifier::
  qualifier (path const& file,
             size_t line,
             size_t column,
             tree tn,
             bool c,
             bool v,
             bool r)
      : node (file, line, column, tn), c_ (c), v_ (v), r_ (r)
  {
  }

  points::
  points ()
  {
  }

  pointer::
  pointer (path const& file, size_t line, size_t column, tree tn)
      : node (file, line, column, tn)
  {
  }

  references::
  references ()
  {
  }

  reference::
  reference (path const& file, size_t line, size_t column, tree tn)
      : node (file, line, column, tn)
  {
  }

  contains::
  contains ()
  {
  }

  array::
  array (path const& file,
         size_t line,
         size_t column,
         tree tn,
         unsigned long long size)
      : node (file, line, column, tn), size_ (size)
  {
  }

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
