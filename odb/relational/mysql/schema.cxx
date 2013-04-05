// file      : odb/relational/mysql/schema.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <set>

#include <odb/relational/schema.hxx>

#include <odb/relational/mysql/common.hxx>
#include <odb/relational/mysql/context.hxx>

using namespace std;

namespace relational
{
  namespace mysql
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
        traverse (sema_rel::table&, bool migration);

      private:
        friend class drop_foreign_key;
        set<qname> tables_; // Set of tables we would have already dropped.
      };
      entry<drop_table> drop_table_;

      struct drop_foreign_key: trav_rel::foreign_key, relational::common
      {
        drop_foreign_key (drop_table& dt, bool m)
            : common (dt.emitter (), dt.stream ()), dt_ (dt), migration_ (m)
        {
        }

        virtual void
        traverse (sema_rel::foreign_key& fk)
        {
          // Deferred constraints are not supported by MySQL.
          //
          if (fk.deferred ())
            return;

          // If the table which we reference is droped before us, then
          // we need to drop the constraint first. Similarly, if the
          // referenced table is not part if this model, then assume
          // it is dropped before us. In migration we always do this
          // first.
          //
          sema_rel::table& t (dynamic_cast<sema_rel::table&> (fk.scope ()));

          if (!migration_)
          {
            sema_rel::qname const& rt (fk.referenced_table ());
            sema_rel::model& m (dynamic_cast<sema_rel::model&> (t.scope ()));

            if (dt_.tables_.find (rt) == dt_.tables_.end () &&
                m.find (rt) != m.names_end ())
              return;
          }

          pre_statement ();

          /*
          // @@ This does not work: in MySQL control statements can only
          //    be used in stored procedures. It seems the only way to
          //    implement this is to define, execute, and drop a stored
          //    procedure, which is just too ugly.
          //
          //    Another option would be to use CREATE TABLE IF NOT EXISTS
          //    to create a dummy table with a dummy constraint that makes
          //    the following DROP succeed. Note, however, that MySQL issues
          //    a notice if the table already exist so would need to suppress
          //    that as well. Still not sure that the utility of this support
          //    justifies this kind of a hack.
          //
          os << "IF EXISTS (SELECT NULL FROM information_schema.TABLE_CONSTRAINTS" << endl
             << "    WHERE CONSTRAINT_TYPE = " << quote_string ("FOREIGN KEY") << "AND" << endl
             << "          CONSTRAINT_SCHEMA = DATABASE() AND" << endl
             << "          CONSTRAINT_NAME = " << quote_string (fk.name ()) << ") THEN" << endl
             << "  ALTER TABLE " << quote_id (t.name ()) << " DROP FOREIGN KEY " << quote_id (fk.name ()) << ";" << endl
             << "END IF;" << endl;
          */

          os << "ALTER TABLE " << quote_id (t.name ()) << " DROP FOREIGN " <<
            "KEY " << quote_id (fk.name ()) << endl;

          post_statement ();
        }

      private:
        drop_table& dt_;
        bool migration_;
      };

      void drop_table::
      traverse (sema_rel::table& t, bool migration)
      {
        // Only enabled for migration support for now (see above).
        //
        if (!migration)
        {
          base::traverse (t, migration);
          return;
        }

        qname const& table (t.name ());

        if (pass_ == 1)
        {
          // Drop constraints. In migration this is always done on pass 1.
          //
          if (!migration)
            tables_.insert (table); // Add it before to cover self-refs.
          drop_foreign_key fk (*this, migration);
          trav_rel::unames n (fk);
          names (t, n);
        }
        else if (pass_ == 2)
        {
          pre_statement ();
          os << "DROP TABLE " << (migration ? "" : "IF EXISTS ") <<
            quote_id (table) << endl;
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

      struct create_foreign_key;

      struct create_table: relational::create_table, context
      {
        create_table (base const& x): base (x) {}

        void
        traverse (sema_rel::table&);

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

            fk.set ("mysql-fk-defined", true); // Mark it as defined.
          }
        }

        virtual void
        deferred ()
        {
          // MySQL doesn't support deferred.
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
          if (!fk.count ("mysql-fk-defined"))
          {
            sema_rel::table& t (dynamic_cast<sema_rel::table&> (fk.scope ()));

            // MySQL has no deferred constraints.
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

      struct drop_index: relational::drop_index, context
      {
        drop_index (base const& x): base (x) {}

        virtual void
        drop (sema_rel::index& in)
        {
          sema_rel::table& t (static_cast<sema_rel::table&> (in.scope ()));

          os << "DROP INDEX " << name (in) << " ON " <<
            quote_id (t.name ()) << endl;
        }
      };
      entry<drop_index> drop_index_;

      struct alter_column: relational::alter_column, context
      {
        alter_column (base const& x): base (x) {}

        virtual void
        alter_header ()
        {
          os << "MODIFY COLUMN ";
        }
      };
      entry<alter_column> alter_column_;
    }
  }
}
