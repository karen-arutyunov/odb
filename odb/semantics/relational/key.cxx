// file      : odb/semantics/relational/key.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/key.hxx>

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

          // contains
          //
          {
            type_info ti (typeid (contains));
            ti.add_base (typeid (edge));
            insert (ti);
          }

          // key
          //
          {
            type_info ti (typeid (key));
            ti.add_base (typeid (node));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
