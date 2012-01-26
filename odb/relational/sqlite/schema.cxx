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

      struct create_foreign_key: relational::create_foreign_key, context
      {
        create_foreign_key (base const& x): base (x) {}

        virtual string
        table_name (sema_rel::foreign_key& fk)
        {
          // In SQLite, the referenced table cannot be qualified with the
          // database name (it has to be in the same database anyway).
          //
          return quote_id (fk.referenced_table ().uname ());
        }
      };
      entry<create_foreign_key> create_foreign_key_;

      struct create_index: relational::create_index, context
      {
        create_index (base const& x): base (x) {}

        virtual string
        table_name (sema_rel::index& in)
        {
          // In SQLite, the index table cannot be qualified with the
          // database name (it has to be in the same database anyway).
          //
          return quote_id (in.table ().name ().uname ());
        }
      };
      entry<create_index> create_index_;
    }
  }
}
