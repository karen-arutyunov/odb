// file      : odb/relational/schema.hxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_SCHEMA_HXX
#define ODB_RELATIONAL_SCHEMA_HXX

#include <set>
#include <vector>
#include <cassert>

#include <odb/emitter.hxx>
#include <odb/relational/common.hxx>
#include <odb/relational/context.hxx>

namespace relational
{
  namespace schema
  {
    struct common: virtual context
    {
      typedef ::emitter emitter_type;

      common (emitter_type& e, ostream& os): e_ (e), os_ (os) {}

      void
      pre_statement ()
      {
        e_.pre ();
        diverge (os_);
      }

      void
      post_statement ()
      {
        restore ();
        e_.post ();
      }

      emitter_type&
      emitter () const
      {
        return e_;
      }

      ostream&
      stream () const
      {
        return os_;
      }

    protected:
      emitter_type& e_;
      ostream& os_;
    };

    //
    // Drop.
    //

    // Only used in migration.
    //
    struct drop_column: trav_rel::drop_column, common
    {
      typedef drop_column base;

      drop_column (emitter_type& e,
                   ostream& os,
                   schema_format f,
                   bool* first = 0)
          : common (e, os),
            format_ (f),
            first_ (first != 0 ? *first : first_data_),
            first_data_ (true)
      {
      }

      drop_column (drop_column const& c)
          : root_context (), // @@ -Wextra
            context (),
            common (c),
            format_ (c.format_),
            first_ (&c.first_ != &c.first_data_ ? c.first_ : first_data_),
            first_data_ (c.first_data_)
      {
      }

      virtual void
      drop_header ()
      {
        // By default ADD COLUMN though some databases use just ADD.
        //
        os << "DROP COLUMN ";
      }

      virtual void
      traverse (sema_rel::drop_column& dc)
      {
        if (first_)
          first_ = false;
        else
          os << "," << endl;

        os << "  ";
        drop_header ();
        os << quote_id (dc.name ());
      }

    protected:
      schema_format format_;
      bool& first_;
      bool first_data_;
    };

    // Currently only used in migration.
    //
    struct drop_index: trav_rel::drop_index, common
    {
      typedef drop_index base;

      enum index_type {unique, non_unique, all};

      drop_index (emitter_type& e,
                  ostream& os,
                  schema_format f,
                  index_type t = all)
          : common (e, os), format_ (f), type_ (t)
      {
      }

      virtual void
      traverse (sema_rel::drop_index& di)
      {
        using sema_rel::model;
        using sema_rel::changeset;
        using sema_rel::table;
        using sema_rel::alter_table;
        using sema_rel::index;

        // Find the index we are dropping in the base model.
        //
        alter_table& at (dynamic_cast<alter_table&> (di.scope ()));
        changeset& cs (dynamic_cast<changeset&> (at.scope ()));
        model& bm (cs.base_model ());
        table* bt (bm.find<table> (at.name ()));
        assert (bt != 0);
        index* bi (bt->find<index> (di.name ()));
        assert (bi != 0);
        traverse (*bi);
      }

      virtual void
      traverse (sema_rel::index& in)
      {
        if (type_ == unique &&
            in.type ().find ("UNIQUE") == string::npos &&
            in.type ().find ("unique") == string::npos)
          return;

        if (type_ == non_unique && (
              in.type ().find ("UNIQUE") != string::npos ||
              in.type ().find ("unique") != string::npos))
          return;

        pre_statement ();
        drop (in);
        post_statement ();
      }

      virtual string
      name (sema_rel::index& in)
      {
        return quote_id (in.name ());
      }

      virtual void
      drop (sema_rel::index& in)
      {
        os << "DROP INDEX " << name (in) << endl;
      }

    protected:
      schema_format format_;
      index_type type_;
    };

