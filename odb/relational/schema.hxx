// file      : odb/relational/schema.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
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
        if (first_)
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
    // File.
    //

    struct schema_file: virtual context
    {
      typedef schema_file base;

      virtual void
      pre ()
      {
      }

      virtual void
      post ()
      {
      }
    };

    //
    // Drop.
    //

    struct drop_common: virtual context
    {
      virtual void
      drop_table (string const& table)
      {
        os << "DROP TABLE IF EXISTS " << quote_id (table) << endl;
      }

      virtual void
      drop_index (string const& /*table*/, string const& /*column*/)
      {
        // Most database systems drop indexes together with the table.
        //

        //os << "DROP INDEX IF EXISTS " << quote_id (table + '_' + column)
        //   << endl;
      }
    };

    struct member_drop: object_members_base, common, virtual drop_common
    {
      typedef member_drop base;

      member_drop (emitter& e, ostream& os, std::vector<tables>& t)
          : object_members_base (false, true, false),
            common (e, os),
            tables_ (t)
      {
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

      virtual void
      traverse_container (semantics::data_member& m, semantics::type& c)
      {
        // Ignore inverse containers of object pointers.
        //
        if (inverse (m, "value"))
          return;

        string const& name (table_name (m, table_prefix_));

        if (tables_[pass_].count (name))
          return;

        // Drop table.
        //
        pre_statement ();
        drop_table (name);
        post_statement ();

        tables_[pass_].insert (name);

        // Drop indexes.
        //
        pre_statement ();
        drop_index (name, column_name (m, "id", "object_id"));
        post_statement ();

        if (container_kind (c) == ck_ordered && !unordered (m))
        {
          pre_statement ();
          drop_index (name, column_name (m, "index", "index"));
          post_statement ();
        }
      }

    protected:
      std::vector<tables>& tables_;
      unsigned short pass_;
    };

    struct class_drop: traversal::class_, common, virtual drop_common
    {
      typedef class_drop base;

      class_drop (emitter& e)
          : common (e, os_), os_ (e), member_drop_ (e, os_, tables_)
      {
        tables_.push_back (tables ()); // Dummy entry.
      }

      class_drop (class_drop const& x)
          : root_context (), //@@ -Wextra
            context (),
            common (x.e_, os_), os_ (x.e_), member_drop_ (x.e_, os_, tables_)
      {
        tables_.push_back (tables ()); // Dummy entry.
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;

        if (tables_.size () == pass_)
          tables_.push_back (tables ());

        member_drop_->pass (p);
      }

      virtual void
      traverse (type& c)
      {
        // By default we do everything in a single pass. But some
        // databases may require the second pass.
        //
        if (pass_ == 1)
          drop (c);
      }

      virtual void
      drop (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (!object (c) || abstract (c))
          return;

        string const& name (table_name (c));

        if (tables_[pass_].count (name))
          return;

        // Drop tables for members. Do it before dropping the primary
        // table -- some databases may prefer it that way.
        //
        member_drop_->traverse (c);

        pre_statement ();
        drop_table (name);
        post_statement ();

        tables_[pass_].insert (name);
      }

    protected:
      emitter_ostream os_;
      unsigned short pass_;
      std::vector<tables> tables_; // Seperate table for each pass.
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
      traverse_column (semantics::data_member& m,
                       string const& name,
                       bool first)
      {
        // Ignore inverse object pointers.
        //
        if (inverse (m))
          return false;

        if (!first)
          os << "," << endl;

        os << "  " << quote_id (name) << " ";

        type (m);
        null (m);

        // An id member cannot have a default value.
        //
        if (!m.count ("id"))
          default_ (m);

        // If we have options, add them.
        //
        string const& o (column_options (m, prefix_));

        if (!o.empty ())
          os << " " << o;

        constraints (m);
        reference (m);

        return true;
      }

      virtual void
      type (semantics::data_member& m)
      {
        os << column_type (m, prefix_);
      }

      virtual void
      null (semantics::data_member& m)
      {
        if (!context::null (m, prefix_))
          os << " NOT NULL";
      }

      virtual void
      default_null (semantics::data_member&)
      {
        os << " DEFAULT NULL";
      }

      virtual void
      default_bool (semantics::data_member&, bool v)
      {
        // Most databases do not support boolean literals. Those that
        // do should override this.
        //
        os << " DEFAULT " << (v ? "1" : "0");
      }

      virtual void
      default_integer (semantics::data_member&, unsigned long long v, bool neg)
      {
        os << " DEFAULT " << (neg ? "-" : "") << v;
      }

      virtual void
      default_float (semantics::data_member&, double v)
      {
        os << " DEFAULT " << v;
      }

      virtual void
      default_string (semantics::data_member&, string const& v)
      {
        os << " DEFAULT " << quote_string (v);
      }

      virtual void
      default_enum (semantics::data_member&,
                    tree /*enumerator*/,
                    string const& /*name*/)
      {
        // Has to be implemented by the database-specific override.
        //
        assert (false);
      }

      virtual void
      constraints (semantics::data_member& m)
      {
        if (m.count ("id"))
          os << " PRIMARY KEY";
      }

      virtual void
      reference (semantics::data_member& m)
      {
        if (semantics::class_* c = object_pointer (member_utype (m, prefix_)))
        {
          os << " REFERENCES " << table_qname (*c) << " (" <<
            column_qname (*id_member (*c)) << ")";
        }
        else if (prefix_ == "id")
        {
          // Container id column references the object table. It also
          // cascades on delete so that we can delete the object with
          // a single delete statement (needed for erase_query()).
          //
          semantics::class_& c (*context::top_object);

          os << " REFERENCES " << table_qname (c) << " (" <<
            column_qname (*id_member (c)) << ") ON DELETE CASCADE";
        }
      }

    protected:
      void
      default_ (semantics::data_member&);

    protected:
      string prefix_;
    };

    struct create_common: virtual context
    {
      virtual void
      create_table_pre (string const& table)
      {
        os << "CREATE TABLE " << quote_id (table) << " (" << endl;
      }

      virtual void
      create_table_post ()
      {
        os << ")" << endl;
      }

      virtual void
      create_index (string const& table, string const& column)
      {
        os << "CREATE INDEX " << quote_id (table + '_' + column) << endl
           << "  ON " << quote_id (table) << " (" << quote_id (column) << ")"
           << endl;
      }
    };

    struct member_create: object_members_base, common, virtual create_common
    {
      typedef member_create base;

      member_create (emitter& e, ostream& os, std::vector<tables>& t)
          : object_members_base (false, true, false),
            common (e, os),
            tables_ (t)
      {
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;
      }

      virtual void
      traverse_container (semantics::data_member& m, semantics::type& t)
      {
        using semantics::type;
        using semantics::data_member;

        // Ignore inverse containers of object pointers.
        //
        if (inverse (m, "value"))
          return;

        container_kind_type ck (container_kind (t));
        type& vt (container_vt (t));

        string const& name (table_name (m, table_prefix_));

        if (tables_[pass_].count (name))
          return;

        pre_statement ();
        create_table_pre (name);

        // object_id (simple value)
        //
        string id_name (column_name (m, "id", "object_id"));
        {
          instance<object_columns> oc ("id");
          oc->traverse_column (m, id_name, true);
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
          oc->traverse_column (m, index_name, true);
        }

        // key (simple or composite value)
        //
        if (ck == ck_map || ck == ck_multimap)
        {
          type& kt (container_kt (t));

          os << "," << endl;

          if (semantics::class_* ckt = composite_wrapper (kt))
          {
            instance<object_columns> oc;
            oc->traverse (m, *ckt, "key", "key");
          }
          else
          {
            instance<object_columns> oc ("key");
            string const& name (column_name (m, "key", "key"));
            oc->traverse_column (m, name, true);
          }
        }

        // value (simple or composite value)
        //
        {
          os << "," << endl;

          if (semantics::class_* cvt = composite_wrapper (vt))
          {
            instance<object_columns> oc;
            oc->traverse (m, *cvt, "value", "value");
          }
          else
          {
            instance<object_columns> oc ("value");
            string const& name (column_name (m, "value", "value"));
            oc->traverse_column (m, name, true);
          }
        }

        create_table_post ();
        post_statement ();

        tables_[pass_].insert (name);

        // Create indexes.
        //
        pre_statement ();
        create_index (name, id_name);
        post_statement ();

        if (ordered)
        {
          pre_statement ();
          create_index (name, index_name);
          post_statement ();
        }
      }

    protected:
      std::vector<tables>& tables_;
      unsigned short pass_;
    };

    struct class_create: traversal::class_, common, virtual create_common
    {
      typedef class_create base;

      class_create (emitter& e)
          : common (e, os_), os_ (e), member_create_ (e, os_, tables_)
      {
        tables_.push_back (tables ()); // Dummy entry.
      }

      class_create (class_create const& x)
          : root_context (), //@@ -Wextra
            context (),
            common (x.e_, os_),
            os_ (x.e_),
            member_create_ (x.e_, os_, tables_)
      {
        tables_.push_back (tables ()); // Dummy entry.
      }

      void
      pass (unsigned short p)
      {
        pass_ = p;

        if (tables_.size () == pass_)
          tables_.push_back (tables ());

        member_create_->pass (p);
      }

      virtual void
      traverse (type& c)
      {
        // By default we do everything in a single pass. But some
        // databases may require the second pass.
        //
        if (pass_ > 1)
          return;

        if (c.file () != unit.file ())
          return;

        if (!object (c) || abstract (c))
          return;

        string const& name (table_name (c));

        // If the table with this name was already created, assume the
        // user knows what they are doing and skip it.
        //
        if (tables_[pass_].count (name))
          return;

        pre_statement ();
        create_table_pre (name);

        {
          instance<object_columns> oc;
          oc->traverse (c);
        }

        create_table_post ();
        post_statement ();

        tables_[pass_].insert (name);

        // Create tables for members.
        //
        member_create_->traverse (c);
      }

    protected:
      emitter_ostream os_;
      unsigned short pass_;
      std::vector<tables> tables_; // Seperate table for each pass.
      instance<member_create> member_create_;
    };
  }
}

#endif // ODB_RELATIONAL_SCHEMA_HXX
