// file      : odb/semantics/relational/table.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/table.hxx>

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

          // table
          //
          {
            type_info ti (typeid (table));
            ti.add_base (typeid (qnameable));
            ti.add_base (typeid (uscope));
            insert (ti);
          }

          // object_table
          //
          {
            type_info ti (typeid (object_table));
            ti.add_base (typeid (table));
            insert (ti);
          }

          // container_table
          //
          {
            type_info ti (typeid (container_table));
            ti.add_base (typeid (table));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