    struct drop_table: trav_rel::table,
                       trav_rel::drop_table,
                       common
    {
      typedef drop_table base;

      drop_table (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      virtual void
      drop (sema_rel::qname const& table, bool migration)
      {
        os << "DROP TABLE " << (migration ? "" : "IF EXISTS ") <<
          quote_id (table) << endl;
      }

      virtual void
      traverse (sema_rel::table& t, bool migration)
      {
        // By default we do everything in a single, last pass. But some
        // databases may require two passes.
        //
        if (pass_ != 2)
          return;

        pre_statement ();
        drop (t.name (), migration);
        post_statement ();
      }

      virtual void
      traverse (sema_rel::table& t)
      {
        traverse (t, false);
      }

      virtual void
      traverse (sema_rel::drop_table& dt)
      {
        using sema_rel::model;
        using sema_rel::changeset;
        using sema_rel::table;

        // Find the table we are dropping in the base model.
        //
        changeset& cs (dynamic_cast<changeset&> (dt.scope ()));
        model& bm (cs.base_model ());
        table* t (bm.find<table> (dt.name ()));
        assert (t != 0);
        traverse (*t, true);
      }

      using table::names;

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

    protected:
      schema_format format_;
      unsigned short pass_;
    };

    struct drop_model: trav_rel::model, common
    {
      typedef drop_model base;

      drop_model (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      // This version is only called for file schema.
      //
      virtual void
      traverse (sema_rel::model& m)
      {
        traverse (m.names_begin (), m.names_end ());
      }

      virtual void
      traverse (sema_rel::model::names_iterator begin,
                sema_rel::model::names_iterator end)
      {
        // Traverse named entities in the reverse order. This way we
        // drop them in the order opposite to creating.
        //
        if (begin != end)
        {
          for (--end;; --end)
          {
            dispatch (*end);

            if (begin == end)
              break;
          }
        }
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

    protected:
      schema_format format_;
      unsigned short pass_;
    };

    //
    // Create.
    //
    struct create_table;

    struct create_column: trav_rel::column,
                          trav_rel::add_column,
                          common
    {
      typedef create_column base;

      create_column (emitter_type& e,
                     ostream& os,
                     schema_format f,
                     bool* first = 0)
          : common (e, os),
            format_ (f),
            first_ (first != 0 ? *first : first_data_),
            first_data_ (true)
      {
      }

      create_column (create_column const& c)
          : root_context (), // @@ -Wextra
            context (),
            common (c),
            format_ (c.format_),
            first_ (&c.first_ != &c.first_data_ ? c.first_ : first_data_),
            first_data_ (c.first_data_)
      {
      }

      virtual void
      traverse (sema_rel::column& c)
      {
        if (first_)
          first_ = false;
        else
          os << "," << endl;

        os << "  ";
        create (c);
      }

      virtual void
      add_header ()
      {
        // By default ADD COLUMN though some databases use just ADD.
        //
        os << "ADD COLUMN ";
      }

      virtual void
      traverse (sema_rel::add_column& ac)
      {
        if (first_)
          first_ = false;
        else
          os << "," << endl;

        os << "  ";
        add_header ();
        create (ac);
      }

      virtual void
      create (sema_rel::column& c)
      {
        using sema_rel::column;

        // See if this column is (part of) a primary key.
        //
        sema_rel::primary_key* pk (0);

        for (column::contained_iterator i (c.contained_begin ());
             i != c.contained_end ();
             ++i)
        {
          if ((pk = dynamic_cast<sema_rel::primary_key*> (&i->key ())))
            break;
        }

        os << quote_id (c.name ()) << " ";

        type (c, pk != 0 && pk->auto_ ());

        if (!c.default_ ().empty ())
          os << " DEFAULT " << c.default_ ();

        null (c);

        // If this is a single-column primary key, generate it inline.
        //
        if (pk != 0 && pk->contains_size () == 1)
          primary_key ();

        if (pk != 0 && pk->auto_ ())
          auto_ (c);

        if (!c.options ().empty ())
          os << " " << c.options ();
      }

      virtual void
      type (sema_rel::column& c, bool /*auto*/)
      {
        os << c.type ();
      }

      virtual void
      null (sema_rel::column& c)
      {
        // Specify both cases explicitly for better readability,
        // especially in ALTER COLUMN clauses.
        //
        os << (c.null () ? " NULL" : " NOT NULL");
      }

      virtual void
      primary_key ()
      {
        os << " PRIMARY KEY";
      }

      virtual void
      auto_ (sema_rel::column&)
      {
      }

    protected:
      schema_format format_;
      bool& first_;
      bool first_data_;
    };

    struct create_primary_key: trav_rel::primary_key, virtual context
    {
      typedef create_primary_key base;

      create_primary_key (schema_format f, create_table& ct)
          : format_ (f), create_table_ (ct)
      {
      }

      virtual void
      traverse (sema_rel::primary_key& pk)
      {
        // Single-column primary keys are generated inline in the
        // column declaration.
        //
        if (pk.contains_size () == 1)
          return;

        // We will always follow a column.
        //
        os << "," << endl
           << endl;

        create (pk);
      }

      virtual void
      create (sema_rel::primary_key& pk)
      {
        using sema_rel::primary_key;

        // By default we create unnamed primary key constraint.
        //

        os << "  PRIMARY KEY (";

        for (primary_key::contains_iterator i (pk.contains_begin ());
             i != pk.contains_end ();
             ++i)
        {
          if (pk.contains_size () > 1)
          {
            if (i != pk.contains_begin ())
              os << ",";

            os << endl
               << "    ";
          }

          os << quote_id (i->column ().name ());
        }

        os << ")";
      }

    protected:
      schema_format format_;
      create_table& create_table_;
    };

    struct create_foreign_key: trav_rel::foreign_key, virtual context
    {
      typedef create_foreign_key base;

      create_foreign_key (schema_format f, create_table& ct)
          : format_ (f), create_table_ (ct)
      {
      }

      virtual void
      traverse (sema_rel::foreign_key& fk)
      {
        // We will always follow a column or another key.
        //
        os << "," << endl
           << endl;

        create (fk);
      }

      virtual void
      create (sema_rel::foreign_key& fk)
      {
        using sema_rel::foreign_key;

        os << "  CONSTRAINT " << name (fk) << endl
           << "    FOREIGN KEY (";

        for (foreign_key::contains_iterator i (fk.contains_begin ());
             i != fk.contains_end ();
             ++i)
        {
          if (fk.contains_size () > 1)
          {
            if (i != fk.contains_begin ())
              os << ",";

            os << endl
               << "      ";
          }

          os << quote_id (i->column ().name ());
        }

        os << ")" << endl
           << "    REFERENCES " << table_name (fk) << " (";

        foreign_key::columns const& refs (fk.referenced_columns ());

        for (foreign_key::columns::const_iterator i (refs.begin ());
             i != refs.end ();
             ++i)
        {
          if (refs.size () > 1)
          {
            if (i != refs.begin ())
              os << ",";

            os << endl
               << "      ";
          }

          os << quote_id (*i);
        }

        os << ")";

        if (fk.on_delete () != foreign_key::no_action)
          on_delete (fk.on_delete ());

        if (fk.deferred ())
          deferred ();
      }

      virtual string
      name (sema_rel::foreign_key& fk)
      {
        return quote_id (fk.name ());
      }

      virtual string
      table_name (sema_rel::foreign_key& fk)
      {
        return quote_id (fk.referenced_table ());
      }

      virtual void
      on_delete (sema_rel::foreign_key::action_type a)
      {
        using sema_rel::foreign_key;

        switch (a)
        {
        case foreign_key::cascade:
          {
            os << endl
               << "    ON DELETE CASCADE";
            break;
          }
        case foreign_key::no_action:
          break;
        }
      }

      virtual void
      deferred ()
      {
        os << endl
           << "    DEFERRABLE INITIALLY DEFERRED";
      }

    protected:
      schema_format format_;
      create_table& create_table_;
    };

    struct create_index: trav_rel::index, common
    {
      typedef create_index base;

      enum index_type {unique, non_unique, all};

      create_index (emitter_type& e,
                    ostream& os,
                    schema_format f,
                    index_type t = all)
          : common (e, os), format_ (f), type_ (t)
      {
      }

      virtual void
      traverse (sema_rel::index& in)
      {
        if (type_ == unique &&
            in.type ().find ("UNIQUE") == string::npos &&
            in.type ().find ("unique") == string::npos)
          return;

        if (type_ == non_unique && (
              in.type ().find ("UNIQUE") != string::npos ||
              in.type ().find ("unique") != string::npos))
          return;

        pre_statement ();
        create (in);
        post_statement ();
      }

      virtual string
      name (sema_rel::index& in)
      {
        return quote_id (in.name ());
      }

      virtual string
      table_name (sema_rel::index& in)
      {
        return quote_id (static_cast<sema_rel::table&> (in.scope ()).name ());
      }

      virtual void
      columns (sema_rel::index& in)
      {
        using sema_rel::index;

        for (index::contains_iterator i (in.contains_begin ());
             i != in.contains_end ();
             ++i)
        {
          if (in.contains_size () > 1)
          {
            if (i != in.contains_begin ())
              os << ",";

            os << endl
               << "    ";
          }

          os << quote_id (i->column ().name ());

          if (!i->options ().empty ())
            os << ' ' << i->options ();
        }
      }

      virtual void
      create (sema_rel::index& in)
      {
        // Default implementation that ignores the method.
        //
        os << "CREATE ";

        if (!in.type ().empty ())
          os << in.type () << ' ';

        os << "INDEX " << name (in) << endl
           << "  ON " << table_name (in) << " (";

        columns (in);

        os << ")" << endl;

        if (!in.options ().empty ())
          os << ' ' << in.options () << endl;
      }

    protected:
      schema_format format_;
      index_type type_;
    };

    struct create_table: trav_rel::table, common
    {
      typedef create_table base;

      create_table (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      virtual void
      create_pre (sema_rel::qname const& table)
      {
        os << "CREATE TABLE " << quote_id (table) << " (" << endl;
      }

      virtual void
      create_post ()
      {
        os << ")" << endl;
      }

      virtual void
      traverse (sema_rel::table& t)
      {
        // By default we do everything in a single pass. But some
        // databases may require the second pass.
        //
        if (pass_ != 1)
          return;

        pre_statement ();
        create_pre (t.name ());

        instance<create_column> c (emitter (), stream (), format_);
        instance<create_primary_key> pk (format_, *this);
        instance<create_foreign_key> fk (format_, *this);
        trav_rel::unames n;

        n >> c;
        n >> pk;
        n >> fk;

        names (t, n);

        create_post ();
        post_statement ();

        // Create indexes.
        //
        {
          instance<create_index> in (emitter (), stream (), format_);
          trav_rel::unames n (*in);
          names (t, n);
        }
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

    protected:
      schema_format format_;
      unsigned short pass_;
    };

    struct create_model: trav_rel::model, common
    {
      typedef create_model base;

      create_model (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      // This version is only called for file schema.
      //
      virtual void
      traverse (sema_rel::model& m)
      {
        traverse (m.names_begin (), m.names_end ());
      }

      virtual void
      traverse (sema_rel::model::names_iterator begin,
                sema_rel::model::names_iterator end)
      {
        for (; begin != end; ++begin)
          dispatch (*begin);
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

    protected:
      schema_format format_;
      unsigned short pass_;
    };

    //
    // Migration.
    //

    struct alter_column: trav_rel::alter_column, common
    {
      typedef alter_column base;

      alter_column (emitter_type& e,
                    ostream& os,
                    schema_format f,
                    bool pre,
                    bool* first = 0)
          : common (e, os),
            format_ (f),
            pre_ (pre),
            first_ (first != 0 ? *first : first_data_),
            first_data_ (true),
            def_ (e, os, f)
      {
      }

      alter_column (alter_column const& c)
          : root_context (), // @@ -Wextra
            context (),
            common (c),
            format_ (c.format_),
            pre_ (c.pre_),
            first_ (&c.first_ != &c.first_data_ ? c.first_ : first_data_),
            first_data_ (c.first_data_),
            def_ (common::e_, common::os_, c.format_)
      {
      }

      virtual void
      alter_header ()
      {
        os << "ALTER COLUMN ";
      }

      virtual void
      alter (sema_rel::alter_column& ac)
      {
        // By default use the whole definition.
        //
        def_->create (ac);
      }

      virtual void
      traverse (sema_rel::alter_column& ac)
      {
        assert (ac.null_altered ());

        // Relax (NULL) in pre and tighten (NOT NULL) in post.
        //
        if (pre_ != ac.null ())
          return;

        if (first_)
          first_ = false;
        else
          os << "," << endl;

        os << "  ";
        alter_header ();
        alter (ac);
      }

    protected:
      schema_format format_;
      bool pre_;
      bool& first_;
      bool first_data_;
      instance<create_column> def_;
    };

    struct alter_table_common: trav_rel::alter_table, common
    {
      alter_table_common (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      template <typename T>
      T*
      check (sema_rel::alter_table& at)
      {
        for (sema_rel::alter_table::names_iterator i (at.names_begin ());
             i != at.names_end (); ++i)
        {
          if (T* x = dynamic_cast<T*> (&i->nameable ()))
            return x;
        }

        return 0;
      }

      sema_rel::alter_column*
      check_alter_null (sema_rel::alter_table& at, bool v)
      {
        using sema_rel::alter_column;

        for (sema_rel::alter_table::names_iterator i (at.names_begin ());
             i != at.names_end (); ++i)
        {
          if (alter_column* ac = dynamic_cast<alter_column*> (&i->nameable ()))
          {
            if (ac->null_altered () && ac->null () == v)
              return ac;
          }
        }

        return 0;
      }

      virtual void
      alter_header (sema_rel::qname const& table)
      {
        os << "ALTER TABLE " << quote_id (table) << endl;
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

    protected:
      schema_format format_;
      unsigned short pass_;
    };

    struct alter_table_pre: alter_table_common
    {
      typedef alter_table_pre base;

      alter_table_pre (emitter_type& e, ostream& os, schema_format f)
          : alter_table_common (e, os, f)
      {
      }

      // Check if there will be any clauses in ALTER TABLE.
      //
      using alter_table_common::check;

      virtual bool
      check (sema_rel::alter_table& at)
      {
        return check<sema_rel::add_column> (at) ||
          check_alter_null (at, true);
      }

      virtual void
      alter (sema_rel::alter_table& at)
      {
        // By default we generate all the alterations in a single ALTER TABLE
        // statement. Quite a few databases don't support this.
        //
        pre_statement ();
        alter_header (at.name ());

        bool f (true);  // Shared first flag.
        bool* pf (&f);  // (Im)perfect forwarding.
        bool tl (true); // (Im)perfect forwarding.
        instance<create_column> cc (emitter (), stream (), format_, pf);
        instance<alter_column> ac (emitter (), stream (), format_, tl, pf);
        trav_rel::unames n;
        n >> cc;
        n >> ac;
        names (at, n);
        os << endl;

        post_statement ();
      }

      virtual void
      traverse (sema_rel::alter_table& at)
      {
        if (pass_ == 1)
        {
          // Drop unique indexes.
          //
          {
            drop_index::index_type it (drop_index::unique);
            instance<drop_index> in (emitter (), stream (), format_, it);
            trav_rel::unames n (*in);
            names (at, n);
          }

          if (check (at))
            alter (at);
        }
        else
        {
          // Add non-unique indexes.
          //
          {
            create_index::index_type it (create_index::non_unique);
            instance<create_index> in (emitter (), stream (), format_, it);
            trav_rel::unames n (*in);
            names (at, n);
          }
        }
      }
    };

    struct changeset_pre: trav_rel::changeset, common
    {
      typedef changeset_pre base;

      changeset_pre (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      // This version is only called for file migration.
      //
      virtual void
      traverse (sema_rel::changeset& m)
      {
        traverse (m.names_begin (), m.names_end ());
      }

      virtual void
      traverse (sema_rel::changeset::names_iterator begin,
                sema_rel::changeset::names_iterator end)
      {
        for (; begin != end; ++begin)
          dispatch (*begin);
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

    protected:
      schema_format format_;
      unsigned short pass_;
    };

    struct alter_table_post: alter_table_common
    {
      typedef alter_table_post base;

      alter_table_post (emitter_type& e, ostream& os, schema_format f)
          : alter_table_common (e, os, f)
      {
      }

      // Check if there will be any clauses in ALTER TABLE.
      //
      using alter_table_common::check;

      virtual bool
      check (sema_rel::alter_table& at)
      {
        return check<sema_rel::drop_column> (at) ||
          check_alter_null (at, false);
      }

      virtual void
      alter (sema_rel::alter_table& at)
      {
        // By default we generate all the alterations in a single ALTER TABLE
        // statement. Quite a few databases don't support this.
        //
        pre_statement ();
        alter_header (at.name ());

        bool f (true); // Shared first flag.
        bool* pf (&f); // (Im)perfect forwarding.
        bool fl (false); // (Im)perfect forwarding.
        instance<drop_column> dc (emitter (), stream (), format_, pf);
        instance<alter_column> ac (emitter (), stream (), format_, fl, pf);
        trav_rel::unames n;
        n >> dc;
        n >> ac;
        names (at, n);
        os << endl;

        post_statement ();
      }

      virtual void
      traverse (sema_rel::alter_table& at)
      {
        if (pass_ == 1)
        {
          // Drop non-unique indexes.
          //
          {
            drop_index::index_type it (drop_index::non_unique);
            instance<drop_index> in (emitter (), stream (), format_, it);
            trav_rel::unames n (*in);
            names (at, n);
          }
        }
        else
        {
          if (check (at))
            alter (at);

          // Add unique indexes.
          //
          {
            create_index::index_type it (create_index::unique);
            instance<create_index> in (emitter (), stream (), format_, it);
            trav_rel::unames n (*in);
            names (at, n);
          }
        }
      }
    };

    struct changeset_post: trav_rel::changeset, common
    {
      typedef changeset_post base;

      changeset_post (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      // This version is only called for file migration.
      //
      virtual void
      traverse (sema_rel::changeset& m)
      {
        traverse (m.names_begin (), m.names_end ());
      }

      virtual void
      traverse (sema_rel::changeset::names_iterator begin,
                sema_rel::changeset::names_iterator end)
      {
        for (; begin != end; ++begin)
          dispatch (*begin);
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

    protected:
      schema_format format_;
      unsigned short pass_;
    };

    //
    // SQL output.
    //

    struct sql_emitter: emitter, virtual context
    {
      typedef sql_emitter base;

      virtual void
      pre ()
      {
        first_ = true;
      }

      virtual void
      line (const std::string& l)
      {
        if (first_ && !l.empty ())
          first_ = false;
        else
          os << endl;

        os << l;
      }

      virtual void
      post ()
      {
        if (!first_) // Ignore empty statements.
          os << ';' << endl
             << endl;
      }

    protected:
      bool first_;
    };

    struct sql_file: virtual context
    {
      typedef sql_file base;

      virtual void
      prologue ()
      {
      }

      virtual void
      epilogue ()
      {
      }
    };

    //
    // C++ output.
    //

    struct cxx_emitter: emitter, virtual context
    {
      typedef cxx_emitter base;

      void
      pass (unsigned short p)
      {
        empty_ = true;
        pass_ = p;
        new_pass_ = true;

        if (pass_ == 1)
          empty_passes_ = 0; // New set of passes.

        // Assume this pass is empty.
        //
        empty_passes_++;
      }

      // Did this pass produce anything?
      //
      bool
      empty () const
      {
        return empty_;
      }

      virtual void
      pre ()
      {
        first_ = true;
      }

      virtual void
      line (const string& l)
      {
        if (l.empty ())
          return; // Ignore empty lines.

        if (first_)
        {
          first_ = false;

          // If this line starts a new pass, then output the switch/case
          // blocks.
          //
          if (new_pass_)
          {
            new_pass_ = false;
            empty_ = false;
            empty_passes_--; // This pass is not empty.

            // Output case statements for empty preceeding passes, if any.
            //
            if (empty_passes_ != 0)
            {
              unsigned short s (pass_ - empty_passes_);

              if (s == 1)
                os << "switch (pass)"
                   << "{";
              else
                os << "return true;" // One more pass.
                   << "}";

              for (; s != pass_; ++s)
                os << "case " << s << ":" << endl;

              os << "{";
              empty_passes_ = 0;
            }

            if (pass_ == 1)
              os << "switch (pass)"
                 << "{";
            else
              os << "return true;" // One more pass.
                 << "}";

            os << "case " << pass_ << ":" << endl
               << "{";
          }

          os << "db.execute (";
        }
        else
          os << strlit (line_ + '\n') << endl;

        line_ = l;
      }

      virtual void
      post ()
      {
        if (!first_) // Ignore empty statements.
          os << strlit (line_) << ");";
      }

    private:
      std::string line_;
      bool first_;
      bool empty_;
      bool new_pass_;
      unsigned short pass_;
      unsigned short empty_passes_; // Number of preceding empty passes.
    };

    struct cxx_object: virtual context
    {
      typedef cxx_object base;

      //@@ (im)-perfect forwarding.
      //
      static schema_format format_embedded;

      cxx_object ()
          : stream_ (*emitter_),
            drop_model_ (*emitter_, stream_, format_embedded),
            drop_table_ (*emitter_, stream_, format_embedded),
            create_model_ (*emitter_, stream_, format_embedded),
            create_table_ (*emitter_, stream_, format_embedded)
      {
        init ();
      }

      cxx_object (cxx_object const&)
          : root_context (), //@@ -Wextra
            context (),
            stream_ (*emitter_),
            drop_model_ (*emitter_, stream_, format_embedded),
            drop_table_ (*emitter_, stream_, format_embedded),
            create_model_ (*emitter_, stream_, format_embedded),
            create_table_ (*emitter_, stream_, format_embedded)
      {
        init ();
      }

      void
      init ()
      {
        drop_model_ >> drop_names_;
        drop_names_ >> drop_table_;

        create_model_ >> create_names_;
        create_names_ >> create_table_;
      }

      void
      traverse (semantics::class_& c)
      {
        typedef sema_rel::model::names_iterator iterator;

        iterator begin (c.get<iterator> ("model-range-first"));
        iterator end (c.get<iterator> ("model-range-last"));

        if (begin == model->names_end ())
          return; // This class doesn't have any model entities (e.g.,
                  // a second class mapped to the same table).

        ++end; // Transform the range from [begin, end] to [begin, end).

        string const& type (class_fq_name (c));
        string traits ("access::object_traits_impl< " + type + ", id_" +
                       db.string () + " >");

        // create_schema ()
        //
        os << "bool " << traits << "::" << endl
           << "create_schema (database& db, unsigned short pass, bool drop)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << "ODB_POTENTIALLY_UNUSED (pass);"
           << "ODB_POTENTIALLY_UNUSED (drop);"
           << endl;

        // Drop.
        //
        if (!options.omit_drop ())
        {
          bool close (false);

          os << "if (drop)"
             << "{";

          for (unsigned short pass (1); pass < 3; ++pass)
          {
            emitter_->pass (pass);
            drop_model_->pass (pass);
            drop_table_->pass (pass);

            drop_model_->traverse (begin, end);

            close = close || !emitter_->empty ();
          }

          if (close) // Close the last case and the switch block.
            os << "return false;"
               << "}"  // case
               << "}";  // switch

          os << "}";
        }

        // Create.
        //
        if (!options.omit_create ())
        {
          bool close (false);

          if (options.omit_drop ())
            os << "if (!drop)";
          else
            os << "else";

          os << "{";

          for (unsigned short pass (1); pass < 3; ++pass)
          {
            emitter_->pass (pass);
            create_model_->pass (pass);
            create_table_->pass (pass);

            create_model_->traverse (begin, end);

            close = close || !emitter_->empty ();
          }

          if (close) // Close the last case and the switch block.
            os << "return false;"
               << "}"  // case
               << "}"; // switch

          os << "}";
        }

        os << "return false;"
           << "}";

        os << "static const schema_catalog_entry" << endl
           << "schema_catalog_entry_" << flat_name (type) << "_ (" << endl
           << "id_" << db << "," << endl
           << strlit (options.schema_name ()[db]) << "," << endl
           << "&" << traits << "::create_schema);"
           << endl;
      }

    private:
      instance<cxx_emitter> emitter_;
      emitter_ostream stream_;

      trav_rel::qnames drop_names_;
      instance<drop_model> drop_model_;
      instance<drop_table> drop_table_;

      trav_rel::qnames create_names_;
      instance<create_model> create_model_;
      instance<create_table> create_table_;
    };
  }
}

#endif // ODB_RELATIONAL_SCHEMA_HXX
