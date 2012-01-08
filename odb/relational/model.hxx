// file      : odb/relational/model.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_MODEL_HXX
#define ODB_RELATIONAL_MODEL_HXX

#include <set>
#include <cassert>
#include <sstream>

#include <odb/emitter.hxx>

#include <odb/semantics/relational.hxx>

#include <odb/relational/common.hxx>
#include <odb/relational/context.hxx>

namespace relational
{
  namespace model
  {
    typedef std::set<std::string> tables;

    struct object_columns: object_columns_base, virtual context
    {
      typedef object_columns base;

      object_columns (sema_rel::model& model,
                      sema_rel::table& table,
                      string const& prefix = string ())
          : model_ (model),
            table_ (table),
            prefix_ (prefix),
            id_override_ (false)
      {
      }

      virtual void
      traverse_object (semantics::class_& c)
      {
        if (context::top_object != &c)
        {
          // We are in one of the bases. Set the id_prefix to its
          // (unqualified) name.
          //
          string t (id_prefix_);
          id_prefix_ = class_name (c) + "::";
          object_columns_base::traverse_object (c);
          id_prefix_ = t;
        }
        else
          object_columns_base::traverse_object (c);
      }

      virtual void
      traverse_composite (semantics::data_member* m, semantics::class_& c)
      {
        string t (id_prefix_);

        if (m != 0)
          // Member of a composite type. Add the data member to id_prefix.
          //
          if (!id_override_)
            id_prefix_ += m->name () + ".";
          else
            id_override_ = false;
        else
          // Composite base. Add its unqualified name to id_prefix.
          //
          id_prefix_ += class_name (c) + "::";

        object_columns_base::traverse_composite (m, c);

        id_prefix_ = t;
      }

      virtual void
      traverse (semantics::data_member& m,
                semantics::class_& c,
                std::string const& kp,
                std::string const& dn)
      {
        // This overrides the member name for a composite container value
        // or key.
        //
        if (!kp.empty ())
        {
          id_prefix_ = kp + ".";
          id_override_ = true;
        }

        object_columns_base::traverse (m, c, kp, dn);
      }

      using object_columns_base::traverse;

      virtual bool
      traverse_column (semantics::data_member& m, string const& name, bool)
      {
        // Ignore inverse object pointers.
        //
        if (inverse (m))
          return false;

        string id (id_prefix_ + (prefix_.empty () ? m.name () : prefix_));

        sema_rel::column& c (
          model_.new_node<sema_rel::column> (
            id, column_type (m, prefix_), context::null (m, prefix_)));
        c.set ("cxx-node", static_cast<semantics::node*> (&m));

        model_.new_edge<sema_rel::names> (table_, c, name);

        // An id member cannot have a default value.
        //
        if (!context::id (m))
        {
          string const& d (default_ (m));

          if (!d.empty ())
            c.default_ (d);
        }

        // If we have options, add them.
        //
        string const& o (column_options (m, prefix_));

        if (!o.empty ())
          c.options (o);

        constraints (m, name, id, c);
        reference (m, name, id, c);

        return true;
      }

      virtual string
      default_null (semantics::data_member&)
      {
        return "NULL";
      }

      virtual string
      default_bool (semantics::data_member&, bool v)
      {
        // Most databases do not support boolean literals. Those that
        // do should override this.
        //
        return (v ? "1" : "0");
      }

      virtual string
      default_integer (semantics::data_member&, unsigned long long v, bool neg)
      {
        std::ostringstream ostr;
        ostr << (neg ? "-" : "") << v;
        return ostr.str ();
      }

      virtual string
      default_float (semantics::data_member&, double v)
      {
        std::ostringstream ostr;
        ostr << v;
        return ostr.str ();
      }

      virtual string
      default_string (semantics::data_member&, string const& v)
      {
        return quote_string (v);
      }

      virtual string
      default_enum (semantics::data_member&,
                    tree /*enumerator*/,
                    string const& /*name*/)
      {
        // Has to be implemented by the database-specific override.
        //
        assert (false);
      }

