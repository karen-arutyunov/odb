// file      : odb/semantics/relational/model.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/model.hxx>

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

          // model
          //
          {
            type_info ti (typeid (model));
            ti.add_base (typeid (qscope));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
