// file      : odb/semantics/relational/index.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/index.hxx>

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

          // index
          //
          {
            type_info ti (typeid (index));
            ti.add_base (typeid (qnameable));
            ti.add_base (typeid (key));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