      virtual void
      constraints (semantics::data_member& m,
                   string const& /* name */,
                   string const& /* id */,
                   sema_rel::column& c)
      {
        if (!id (m))
          return;

        sema_rel::primary_key& pk (
          model_.new_node<sema_rel::primary_key> (m.count ("auto")));
        pk.set ("cxx-node", static_cast<semantics::node*> (&m));

        model_.new_edge<sema_rel::contains> (pk, c);

        // In most databases the primary key constraint can be manipulated
        // without an explicit name. So we use the special empty name for
        // primary keys in order not to clash with columns and other
        // constraints. If the target database does not support unnamed
        // primary key manipulation, then the database-specific code will
        // have to come up with a suitable name.
        //
        model_.new_edge<sema_rel::names> (table_, pk, "");
      }

      virtual void
      reference (semantics::data_member& m,
                 string const& name,
                 string const& id,
                 sema_rel::column& c)
      {
        semantics::class_* p (object_pointer (member_utype (m, prefix_)));

        if (p == 0)
          return;

        sema_rel::foreign_key& fk (
          model_.new_node<sema_rel::foreign_key> (
            id,
            table_name (*p),
            true)); // deferred

        fk.set ("cxx-node", static_cast<semantics::node*> (&m));

        fk.referenced_columns ().push_back (column_name (*id_member (*p)));

        model_.new_edge<sema_rel::contains> (fk, c);

        // Derive the constraint name. Generally, we want it to be based
        // on the column name. This is straightforward for single column
        // references. In case of the composite ids, we will need to use
        // the column prefix which is based on the data member name,
        // unless overridden (see how the column pragma works for members
        // of composite value types). @@ This is a TODO. Perhaps use the
        // up-to-and-including composite member prefix? Though it can be
        // empty.
        //
        model_.new_edge<sema_rel::names> (table_, fk, name + "_fk");
      }

    protected:
      string
      default_ (semantics::data_member&);

    protected:
      sema_rel::model& model_;
      sema_rel::table& table_;
      string prefix_;
      string id_prefix_;
      bool id_override_;
    };

    struct member_create: object_members_base, virtual context
    {
      typedef member_create base;

      member_create (sema_rel::model& model)
          : object_members_base (false, true, false), model_ (model)
      {
      }

      virtual void
      traverse_object (semantics::class_& c)
      {
        if (context::top_object != &c)
        {
          // We are in one of the bases. Set the id_prefix to its
          // (unqualified) name.
          //
          string t (id_prefix_);
          id_prefix_ = class_name (c) + "::";
          object_members_base::traverse_object (c);
          id_prefix_ = t;
        }
        else
        {
          // Top-level object. Set its id as a prefix.
          //
          id_prefix_ = string (class_fq_name (c), 2) + "::";
          object_members_base::traverse_object (c);
        }
      }

      virtual void
      traverse_composite (semantics::data_member* m, semantics::class_& c)
      {
        string t (id_prefix_);

        if (m != 0)
          // Member of a composite type. Add the data member to id_prefix.
          //
          id_prefix_ += m->name () + ".";
        else
          // Composite base. Add its unqualified name to id_prefix.
          //
          id_prefix_ += class_name (c) + "::";

        object_members_base::traverse_composite (m, c);

        id_prefix_ = t;
      }

