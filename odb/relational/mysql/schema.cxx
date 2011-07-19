// file      : odb/relational/mysql/schema.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/schema.hxx>

#include <odb/relational/mysql/common.hxx>
#include <odb/relational/mysql/context.hxx>

namespace relational
{
  namespace mysql
  {
    namespace schema
    {
      namespace relational = relational::schema;

      //
      // Create.
      //

      struct create_common: virtual relational::create_common
      {
        virtual void
        create_table_post ()
        {
          os << ")";

          string const& engine (options.mysql_engine ());

          if (engine != "default")
            os << endl
               << " ENGINE=" << engine;

          os << endl;
        }
      };

      struct object_columns: relational::object_columns, context
      {
        object_columns (base const& x): base (x) {}

        virtual void
        null (semantics::data_member& m)
        {
          if (!context::null (m, prefix_))
            os << " NOT NULL";
          else
          {
            // MySQL TIMESTAMP is by default NOT NULL. If we want it
            // to contain NULL values, we need to explicitly declare
            // the column as NULL.
            //
            if (column_sql_type (m, prefix_).type == sql_type::TIMESTAMP)
              os << " NULL";
          }
        }

        virtual void
        constraints (semantics::data_member& m)
        {
          base::constraints (m);

          if (m.count ("auto"))
            os << " AUTO_INCREMENT";
        }

      };
      entry<object_columns> object_columns_;

      struct member_create: relational::member_create, create_common
      {
        member_create (base const& x): base (x) {}
      };
      entry<member_create> member_create_;

      struct class_create: relational::class_create, create_common
      {
        class_create (base const& x): base (x) {}
      };
      entry<class_create> class_create_;
    }
  }
}
