// file      : odb/relational/mysql/schema.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
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

      struct create_column: relational::create_column, context
      {
        create_column (base const& x): base (x) {}

        virtual void
        null (sema_rel::column& c)
        {
          // MySQL TIMESTAMP is by default NOT NULL. If we want it
          // to contain NULL values, we need to explicitly declare
          // the column as NULL.
          //
          if (c.null ())
          {
            // This should never fail since we have already parsed this.
            //
            sql_type const& t (parse_sql_type (c.type ()));

            if (t.type == sql_type::TIMESTAMP)
            {
              os << " NULL";
              return;
            }
          }

          base::null (c);
        }

        virtual void
        auto_ (sema_rel::column&)
        {
          os << " AUTO_INCREMENT";
        }
      };
      entry<create_column> create_column_;

      struct create_foreign_key: relational::create_foreign_key, context
      {
        create_foreign_key (base const& x): base (x) {}

        virtual void
        traverse (sema_rel::foreign_key& fk)
        {
          // MySQL does not support deferred constraint checking. Output
          // such foreign keys as comments, for documentation, unless we
          // are generating embedded schema.
          //
          if (fk.deferred ())
          {
            // Don't bloat C++ code with comment strings if we are
            // generating embedded schema.
            //
            if (format_ != schema_format::embedded)
            {
              os << endl
                 << endl
                 << "  /*" << endl;

              base::create (fk);

              os << endl
                 << "  */";
            }
          }
          else
            base::traverse (fk);
        }

        virtual void
        deferred ()
        {
          // MySQL doesn't support deferred.
        }
      };
      entry<create_foreign_key> create_foreign_key_;

      struct create_table: relational::create_table, context
      {
        create_table (base const& x): base (x) {}

        virtual void
        create_post ()
        {
          os << ")";

          string const& engine (options.mysql_engine ());

          if (engine != "default")
            os << endl
               << " ENGINE=" << engine;

          os << endl;
        }
      };
      entry<create_table> create_table_;

      struct create_index: relational::create_index, context
      {
        create_index (base const& x): base (x) {}

        virtual void
        create (sema_rel::index& in)
        {
          os << "CREATE ";

          if (!in.type ().empty ())
            os << in.type () << ' ';

          os << "INDEX " << name (in);

          if (!in.method ().empty ())
            os << " USING " << in.method ();

          os << endl
             << "  ON " << table_name (in) << " (";

          columns (in);

          os << ")" << endl;

          if (!in.options ().empty ())
            os << ' ' << in.options () << endl;
        }
      };
      entry<create_index> create_index_;
    }
  }
}
