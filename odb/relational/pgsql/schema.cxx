// file      : odb/relational/pgsql/schema.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <set>

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

      //
      // Drop.
      //

      struct drop_table: relational::drop_table, context
      {
        drop_table (base const& x): base (x) {}

        virtual void
        drop (sema_rel::qname const& table, bool migration)
        {
          os << "DROP TABLE " << (migration ? "" : "IF EXISTS ") <<
            quote_id (table) << " CASCADE" << endl;
        }
      };
      entry<drop_table> drop_table_;

      //
      // Create.
      //

      struct create_foreign_key;

      struct create_table: relational::create_table, context
      {
        create_table (base const& x): base (x) {}

        void
        traverse (sema_rel::table&);

      private:
        friend class create_foreign_key;
        set<qname> tables_; // Set of tables we have already defined.
      };
      entry<create_table> create_table_;

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
        create_foreign_key (schema_format f, relational::create_table& ct)
            : base (f, ct)
        {
        }

        create_foreign_key (base const& x): base (x) {}

        virtual void
        traverse (sema_rel::foreign_key& fk)
        {
          // If the referenced table has already been defined, do the
          // foreign key definition in the table definition. Otherwise
          // postpone it until pass 2 where we do it via ALTER TABLE
          // (see add_foreign_key below).
          //
          create_table& ct (static_cast<create_table&> (create_table_));

          if (ct.tables_.find (fk.referenced_table ()) != ct.tables_.end ())
          {
            base::traverse (fk);
            fk.set ("pgsql-fk-defined", true); // Mark it as defined.
          }
        }

        virtual void
        deferred ()
        {
          os << endl
             << "    INITIALLY DEFERRED";
        }
      };
      entry<create_foreign_key> create_foreign_key_;

      struct add_foreign_key: create_foreign_key, relational::common
      {
        add_foreign_key (schema_format f, relational::create_table& ct)
            : create_foreign_key (f, ct), common (ct.emitter (), ct.stream ())
        {
        }

        virtual void
        traverse (sema_rel::foreign_key& fk)
        {
          if (!fk.count ("pgsql-fk-defined"))
          {
            sema_rel::table& t (dynamic_cast<sema_rel::table&> (fk.scope ()));

            pre_statement ();

            os << "ALTER TABLE " << quote_id (t.name ()) << " ADD" << endl;
            base::create (fk);
            os << endl;

            post_statement ();
          }
        }
      };

      void create_table::
      traverse (sema_rel::table& t)
      {
        if (pass_ == 1)
        {
          // In migration we always add foreign keys on pass 2.
          //
          if (!t.is_a<sema_rel::add_table> ())
            tables_.insert (t.name ()); // Add it before to cover self-refs.
          base::traverse (t);
          return;
        }

        // Add foreign keys.
        //
        add_foreign_key fk (format_, *this);
        trav_rel::unames n (fk);
        names (t, n);
      }

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
