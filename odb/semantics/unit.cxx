// file      : odb/semantics/unit.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/unit.hxx>

namespace semantics
{
  unit::
  unit (path const& file)
      : node (file, 1, 1), graph_ (*this)
  {
    // Use a special edge to get this->name() return the global
    // namespace name ("").
    //
    new_edge<global_names> (*this, *this);
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
