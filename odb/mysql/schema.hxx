// file      : odb/mysql/schema.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_MYSQL_SCHEMA_HXX
#define ODB_MYSQL_SCHEMA_HXX

#include <set>

#include <odb/emitter.hxx>
#include <odb/mysql/common.hxx>

namespace mysql
{
  struct schema_context: context
  {
    typedef std::set<string> tables;

    schema_context (context& c, std::ostream& os, emitter& e)
        : context (c, os), e_ (e)
    {
    }

    schema_context (schema_context& c) : context (c), e_ (c.e_) {}

    emitter& e_;
  };

  //
  // Drop.
  //

  struct member_drop: object_members_base, schema_context
  {
    member_drop (schema_context& c, tables& t)
        : object_members_base (c, false, true),
          schema_context (c),
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

      e_.pre ();
      os << "DROP TABLE IF EXISTS `" << name << "`" << endl;
      e_.post ();

      tables_.insert (name);
    }

  private:
    tables& tables_;
  };

  struct class_drop: traversal::class_, schema_context
  {
    class_drop (context& c, emitter& e)
        : schema_context (c, os, e), os (e),
          member_drop_ (*this, tables_)
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

      e_.pre ();
      os << "DROP TABLE IF EXISTS `" << name << "`" << endl;
      e_.post ();

      tables_.insert (name);

      // Drop tables for members.
      //
      member_drop_.traverse (c);
    }

  private:
    tables tables_;
    emitter_ostream os;
    member_drop member_drop_;
  };

  //
  // Create.
  //

  struct object_columns: object_columns_base, schema_context
  {
    object_columns (schema_context& c, string const& prefix = string ())
        : object_columns_base (c), schema_context (c), prefix_ (prefix)
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

  struct member_create: object_members_base, schema_context
  {
    member_create (schema_context& c, semantics::class_& object, tables& t)
        : object_members_base (c, false, true),
          schema_context (c),
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

      e_.pre ();
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
           << " ENGINE=" << engine;

      os << endl;
      e_.post ();

      tables_.insert (name);
    }

  private:
    semantics::data_member& id_member_;
    tables& tables_;
  };

  struct class_create: traversal::class_, schema_context
  {
    class_create (context& c, emitter& e)
        : schema_context (c, os, e), os (e)
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

      e_.pre ();
      os << "CREATE TABLE `" << name << "` (" << endl;

      {
        object_columns oc (*this);
        oc.traverse (c);
      }

      os << ")";

      string const& engine (options.mysql_engine ());

      if (engine != "default")
        os << endl
           << " ENGINE=" << engine;

      os << endl;
      e_.post ();

      tables_.insert (name);

      // Create tables for members.
      //
      {
        member_create mc (*this, c, tables_);
        mc.traverse (c);
      }
    }

  private:
    tables tables_;
    emitter_ostream os;
  };
}

#endif // ODB_MYSQL_SCHEMA_HXX
