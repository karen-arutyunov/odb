// file      : odb/relational/mssql/schema.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
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
        traverse (sema_rel::table&);

      private:
        friend class drop_foreign_key;
        set<qname> tables_; // Set of tables we would have already dropped.
      };
      entry<drop_table> drop_table_;

      struct drop_foreign_key: trav_rel::foreign_key, relational::common
      {
        drop_foreign_key (drop_table& dt)
            : common (dt.emitter (), dt.stream ()), dt_ (dt)
        {
        }

        virtual void
        traverse (sema_rel::foreign_key& fk)
        {
          // Deferred constraints are not supported by SQL Server.
          //
          if (fk.deferred ())
            return;

          // If the table which we reference is droped before us, then
          // we need to drop the constraint first. Similarly, if the
          // referenced table is not part if this model, then assume
          // it is dropped before us.
          //
          sema_rel::qname const& rt (fk.referenced_table ());
          sema_rel::table& t (dynamic_cast<sema_rel::table&> (fk.scope ()));
          sema_rel::model& m (dynamic_cast<sema_rel::model&> (t.scope ()));

          if (dt_.tables_.find (rt) != dt_.tables_.end () ||
              m.find (rt) == m.names_end ())
          {
            pre_statement ();
            os << "IF OBJECT_ID(" << quote_string (fk.name ()) << ", " <<
              quote_string ("F") << ") IS NOT NULL" << endl
               << "  ALTER TABLE " << quote_id (t.name ()) << " DROP" << endl
               << "    CONSTRAINT " << quote_id (fk.name ()) << endl;
            post_statement ();
          }
        }

      private:
        drop_table& dt_;
      };

      void drop_table::
      traverse (sema_rel::table& t)
      {
        qname const& table (t.name ());

        if (pass_ == 1)
        {
          // Drop constraints.
          //
          tables_.insert (table); // Add it before to cover self-refs.
          drop_foreign_key fk (*this);
          trav_rel::unames n (fk);
          names (t, n);
        }
        else if (pass_ == 2)
        {
          // SQL Server has no IF EXISTS conditional for dropping table.
          // The following approach appears to be the recommended way to
          // drop a table if it exists.
          //
          pre_statement ();
          os << "IF OBJECT_ID(" << quote_string (table.string ()) <<
            ", " << quote_string ("U") << ") IS NOT NULL" << endl
             << "  DROP TABLE " << quote_id (table) << endl;
          post_statement ();
        }
      }

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
        add_foreign_key fk (format_, *this);
        trav_rel::unames n (fk);
        names (t, n);
      }
    }
  }
}
