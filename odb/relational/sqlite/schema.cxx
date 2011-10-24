// file      : odb/relational/sqlite/schema.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/schema.hxx>

#include <odb/relational/sqlite/common.hxx>
#include <odb/relational/sqlite/context.hxx>

namespace relational
{
  namespace sqlite
  {
    namespace schema
    {
      namespace relational = relational::schema;

      //
      // Create.
      //

      struct create_column: relational::create_column, context
      {
        create_column (base const& x): base (x) {}

        virtual void
        auto_ (sema_rel::column&)
        {
          if (options.sqlite_lax_auto_id ())
            os << " /*AUTOINCREMENT*/";
          else
            os << " AUTOINCREMENT";
        }
      };
      entry<create_column> create_column_;
    }
  }
}
