// file      : odb/semantics/relational/primary-key.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/primary-key.hxx>

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

          // primary_key
          //
          {
            type_info ti (typeid (primary_key));
            ti.add_base (typeid (key));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
