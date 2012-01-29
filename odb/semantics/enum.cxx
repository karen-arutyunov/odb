// file      : odb/semantics/enum.cxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
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
  enumerator (path const& file,
              size_t line,
              size_t column,
              tree tn,
              unsigned long long value)
      : node (file, line, column, tn), value_ (value)
  {
  }

  enum_::
  enum_ (path const& file,
         size_t line,
         size_t column,
         tree tn,
         bool unsigned_)
      : node (file, line, column, tn), unsigned__ (unsigned_)
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
