// file      : odb/relational/schema.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_SCHEMA_HXX
#define ODB_RELATIONAL_SCHEMA_HXX

#include <set>
#include <cassert>

#include <odb/emitter.hxx>

#include <odb/relational/common.hxx>
#include <odb/relational/context.hxx>

namespace relational
{
  namespace schema
  {
    typedef std::set<std::string> tables;

    struct common: virtual context
    {
      common (emitter& e, ostream& os): e_ (e), os_ (os) {}

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

    protected:
      emitter& e_;
      ostream& os_;
    };

    //
    // Drop.
    //

    struct drop_common: virtual context
    {
      virtual void
      drop (string const& table)
      {
        os << "DROP TABLE IF EXISTS " << quote_id (table) << endl;
      }
    };

    struct member_drop: object_members_base, common, virtual drop_common
    {
      typedef member_drop base;

      member_drop (emitter& e, ostream& os, tables& t)
          : object_members_base (false, true), common (e, os), tables_ (t)
      {
      }

      virtual void
      container (semantics::data_member& m)
      {
        // Ignore inverse containers of object pointers.
        //
        if (inverse (m, "value"))
          return;

        string const& name (table_name (m, table_prefix_));

        if (tables_.count (name))
          return;

        pre_statement ();
        drop (name);
        post_statement ();

        tables_.insert (name);
      }

    protected:
      tables& tables_;
    };

    struct class_drop: traversal::class_, common, virtual drop_common
    {
      typedef class_drop base;

      class_drop (emitter& e)
          : common (e, os_), os_ (e), member_drop_ (e, os_, tables_)
      {
      }

      class_drop (class_drop const& x)
          : root_context (), //@@ -Wextra
            context (),
            common (x.e_, os_), os_ (x.e_), member_drop_ (x.e_, os_, tables_)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (!c.count ("object"))
          return;

        string const& name (table_name (c));

        if (tables_.count (name))
          return;

        pre_statement ();
        drop (name);
        post_statement ();

        tables_.insert (name);

        // Drop tables for members.
        //
        member_drop_->traverse (c);
      }

    protected:
      tables tables_;
      emitter_ostream os_;
      instance<member_drop> member_drop_;
    };

    //
    // Create.
    //

    struct object_columns: object_columns_base, virtual context
    {
      typedef object_columns base;

      object_columns (string const& prefix = string ())
          : prefix_ (prefix)
      {
      }

      virtual bool
      column (semantics::data_member& m, string const& name, bool first)
      {
        // Ignore inverse object pointers.
        //
        if (inverse (m))
          return false;

        if (!first)
          os << "," << endl;

        os << "  " << quote_id (name) << " " << column_type (m, prefix_);

        constraints (m);

        if (semantics::class_* c = object_pointer (member_type (m, prefix_)))
        {
          os << " REFERENCES " << quote_id (table_name (*c)) << " (" <<
            quote_id (column_name (id_member (*c))) << ")";
        }

        return true;
      }

      virtual void
      constraints (semantics::data_member& m)
      {
        if (m.count ("id"))
          os << " PRIMARY KEY";
      }

    protected:
      string prefix_;
    };

    struct create_common: virtual context
    {
      virtual void
      create_pre (string const& table)
      {
        os << "CREATE TABLE " << quote_id (table) << " (" << endl;
      }

      virtual void
      index (string const& column)
      {
        os << "INDEX (" << quote_id (column) << ")";
      }

      virtual void
      create_post ()
      {
        os << ")" << endl;
      }
    };


    struct member_create: object_members_base, common, virtual create_common
    {
      typedef member_create base;

      member_create (emitter& e, ostream& os, tables& t)
          : object_members_base (false, true), common (e, os), tables_ (t)
      {
      }

      virtual void
      container (semantics::data_member& m)
      {
        using semantics::type;
        using semantics::data_member;

        // Ignore inverse containers of object pointers.
        //
        if (inverse (m, "value"))
          return;

        type& t (m.type ());
        container_kind_type ck (container_kind (t));
        type& vt (container_vt (t));

        string const& name (table_name (m, table_prefix_));

        if (tables_.count (name))
          return;

        pre_statement ();
        create_pre (name);

        // object_id (simple value)
        //
        string id_name (column_name (m, "id", "object_id"));
        {
          instance<object_columns> oc ("id");
          oc->column (m, id_name, true);
        }

        // index (simple value)
        //
        string index_name;
        bool ordered (ck == ck_ordered && !unordered (m));
        if (ordered)
        {
          os << "," << endl;

          instance<object_columns> oc ("index");
          index_name = column_name (m, "index", "index");
          oc->column (m, index_name, true);
        }

        // key (simple or composite value)
        //
        if (ck == ck_map || ck == ck_multimap)
        {
          type& kt (container_kt (t));

          os << "," << endl;

          if (semantics::class_* ckt = comp_value (kt))
          {
            instance<object_columns> oc;
            oc->traverse_composite (m, *ckt, "key", "key");
          }
          else
          {
            instance<object_columns> oc ("key");
            string const& name (column_name (m, "key", "key"));
            oc->column (m, name, true);
          }
        }

        // value (simple or composite value)
        //
        {
          os << "," << endl;

          if (semantics::class_* cvt = comp_value (vt))
          {
            instance<object_columns> oc;
            oc->traverse_composite (m, *cvt, "value", "value");
          }
          else
          {
            instance<object_columns> oc ("value");
            string const& name (column_name (m, "value", "value"));
            oc->column (m, name, true);
          }
        }

        // object_id index
        //
        os << "," << endl
           << "  ";
        index (id_name);

        // index index
        //
        if (ordered)
        {
          os << "," << endl
             << "  ";
          index (index_name);
        }

        create_post ();
        post_statement ();

        tables_.insert (name);
      }

    protected:
      tables& tables_;
    };

    struct class_create: traversal::class_, common, virtual create_common
    {
      typedef class_create base;

      class_create (emitter& e)
          : common (e, os_), os_ (e), member_create_ (e, os_, tables_)
      {
      }

      class_create (class_create const& x)
          : root_context (), //@@ -Wextra
            context (),
            common (x.e_, os_), os_ (x.e_), member_create_ (x.e_, os_, tables_)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (!c.count ("object"))
          return;

        string const& name (table_name (c));

        // If the table with this name was already created, assume the
        // user knows what they are doing and skip it.
        //
        if (tables_.count (name))
          return;

        pre_statement ();
        create_pre (name);

        {
          instance<object_columns> oc;
          oc->traverse (c);
        }

        create_post ();
        post_statement ();

        tables_.insert (name);

        // Create tables for members.
        //
        member_create_->traverse (c);
      }

    protected:
      tables tables_;
      emitter_ostream os_;
      instance<member_create> member_create_;
    };
  }
}

#endif // ODB_RELATIONAL_SCHEMA_HXX
