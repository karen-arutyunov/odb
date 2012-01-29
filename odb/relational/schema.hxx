// file      : odb/relational/schema.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
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

    struct schema_emitter: emitter, virtual context
    {
      typedef schema_emitter base;

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

    //
    // File prologue/epilogue.
    //

    struct schema_file: virtual context
    {
      typedef schema_file base;

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
    // Drop.
    //

    struct drop_table: trav_rel::table, common
    {
      typedef drop_table base;

      drop_table (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      virtual void
      drop (sema_rel::qname const& table)
      {
        os << "DROP TABLE IF EXISTS " << quote_id (table) << endl;
      }

      virtual void
      traverse (sema_rel::table& t)
      {
        // By default we do everything in a single pass. But some
        // databases may require the second pass.
        //
        if (pass_ > 1)
          return;

        pre_statement ();
        drop (t.name ());
        post_statement ();
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

    struct drop_index: trav_rel::index, common
    {
      typedef drop_index base;

      drop_index (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      virtual void
      drop (sema_rel::qname const& /*index*/)
      {
        // Most database systems drop indexes together with the table.
        //
        //os << "DROP INDEX IF EXISTS " << quote_id (index);
      }

      virtual void
      traverse (sema_rel::index& in)
      {
        // By default we do everything in a single pass. But some
        // databases may require the second pass.
        //
        if (pass_ > 1)
          return;

        pre_statement ();
        drop (in.name ());
        post_statement ();
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

    struct create_column: trav_rel::column, virtual context
    {
      typedef create_column base;

      create_column (schema_format f, create_table& ct)
          : format_ (f), create_table_ (ct), first_ (true)
      {
      }

      virtual void
      traverse (sema_rel::column& c)
      {
        if (first_)
          first_ = false;
        else
          os << "," << endl;

        create (c);
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

        os << "  " << quote_id (c.name ()) << " ";

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
        if (!c.null ())
          os << " NOT NULL";
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
      create_table& create_table_;
      bool first_;
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
      on_delete (sema_rel::foreign_key::action a)
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
        if (pass_ > 1)
          return;

        pre_statement ();
        create_pre (t.name ());

        instance<create_column> c (format_, *this);
        instance<create_primary_key> pk (format_, *this);
        instance<create_foreign_key> fk (format_, *this);
        trav_rel::unames n;

        n >> c;
        n >> pk;
        n >> fk;

        names (t, n);

        create_post ();
        post_statement ();
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

    struct create_index: trav_rel::index, common
    {
      typedef create_index base;

      create_index (emitter_type& e, ostream& os, schema_format f)
          : common (e, os), format_ (f)
      {
      }

      virtual void
      traverse (sema_rel::index& in)
      {
        // By default we do everything in a single pass. But some
        // databases may require the second pass.
        //
        if (pass_ > 1)
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
        return quote_id (in.table ().name ());
      }

      virtual void
      create (sema_rel::index& in)
      {
        using sema_rel::index;

        os << "CREATE INDEX " << name (in) << endl
           << "  ON " << table_name (in) << " (";

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
        }

        os << ")" << endl;
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
  }
}

#endif // ODB_RELATIONAL_SCHEMA_HXX
