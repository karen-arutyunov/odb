// file      : odb/relational/pgsql/schema.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/schema.hxx>

#include <odb/relational/pgsql/common.hxx>
#include <odb/relational/pgsql/context.hxx>

using namespace std;

namespace relational
{
  namespace pgsql
  {
    namespace schema
    {
      namespace relational = relational::schema;
      using relational::table_set;

      //
      // Drop.
      //

      struct drop_table: relational::drop_table, context
      {
        drop_table (base const& x): base (x) {}

        virtual void
        traverse (sema_rel::table& t, bool migration)
        {
          // For PostgreSQL we use the CASCADE clause to drop foreign keys.
          //
          if (pass_ != 2)
            return;

          pre_statement ();
          os << "DROP TABLE " << (migration ? "" : "IF EXISTS ") <<
            quote_id (t.name ()) << " CASCADE" << endl;
          post_statement ();
        }
      };
      entry<drop_table> drop_table_;

      //
      // Create.
      //

      struct create_column: relational::create_column, context
      {
        create_column (base const& x): base (x) {}

        virtual void
        type (sema_rel::column& c, bool auto_)
        {
          if (auto_)
          {
            // This should never fail since we have already parsed this.
            //
            sql_type const& t (parse_sql_type (c.type ()));

            // The model creation code makes sure it is one of these type.
            //
            if (t.type == sql_type::INTEGER)
              os << "SERIAL";
            else if (t.type == sql_type::BIGINT)
              os << "BIGSERIAL";
          }
          else
            base::type (c, auto_);
        }
      };
      entry<create_column> create_column_;

      struct create_foreign_key: relational::create_foreign_key, context
      {
        create_foreign_key (base const& x): base (x) {}

        virtual void
        deferrable (sema_rel::deferrable d)
        {
          os << endl
             << "    INITIALLY " << d;
        }
      };
      entry<create_foreign_key> create_foreign_key_;

      struct create_index: relational::create_index, context
      {
        create_index (base const& x): base (x) {}

        virtual void
        create (sema_rel::index& in)
        {
          os << "CREATE ";

          if (!in.type ().empty ())
          {
            // Handle the CONCURRENTLY keyword.
            //
            string const& t (in.type ());

            if (t == "concurrently" || t == "CONCURRENTLY")
            {
              os << "INDEX " << t;
            }
            else
            {
              size_t p (t.rfind (' '));
              string s (t, (p != string::npos ? p + 1 : 0), string::npos);

              if (s == "concurrently" || s == "CONCURRENTLY")
                os << string (t, 0, p) << " INDEX " << s;
              else
                os << t << " INDEX";
            }
          }
          else
            os << "INDEX";

          os << " " << name (in) << endl
             << "  ON " << table_name (in);

          if (!in.method ().empty ())
            os << " USING " << in.method ();

          os << " (";
          columns (in);
          os << ")" << endl;

          if (!in.options ().empty ())
            os << ' ' << in.options () << endl;
        }
      };
      entry<create_index> create_index_;

      //
      // Alter.
      //

      struct alter_column: relational::alter_column, context
      {
        alter_column (base const& x): base (x) {}

        virtual void
        alter (sema_rel::column& c)
        {
          os << quote_id (c.name ()) << " " <<
            (c.null () ? "DROP" : "SET") << " NOT NULL";
        }
      };
      entry<alter_column> alter_column_;
    }
  }
}
