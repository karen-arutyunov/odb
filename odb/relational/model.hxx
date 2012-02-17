// file      : odb/relational/model.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
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
    typedef std::set<qname> tables;

    struct object_columns: object_columns_base, virtual context
    {
      typedef object_columns base;

      object_columns (sema_rel::model& model, sema_rel::table& table)
          : model_ (model),
            table_ (table),
            pkey_ (0),
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
                semantics::type& t,
                string const& kp,
                string const& dn,
                semantics::class_* to = 0)
      {
        // This overrides the member name for a composite container value
        // or key.
        //
        if (!kp.empty ())
        {
          semantics::class_* c (object_pointer (t));
          if (composite_wrapper (c == 0 ? t : utype (*id_member (*c))))
          {
            id_prefix_ = kp + ".";
            id_override_ = true;
          }
        }

        object_columns_base::traverse (m, t, kp, dn, to);
      }

      using object_columns_base::traverse;

      virtual bool
      traverse_column (semantics::data_member& m, string const& name, bool)
      {
        bool id (object_columns_base::id ());             // Id or part of.
        bool null (!id && context::null (m, key_prefix_));

        string col_id (id_prefix_ +
                       (key_prefix_.empty () ? m.name () : key_prefix_));

        sema_rel::column& c (
          model_.new_node<sema_rel::column> (col_id, column_type (), null));

        c.set ("cxx-node", static_cast<semantics::node*> (&m));

        model_.new_edge<sema_rel::unames> (table_, c, name);

        // An id member cannot have a default value.
        //
        if (!id)
        {
          string const& d (default_ (m));

          if (!d.empty ())
            c.default_ (d);
        }

        // If we have options, add them.
        //
        string const& o (column_options (m, key_prefix_));

        if (!o.empty ())
          c.options (o);

        constraints (m, name, col_id, c);
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
        if (table_.is_a<sema_rel::object_table> ())
        {
          if (semantics::data_member* idm = id ())
          {
            if (pkey_ == 0)
            {
              pkey_ = &model_.new_node<sema_rel::primary_key> (
                m.count ("auto"));
              pkey_->set ("cxx-node", static_cast<semantics::node*> (idm));

              // In most databases the primary key constraint can be
              // manipulated without an explicit name. So we use the special
              // empty name for primary keys in order not to clash with
              // columns and other constraints. If the target database does
              // not support unnamed primary key manipulation, then the
              // database-specific code will have to come up with a suitable
              // name.
              //
              model_.new_edge<sema_rel::unames> (table_, *pkey_, "");
            }

            model_.new_edge<sema_rel::contains> (*pkey_, c);
          }
        }
      }

      virtual void
      traverse_pointer (semantics::data_member& m, semantics::class_& c)
      {
        using sema_rel::column;

        // Ignore inverse object pointers.
        //
        if (inverse (m, key_prefix_))
          return;

        string id (id_prefix_ +
                   (key_prefix_.empty () ? m.name () : key_prefix_));

        sema_rel::foreign_key& fk (
          model_.new_node<sema_rel::foreign_key> (
            id,
            table_name (c),
            true)); // deferred

        fk.set ("cxx-node", static_cast<semantics::node*> (&m));

        bool simple;

        // Get referenced columns.
        //
        {
          semantics::data_member& idm (*id_member (c));

          instance<object_columns_list> ocl;
          ocl->traverse (idm);

          for (object_columns_list::iterator i (ocl->begin ());
               i != ocl->end (); ++i)
            fk.referenced_columns ().push_back (i->name);

          simple = (fk.referenced_columns ().size () == 1);
        }

        // Get the position of the last column.
        //
        sema_rel::table::names_iterator i (table_.names_end ());

        while (i != table_.names_begin ())
        {
          --i;

          if (i->nameable ().is_a<column> ())
            break;
        }

        // Traverse the object pointer as columns.
        //
        object_columns_base::traverse_pointer (m, c);

        // Add the newly added columns to the foreign key.
        //
        if (i != table_.names_end ())
          ++i;
        else
          i = table_.names_begin ();

        for (; i != table_.names_end (); ++i)
        {
          if (column* c = dynamic_cast<column*> (&i->nameable ()))
            model_.new_edge<sema_rel::contains> (fk, *c);
          else
            break;
        }

        // Derive the constraint name. Generally, we want it to be based
        // on the column name. This is straightforward for single-column
        // references. In case of a composite id, we will need to use the
        // column prefix which is based on the data member name, unless
        // overridden by the user. In the latter case the prefix can be
        // empty, in which case we will just fall back on the member's
        // public name.
        //
        string name;

        if (simple)
          name = fk.contains_begin ()->column ().name ();
        else
        {
          string p (column_prefix (m, key_prefix_, default_name_));

          if (p.empty ())
            p = public_name_db (m);

          name = column_prefix_ + p;
        }

        // Add an underscore unless we already have one.
        //
        if (name[name.size () - 1] != '_')
          name += '_';

        model_.new_edge<sema_rel::unames> (table_, fk, name + "fk");
      }

    protected:
      string
      default_ (semantics::data_member&);

    protected:
      sema_rel::model& model_;
      sema_rel::table& table_;
      sema_rel::primary_key* pkey_;
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

        using sema_rel::index;
        using sema_rel::column;

        // Ignore inverse containers of object pointers.
        //
        if (inverse (m, "value"))
          return;

        container_kind_type ck (container_kind (ct));

        qname const& name (table_name (m, table_prefix_));

        // Add the [] decorator to distinguish this id from non-container
        // ids (we don't want to ever end up comparing, for example, an
        // object table to a container table).
        //
        string id (id_prefix_ + m.name () + "[]");

        sema_rel::container_table& t (
          model_.new_node<sema_rel::container_table> (id));
        t.set ("cxx-node", static_cast<semantics::node*> (&m));

        model_.new_edge<sema_rel::qnames> (model_, t, name);

        // object_id
        //
        {
          instance<object_columns> oc (model_, t);
          oc->traverse (m, container_idt (m), "id", "object_id");
        }

        // Foreign key and index for the object id.
        //
        {
          sema_rel::foreign_key& fk (
            model_.new_node<sema_rel::foreign_key> (
              id + ".id",
              table_name (*context::top_object),
              false, // immediate
              sema_rel::foreign_key::cascade));
          fk.set ("cxx-node", static_cast<semantics::node*> (&m));

          index& in (model_.new_node<index> (id + ".id"));
          in.set ("cxx-node", static_cast<semantics::node*> (&m));

          // Get referenced columns.
          //
          {
            data_member& idm (*id_member (*context::top_object));

            instance<object_columns_list> ocl;
            ocl->traverse (idm);

            for (object_columns_list::iterator i (ocl->begin ());
                 i != ocl->end (); ++i)
              fk.referenced_columns ().push_back (i->name);
          }

          // All the columns we have in this table so far are for the
          // object id. Add them to the foreign key and the index.
          //
          for (sema_rel::table::names_iterator i (t.names_begin ());
               i != t.names_end ();
               ++i)
          {
            column& c (dynamic_cast<column&> (i->nameable ()));

            model_.new_edge<sema_rel::contains> (fk, c);
            model_.new_edge<sema_rel::contains> (in, c);
          }

          // Derive the names. See the comment for the other foreign key
          // code above.
          //
          // Note also that id_name can be a column prefix (if id is
          // composite), in which case it can be empty and if not, then
          // it will most likely already contain a trailing underscore.
          //
          string id_name (column_name (m, "id", "object_id"));

          if (id_name.empty ())
            id_name = "object_id";

          if (id_name[id_name.size () - 1] != '_')
            id_name += '_';

          model_.new_edge<sema_rel::unames> (t, fk, id_name + "fk");

          model_.new_edge<sema_rel::qnames> (
            model_, in, name + "_" + id_name + "i");
        }

        // index (simple value)
        //
        bool ordered (ck == ck_ordered && !unordered (m));
        if (ordered)
        {
          instance<object_columns> oc (model_, t);
          oc->traverse (m, container_it (ct), "index", "index");

          string col_name (column_name (m, "index", "index"));

          index& in (model_.new_node<index> (id + ".index"));
          in.set ("cxx-node", static_cast<semantics::node*> (&m));

          model_.new_edge<sema_rel::contains> (
            in, dynamic_cast<column&> (t.find (col_name)->nameable ()));

          model_.new_edge<sema_rel::qnames> (
            model_, in, name + "_" + col_name + "_i");
        }

        // key
        //
        if (ck == ck_map || ck == ck_multimap)
        {
          instance<object_columns> oc (model_, t);
          oc->traverse (m, container_kt (ct), "key", "key");
        }

        // value
        //
        {
          instance<object_columns> oc (model_, t);
          oc->traverse (m, container_vt (ct), "value", "value");
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

        qname const& name (table_name (c));

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

        model_.new_edge<sema_rel::qnames> (model_, t, name);

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
