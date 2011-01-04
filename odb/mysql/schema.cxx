// file      : odb/mysql/schema.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <set>

#include <odb/mysql/common.hxx>
#include <odb/mysql/schema.hxx>

using namespace std;

namespace mysql
{
  namespace
  {
    typedef set<string> tables;

    struct object_columns: object_columns_base, context
    {
      object_columns (context& c, string const& prefix = string ())
          : object_columns_base (c), context (c), prefix_ (prefix)
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

        os << "  `" << name << "` " << column_type (m, prefix_);

        if (m.count ("id"))
          os << " PRIMARY KEY";

        using semantics::class_;
        if (class_* c = object_pointer (member_type (m, prefix_)))
        {
          os << " REFERENCES `" << table_name (*c) << "` (`" <<
            column_name (id_member (*c)) << "`)";
        }

        return true;
      }

    private:
      string prefix_;
    };

    struct member_create: object_members_base, context
    {
      member_create (context& c, semantics::class_& object, tables& t)
          : object_members_base (c, false, true),
            context (c),
            object_ (object),
            id_member_ (id_member (object)),
            tables_ (t)
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

        os << "CREATE TABLE `" << name << "` (" << endl;

        // object_id (simple value)
        //
        string id_name (column_name (m, "id", "object_id"));
        os << "  `" << id_name << "` " << column_type (id_member_, "ref");

        // index (simple value)
        //
        string index_name;
        bool ordered (ck == ck_ordered && !unordered (m));
        if (ordered)
        {
          index_name = column_name (m, "index", "index");

          os << "," << endl
             << "  `" << index_name << "` " << column_type (m, "index");
        }

        // key (simple or composite value)
        //
        if (ck == ck_map || ck == ck_multimap)
        {
          type& kt (container_kt (t));

          os << "," << endl;

          if (semantics::class_* ckt = comp_value (kt))
          {
            object_columns oc (*this);
            oc.traverse_composite (m, *ckt, "key", "key");
          }
          else
          {
            object_columns oc (*this, "key");
            string const& name (column_name (m, "key", "key"));
            oc.column (m, name, true);
          }
        }

        // value (simple or composite value)
        //
        {
          os << "," << endl;

          if (semantics::class_* cvt = comp_value (vt))
          {
            object_columns oc (*this);
            oc.traverse_composite (m, *cvt, "value", "value");
          }
          else
          {
            object_columns oc (*this, "value");
            string const& name (column_name (m, "value", "value"));
            oc.column (m, name, true);
          }
        }

        // object_id index
        //
        os << "," << endl
           << "  INDEX (`" << id_name << "`)";

        // index index
        //
        if (ordered)
          os << "," << endl
             << "  INDEX (`" << index_name << "`)";

        os << ")";

        string const& engine (options.mysql_engine ());

        if (engine != "default")
          os << endl
             << "  ENGINE=" << engine;

        os << ";" << endl
           << endl;

        tables_.insert (name);
      }

    private:
      semantics::class_& object_;
      semantics::data_member& id_member_;
      tables& tables_;
    };

    struct class_create: traversal::class_, context
    {
      class_create (context& c)
          : context (c)
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

        os << "CREATE TABLE `" << name << "` (" << endl;

        {
          object_columns oc (*this);
          oc.traverse (c);
        }

        os << ")";

        string const& engine (options.mysql_engine ());

        if (engine != "default")
          os << endl
             << "  ENGINE=" << engine;

        os << ";" << endl
           << endl;

        // Create tables for members.
        //
        {
          member_create mc (*this, c, tables_);
          mc.traverse (c);
        }

        tables_.insert (name);
      }

    private:
      tables tables_;
    };

    struct member_drop: object_members_base, context
    {
      member_drop (context& c, semantics::class_& object, tables& t)
          : object_members_base (c, false, true),
            context (c),
            object_ (object),
            tables_ (t)
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

        os << "DROP TABLE IF EXISTS `" << name << "`;" << endl;
        tables_.insert (name);
      }

    private:
      semantics::class_& object_;
      tables& tables_;
    };

    struct class_drop: traversal::class_, context
    {
      class_drop (context& c)
          : context (c)
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

        os << "DROP TABLE IF EXISTS `" << name << "`;" << endl;

        // Drop tables for members.
        //
        {
          member_drop mc (*this, c, tables_);
          mc.traverse (c);
        }

        tables_.insert (name);
      }

    private:
      tables tables_;
    };

    static char const file_header[] =
    "/* This file was generated by ODB, object-relational mapping (ORM)\n"
    " * compiler for C++.\n"
    " */\n\n";
  }

  void
  generate_schema (context& ctx)
  {
    ctx.os << file_header;

    // Drop.
    //
    {
      traversal::unit unit;
      traversal::defines unit_defines;
      traversal::namespace_ ns;
      class_drop c (ctx);

      unit >> unit_defines >> ns;
      unit_defines >> c;

      traversal::defines ns_defines;

      ns >> ns_defines >> ns;
      ns_defines >> c;
      unit.dispatch (ctx.unit);
    }

    ctx.os << endl;

    // Create.
    //
    {
      traversal::unit unit;
      traversal::defines unit_defines;
      traversal::namespace_ ns;
      class_create c (ctx);

      unit >> unit_defines >> ns;
      unit_defines >> c;

      traversal::defines ns_defines;

      ns >> ns_defines >> ns;
      ns_defines >> c;
      unit.dispatch (ctx.unit);
    }
  }
}
