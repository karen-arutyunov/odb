// file      : odb/relational/mysql/schema.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

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
      using relational::table_set;

      //
      // Drop.
      //
      struct drop_foreign_key: relational::drop_foreign_key, context
      {
        drop_foreign_key (base const& x): base (x) {}

        virtual void
        drop (sema_rel::table& t, sema_rel::foreign_key& fk)
        {
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

          // So for now we only do this in migration.
          //
          if (dropped_ == 0)
          {
            if (fk.not_deferrable ())
              pre_statement ();
            else
            {
              if (format_ != schema_format::sql)
                return;

              os << "/*" << endl;
            }

            os << "ALTER TABLE " << quote_id (t.name ()) << endl
               << "  DROP FOREIGN KEY " << quote_id (fk.name ()) << endl;

            if (fk.not_deferrable ())
              post_statement ();
            else
              os << "*/" << endl
                 << endl;
          }
        }

        using base::drop;

        virtual void
        traverse (sema_rel::drop_foreign_key& dfk)
        {
          // Find the foreign key we are dropping in the base model.
          //
          sema_rel::foreign_key& fk (find<sema_rel::foreign_key> (dfk));

          if (fk.not_deferrable () || in_comment)
            base::traverse (dfk);
          else
          {
            if (format_ != schema_format::sql)
              return;

            os << endl
               << "  /*"
               << endl;

            drop (dfk);

            os << endl
               << "  */";
          }
        }

        virtual void
        drop_header ()
        {
          os << "DROP FOREIGN KEY ";
        }
      };
      entry<drop_foreign_key> drop_foreign_key_;

      //
      // Create.
      //

      struct create_column: relational::create_column, context
      {
        create_column (base const& x): base (x) {}

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
        generate (sema_rel::foreign_key& fk)
        {
          // MySQL does not support deferrable constraint checking. Output
          // such foreign keys as comments, for documentation, unless we
          // are generating embedded schema.
          //
          if (fk.not_deferrable ())
            base::generate (fk);
          else
          {
            // Don't bloat C++ code with comment strings if we are
            // generating embedded schema.
            //
            if (format_ != schema_format::sql)
              return;

            os << endl
               << "  /*" << endl
               << "  CONSTRAINT ";
            base::create (fk);
            os << endl
               << "  */";
          }
        }

        virtual void
        traverse (sema_rel::add_foreign_key& afk)
        {
          if (afk.not_deferrable () || in_comment)
            base::traverse (afk);
          else
          {
            if (format_ != schema_format::sql)
              return;

            os << endl
               << "  /*"
               << endl;

            add (afk);

            os << endl
               << "  */";
          }
        }

        virtual void
        deferrable (sema_rel::deferrable)
        {
          // This will still be called to output the comment.
        }
      };
      entry<create_foreign_key> create_foreign_key_;

      struct add_foreign_key: relational::add_foreign_key, context
      {
        add_foreign_key (base const& x): base (x) {}

        virtual void
        generate (sema_rel::table& t, sema_rel::foreign_key& fk)
        {
          // MySQL has no deferrable constraints.
          //
          if (fk.not_deferrable ())
            base::generate (t, fk);
          else
          {
            if (format_ != schema_format::sql)
              return;

            os << "/*" << endl;
            os << "ALTER TABLE " << quote_id (t.name ()) << endl
               << "  ADD CONSTRAINT ";
            def_->create (fk);
            os << endl
               << "*/" << endl
               << endl;
          }
        }
      };
      entry<add_foreign_key> add_foreign_key_;

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

      struct alter_table_pre: relational::alter_table_pre, context
      {
        alter_table_pre (base const& x): base (x) {}

        // Check if we are only dropping deferrable foreign keys.
        //
        bool
        check_drop_deferrable_only (sema_rel::alter_table& at)
        {
          if (check<sema_rel::add_column> (at) ||
              check_alter_column_null (at, true))
            return false;

          for (sema_rel::alter_table::names_iterator i (at.names_begin ());
               i != at.names_end (); ++i)
          {
            using sema_rel::foreign_key;
            using sema_rel::drop_foreign_key;

            if (drop_foreign_key* dfk =
                dynamic_cast<drop_foreign_key*> (&i->nameable ()))
            {
              foreign_key& fk (find<foreign_key> (*dfk));

              if (fk.not_deferrable ())
                return false;
            }
          }
          return true;
        }

        virtual void
        alter (sema_rel::alter_table& at)
        {
          if (check_drop_deferrable_only (at))
          {
            if (format_ != schema_format::sql)
              return;

            os << "/*" << endl;
            in_comment = true;

            alter_header (at.name ());
            instance<drop_foreign_key> dfk (*this);
            trav_rel::unames n (*dfk);
            names (at, n);
            os << endl;

            in_comment = false;
            os << "*/" << endl
               << endl;
          }
          else
            base::alter (at);
        }
      };
      entry<alter_table_pre> alter_table_pre_;

      struct alter_table_post: relational::alter_table_post, context
      {
        alter_table_post (base const& x): base (x) {}

        // Check if we are only adding deferrable foreign keys.
        //
        bool
        check_add_deferrable_only (sema_rel::alter_table& at)
        {
          if (check<sema_rel::drop_column> (at) ||
              check_alter_column_null (at, false))
            return false;

          for (sema_rel::alter_table::names_iterator i (at.names_begin ());
               i != at.names_end (); ++i)
          {
            using sema_rel::add_foreign_key;

            if (add_foreign_key* afk =
                dynamic_cast<add_foreign_key*> (&i->nameable ()))
            {
              if (afk->not_deferrable ())
                return false;
            }
          }
          return true;
        }

        virtual void
        alter (sema_rel::alter_table& at)
        {
          if (check_add_deferrable_only (at))
          {
            if (format_ != schema_format::sql)
              return;

            os << "/*" << endl;
            in_comment = true;

            alter_header (at.name ());
            instance<create_foreign_key> cfk (*this);
            trav_rel::unames n (*cfk);
            names (at, n);
            os << endl;

            in_comment = false;
            os << "*/" << endl
               << endl;
          }
          else
            base::alter (at);
        }
      };
      entry<alter_table_post> alter_table_post_;
    }
  }
}
