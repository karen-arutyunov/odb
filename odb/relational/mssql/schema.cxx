// file      : odb/relational/mssql/schema.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <set>

#include <odb/relational/schema.hxx>

#include <odb/relational/mssql/common.hxx>
#include <odb/relational/mssql/context.hxx>

using namespace std;

namespace relational
{
  namespace mssql
  {
    namespace schema
    {
      namespace relational = relational::schema;

      struct sql_emitter: relational::sql_emitter
      {
        sql_emitter (const base& x): base (x) {}

        virtual void
        post ()
        {
          if (!first_) // Ignore empty statements.
          {
            os << ';' << endl
               << "GO" << endl
               << endl;
          }
        }
      };
      entry<sql_emitter> sql_emitter_;

      //
      // Drop.
      //

      struct drop_table: relational::drop_table, context
      {
        drop_table (base const& x): base (x) {}

        virtual void
        drop (sema_rel::qname const& table)
        {
          // SQL Server has no IF EXISTS conditional for dropping table.
          // The following approach appears to be the recommended way to
          // drop a table if it exists.
          //
          os << "IF OBJECT_ID(" << quote_string (table.string ()) <<
            ", " << quote_string ("U") << ") IS NOT NULL" << endl
             << "  DROP TABLE " << quote_id (table) << endl;
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
        auto_ (sema_rel::column&)
        {
          os << " IDENTITY";
        }
      };
      entry<create_column> create_column_;

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
            // SQL Server does not support deferred constraint checking.
            // Output such foreign keys as comments, for documentation,
            // unless we are generating embedded schema.
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

            fk.set ("mssql-fk-defined", true); // Mark it as defined.
          }
        }

        virtual string
        name (sema_rel::foreign_key& fk)
        {
          // In SQL Server, foreign key names are schema-global. Make them
          // unique by prefixing the key name with table name. Note, however,
          // that they cannot have a schema.
          //
          return quote_id (
            static_cast<sema_rel::table&> (fk.scope ()).name ().uname ()
            + "_" + fk.name ());
        }

        virtual void
        deferred ()
        {
          // SQL Server doesn't support deferred.
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
          if (!fk.count ("mssql-fk-defined"))
          {
            sema_rel::table& t (dynamic_cast<sema_rel::table&> (fk.scope ()));

            // SQL Server has no deferred constraints.
            //
            if (fk.deferred ())
            {
              if (format_ != schema_format::embedded)
              {
                os << "/*" << endl;

                os << "ALTER TABLE " << quote_id (t.name ()) << " ADD" << endl;
                base::create (fk);

                os << endl
                   << "*/" << endl
                   << endl;
              }
            }
            else
            {
              pre_statement ();

              os << "ALTER TABLE " << quote_id (t.name ()) << " ADD" << endl;
              base::create (fk);
              os << endl;

              post_statement ();
            }
          }
        }
      };

      void create_table::
      traverse (sema_rel::table& t)
      {
        if (pass_ == 1)
        {
          tables_.insert (t.name ()); // Add it before to cover self-refs.
          base::traverse (t);
          return;
        }

        // Add foreign keys.
        //
        instance<add_foreign_key> fk (format_, *this);
        trav_rel::unames n (*fk);
        names (t, n);
      }

      struct create_index: relational::create_index, context
      {
        create_index (base const& x): base (x) {}

        virtual string
        name (sema_rel::index& in)
        {
          // In SQL Server indexes cannot have a schema.
          //
          return quote_id (in.name ().uname ());
        }
      };
      entry<create_index> create_index_;
    }
  }
}
