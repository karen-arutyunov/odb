// file      : odb/semantics/relational/column.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/column.hxx>

namespace semantics
{
  namespace relational
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

          // column
          //
          {
            type_info ti (typeid (column));
            ti.add_base (typeid (unameable));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
