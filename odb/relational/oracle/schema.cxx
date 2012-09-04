// file      : odb/relational/oracle/schema.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <set>

#include <odb/relational/schema.hxx>

#include <odb/relational/oracle/common.hxx>
#include <odb/relational/oracle/context.hxx>

using namespace std;

namespace relational
{
  namespace oracle
  {
    namespace schema
    {
      namespace relational = relational::schema;

      struct sql_emitter: relational::sql_emitter
      {
        sql_emitter (const base& x): base (x) {}

        virtual void
        line (const std::string& l)
        {
          // SQLPlus doesn't like empty line in the middle of a statement.
          //
          if (!l.empty ())
          {
            base::line (l);
            last_ = l;
          }
        }

        virtual void
        post ()
        {
          if (!first_) // Ignore empty statements.
          {
            if (last_ == "END;")
              os << endl
                 << '/' << endl
                 << endl;

            else
              os << ';' << endl
                 << endl;
          }
        }

      private:
        string last_;
      };
      entry<sql_emitter> sql_emitter_;

      //
      // File.
      //

      struct sql_file: relational::sql_file, context
      {
        sql_file (const base& x): base (x) {}

        virtual void
        prologue ()
        {
          // Quiet down SQLPlus and make sure it exits with an error
          // code if there is an error.
          //
          os << "SET FEEDBACK OFF;" << endl
             << "WHENEVER SQLERROR EXIT FAILURE;" << endl
             << "WHENEVER OSERROR EXIT FAILURE;" << endl
             << endl;
        }

        virtual void
        epilogue ()
        {
          os << "EXIT;" << endl;
        }
      };
      entry<sql_file> sql_file_;

      //
      // Drop.
      //

      struct drop_table: relational::drop_table, context
      {
        drop_table (base const& x): base (x) {}

        virtual void
        traverse (sema_rel::table& t)
        {
          if (pass_ != 1)
            return;

          qname const& table (t.name ());

          // Oracle has no IF EXISTS conditional for dropping objects. The
          // PL/SQL approach below seems to be the least error-prone and the
          // most widely used of the alternatives.
          //
          pre_statement ();
          os << "BEGIN" << endl
             << "  BEGIN" << endl
             << "    EXECUTE IMMEDIATE 'DROP TABLE " << quote_id (table) <<
            " CASCADE CONSTRAINTS';" << endl
             << "  EXCEPTION" << endl
             << "    WHEN OTHERS THEN" << endl
             << "      IF SQLCODE != -942 THEN RAISE; END IF;" << endl
             << "  END;" << endl;

          // Drop the sequence if we have auto primary key.
          //
          using sema_rel::primary_key;

          sema_rel::table::names_iterator i (t.find ("")); // Special name.
          primary_key* pk (i != t.names_end ()
                           ? &dynamic_cast<primary_key&> (i->nameable ())
                           : 0);

          if (pk != 0 && pk->auto_ ())
          {
            os << "  BEGIN" << endl
               << "    EXECUTE IMMEDIATE 'DROP SEQUENCE " <<
              quote_id (table + "_seq") << "';" << endl
               << "  EXCEPTION" << endl
               << "    WHEN OTHERS THEN" << endl
               << "      IF SQLCODE != -2289 THEN RAISE; END IF;" << endl
               << "  END;" << endl;
          }

          os << "END;" << endl;
          post_statement ();
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
            fk.set ("oracle-fk-defined", true); // Mark it as defined.
          }
        }

        virtual string
        name (sema_rel::foreign_key& fk)
        {
          // In Oracle, foreign key names are schema-global. Make them
          // unique by prefixing the key name with table name. Note,
          // however, that they cannot have a schema.
          //
          return quote_id (
            static_cast<sema_rel::table&> (fk.scope ()).name ().uname ()
            + "_" + fk.name ());
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
          if (!fk.count ("oracle-fk-defined"))
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
          tables_.insert (t.name ()); // Add it before to cover self-refs.
          base::traverse (t);

          // Create the sequence if we have auto primary key.
          //
          using sema_rel::primary_key;

          sema_rel::table::names_iterator i (t.find ("")); // Special name.
          primary_key* pk (i != t.names_end ()
                           ? &dynamic_cast<primary_key&> (i->nameable ())
                           : 0);

          if (pk != 0 && pk->auto_ ())
          {
            pre_statement ();
            os_ << "CREATE SEQUENCE " << quote_id (t.name () + "_seq") << endl
                << "  START WITH 1 INCREMENT BY 1" << endl;
            post_statement ();
          }

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

        virtual string
        name (sema_rel::index& in)
        {
          // In Oracle, index names are database-global. Make them unique
          // by prefixing the index name with table name (preserving the
          // schema).
          //
          sema_rel::qname n (
            static_cast<sema_rel::table&> (in.scope ()).name ());

          n.uname () += "_" + in.name ();
          return quote_id (n);
        }
      };
      entry<create_index> create_index_;
    }
  }
}