      virtual void
      traverse_container (semantics::data_member& m, semantics::type& ct)
      {
        using semantics::type;
        using semantics::data_member;

        // Ignore inverse containers of object pointers.
        //
        if (inverse (m, "value"))
          return;

        container_kind_type ck (container_kind (ct));
        type& vt (container_vt (ct));

        string const& name (table_name (m, table_prefix_));

        // Add the [] decorator to distinguish this id from non-container
        // ids (we don't want to ever end up comparing, for example, an
        // object table to a container table).
        //
        string id (id_prefix_ + m.name () + "[]");

        sema_rel::container_table& t (
          model_.new_node<sema_rel::container_table> (id));
        t.set ("cxx-node", static_cast<semantics::node*> (&m));

        model_.new_edge<sema_rel::names> (model_, t, name);

        // object_id (simple value, for now)
        //
        string id_name (column_name (m, "id", "object_id"));
        {
          instance<object_columns> oc (model_, t, "id");
          oc->traverse_column (m, id_name, true);
        }

        // Foreign key for the object id.
        //
        {
          sema_rel::foreign_key& fk (
            model_.new_node<sema_rel::foreign_key> (
              id + ".id",
              table_name (*context::top_object),
              false, // immediate
              sema_rel::foreign_key::cascade));

          fk.set ("cxx-node", static_cast<semantics::node*> (&m));

          fk.referenced_columns ().push_back (
            column_name (
              *id_member (*context::top_object)));

          // All the columns we have in this table so far are for the
          // object id.
          //
          for (sema_rel::table::names_iterator i (t.names_begin ());
               i != t.names_end ();
               ++i)
            model_.new_edge<sema_rel::contains> (
              fk, dynamic_cast<sema_rel::column&> (i->nameable ()));

          // Derive the constraint name. See the comment for the other
          // foreign key code above.
          //
          model_.new_edge<sema_rel::names> (t, fk, id_name + "_fk");
        }

        // index (simple value)
        //
        string index_name;
        bool ordered (ck == ck_ordered && !unordered (m));
        if (ordered)
        {
          instance<object_columns> oc (model_, t, "index");
          index_name = column_name (m, "index", "index");
          oc->traverse_column (m, index_name, true);
        }

        // key (simple or composite value)
        //
        if (ck == ck_map || ck == ck_multimap)
        {
          type& kt (container_kt (ct));

          if (semantics::class_* ckt = composite_wrapper (kt))
          {
            instance<object_columns> oc (model_, t);
            oc->traverse (m, *ckt, "key", "key");
          }
          else
          {
            instance<object_columns> oc (model_, t, "key");
            string const& name (column_name (m, "key", "key"));
            oc->traverse_column (m, name, true);
          }
        }

        // value (simple or composite value)
        //
        {
          if (semantics::class_* cvt = composite_wrapper (vt))
          {
            instance<object_columns> oc (model_, t);
            oc->traverse (m, *cvt, "value", "value");
          }
          else
          {
            instance<object_columns> oc (model_, t, "value");
            string const& name (column_name (m, "value", "value"));
            oc->traverse_column (m, name, true);
          }
        }

        // Create indexes.
        //
        using sema_rel::index;
        using sema_rel::column;

        {
          index& i (model_.new_node<index> (id + ".id"));
          i.set ("cxx-node", static_cast<semantics::node*> (&m));

          model_.new_edge<sema_rel::contains> (
            i, dynamic_cast<column&> (t.find (id_name)->nameable ()));

          //@@ Once id can be composite, we need to revise this (see
          //   a comment for the foreign key generation above).
          //
          model_.new_edge<sema_rel::names> (
            model_, i, name + '_' + id_name + "_i");
        }

        if (ordered)
        {
          index& i (model_.new_node<index> (id + ".index"));
          i.set ("cxx-node", static_cast<semantics::node*> (&m));

          model_.new_edge<sema_rel::contains> (
            i, dynamic_cast<column&> (t.find (index_name)->nameable ()));

          // This is always a single column (simple value).
          //
          model_.new_edge<sema_rel::names> (
            model_, i, name + '_' + index_name + "_i");
        }
      }

    protected:
      sema_rel::model& model_;
      string id_prefix_;
    };

    struct class_: traversal::class_, virtual context
    {
      typedef class_ base;

      class_ (sema_rel::model& model)
          : model_ (model)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (class_file (c) != unit.file ())
          return;

        if (!object (c) || abstract (c))
          return;

        string const& name (table_name (c));

        // If the table with this name was already created, assume the
        // user knows what they are doing and skip it.
        //
        if (tables_.count (name))
        {
          c.set ("model-range-first", model_.names_end ());
          c.set ("model-range-last", model_.names_end ());
          return;
        }

        string id (class_fq_name (c), 2); // Remove leading '::'.

        sema_rel::object_table& t(
          model_.new_node<sema_rel::object_table> (id));

        t.set ("cxx-node", static_cast<semantics::node*> (&c));

        model_.new_edge<sema_rel::names> (model_, t, name);

        sema_rel::model::names_iterator begin (--model_.names_end ());

        {
          instance<object_columns> oc (model_, t);
          oc->traverse (c);
        }

        tables_.insert (name);

        // Create tables for members.
        //
        {
          instance<member_create> mc (model_);
          mc->traverse (c);
        }

        sema_rel::model::names_iterator end (--model_.names_end ());

        c.set ("model-range-first", begin);
        c.set ("model-range-last", end);
      }

    protected:
      sema_rel::model& model_;
      tables tables_;
    };
  }
}

#endif // ODB_RELATIONAL_MODEL_HXX
