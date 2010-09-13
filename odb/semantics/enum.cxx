// file      : odb/semantics/enum.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/enum.hxx>

namespace semantics
{
  enumerates::
  enumerates ()
  {
  }

  enumerator::
  enumerator (path const& file, size_t line, size_t column)
      : node (file, line, column)
  {
  }

  enum_::
  enum_ (path const& file, size_t line, size_t column, tree tn)
      : node (file, line, column), type (tn)
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
