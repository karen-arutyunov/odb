// file      : odb/semantics/namespace.cxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/namespace.hxx>

namespace semantics
{
  namespace_::
  namespace_ (path const& file, size_t line, size_t column, tree tn)
      : node (file, line, column, tn), original_ (0)
  {
  }

  namespace_::
  namespace_ ()
      : original_ (0)
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

        type_info ti (typeid (namespace_));
        ti.add_base (typeid (scope));
        insert (ti);
      }
    } init_;
  }
}
