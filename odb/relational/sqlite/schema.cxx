// file      : odb/relational/sqlite/schema.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
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
        traverse (sema_rel::add_column& ac)
        {
          using sema_rel::alter_table;
          alter_table& at (static_cast<alter_table&> (ac.scope ()));

          pre_statement ();

          os << "ALTER TABLE " << quote_id (at.name ()) << endl
             << "  ADD COLUMN ";
          create (ac);
          os << endl;

          post_statement ();
        }

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
        name (sema_rel::index& in)
        {
          // In SQLite, index names can be qualified with the database.
          //
          sema_rel::table& t (static_cast<sema_rel::table&> (in.scope ()));
          sema_rel::qname n (t.name ().qualifier ());
          n.append (in.name ());
          return quote_id (n);
        }

        virtual string
        table_name (sema_rel::index& in)
        {
          // In SQLite, the index table cannot be qualified with the
          // database name (it has to be in the same database).
          //
          return quote_id (
            static_cast<sema_rel::table&> (in.scope ()).name ().uname ());
        }
      };
      entry<create_index> create_index_;

      struct drop_index: relational::drop_index, context
      {
        drop_index (base const& x): base (x) {}

        virtual string
        name (sema_rel::index& in)
        {
          // In SQLite, index names can be qualified with the database.
          //
          sema_rel::table& t (static_cast<sema_rel::table&> (in.scope ()));
          sema_rel::qname n (t.name ().qualifier ());
          n.append (in.name ());
          return quote_id (n);
        }
      };
      entry<drop_index> drop_index_;

      struct alter_table_pre: relational::alter_table_pre, context
      {
        alter_table_pre (base const& x): base (x) {}

        virtual void
        alter (sema_rel::alter_table& at)
        {
          // SQLite can only add a single column per ALTER TABLE statement.
          //
          instance<create_column> c (emitter (), stream (), format_);
          trav_rel::unames n;
          n >> c;
          names (at, n);
        }
      };
      entry<alter_table_pre> alter_table_pre_;

      struct alter_table_post: relational::alter_table_post, context
      {
        alter_table_post (base const& x): base (x) {}

        virtual void
        alter (sema_rel::alter_table& at)
        {
          // SQLite does not support dropping columns.
          //
          if (sema_rel::drop_column* dc = check<sema_rel::drop_column> (at))
          {
            cerr << "error: SQLite does not support dropping of columns"
                 << endl;
            cerr << "info: first dropped column is '" << dc->name () <<
              "' in table '" << at.name () << "'" << endl;
            throw operation_failed ();
          }
        }
      };
      entry<alter_table_post> alter_table_post_;
    }
  }
}
