// file      : odb/relational/changelog.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/diagnostics.hxx>

#include <odb/semantics/relational.hxx>
#include <odb/traversal/relational.hxx>

#include <odb/relational/context.hxx>
#include <odb/relational/generate.hxx>

using namespace std;

namespace relational
{
  namespace changelog
  {
    using namespace sema_rel;
    using sema_rel::model;
    using sema_rel::changelog;

    namespace
    {
      //
      // diff
      //

      struct diff_table: trav_rel::column,
                         trav_rel::primary_key,
                         trav_rel::foreign_key,
                         trav_rel::index
      {
        enum mode_type {mode_add, mode_drop};

        diff_table (table& o, mode_type m, alter_table& a, graph& gr)
            : other (o), mode (m), at (a), g (gr) {}

        virtual void
        traverse (sema_rel::column& c)
        {
          using sema_rel::column;

          if (mode == mode_add)
          {
            if (column* oc = other.find<column> (c.name ()))
            {
              if (c.type () != oc->type ())
                diagnose_column (c, "type", oc->type (), c.type ());

              if (c.null () != oc->null ())
              {
                alter_column& ac (g.new_node<alter_column> (c.id ()));

                // Set the alters edge.
                //
                column* b (at.lookup<column, drop_column> (c.name ()));
                assert (b != 0);
                g.new_edge<alters> (ac, *b);

                ac.null (c.null ());
                g.new_edge<unames> (at, ac, c.name ());
              }

              if (c.default_ () != oc->default_ ())
                diagnose_column (
                  c, "default value", oc->default_ (), c.default_ ());

              if (c.options () != oc->options ())
                diagnose_column (c, "options", oc->options (), c.options ());
            }
            else
            {
              add_column& ac (g.new_node<add_column> (c, at, g));
              g.new_edge<unames> (at, ac, c.name ());
            }
          }
          else
          {
            if (other.find<column> (c.name ()) == 0)
            {
              drop_column& dc (g.new_node<drop_column> (c.id ()));
              g.new_edge<unames> (at, dc, c.name ());
            }
          }
        }

        virtual void
        traverse (sema_rel::primary_key& pk)
        {
          using sema_rel::primary_key;

          if (mode == mode_add)
          {
            if (primary_key* opk = other.find<primary_key> (pk.name ()))
            {
              if (pk.auto_ () != opk->auto_ ())
                diagnose_primary_key (pk, "auto kind");

              if (pk.contains_size () != opk->contains_size ())
                diagnose_primary_key (pk, "member set");

              for (primary_key::contains_size_type i (0);
                   i != pk.contains_size (); ++i)
              {
                sema_rel::contains& c (pk.contains_at (i));
                sema_rel::contains& oc (opk->contains_at (i));

                if (c.column ().name () != oc.column ().name ())
                  diagnose_primary_key (pk, "member set");
              }
            }
            else
            {
              location const& l (pk.get<location> ("cxx-location"));
              error (l) << "adding object id to an existing class is " <<
                "not supported" << endl;
              info (l) << "consider re-implementing this change by adding " <<
                "a new class with the object id, migrating the data, and " <<
                "deleteing the old class" << endl;
              throw operation_failed ();
            }
          }
          else
          {
            if (other.find<primary_key> (pk.name ()) == 0)
            {
              location const& l (other.get<location> ("cxx-location"));
              error (l) << "deleting object id from an existing class is " <<
                "not supported" << endl;
              info (l) << "consider re-implementing this change by adding " <<
                "a new class without the object id, migrating the data, " <<
                "and deleteing the old class" << endl;
              throw operation_failed ();
            }
          }
        }

        virtual void
        traverse (sema_rel::foreign_key& fk)
        {
          using sema_rel::foreign_key;

          if (mode == mode_add)
          {
            if (foreign_key* ofk = other.find<foreign_key> (fk.name ()))
            {
              if (fk.deferred () != ofk->deferred ())
                diagnose_foreign_key (fk, "deferred kind");

              if (fk.on_delete () != ofk->on_delete ())
                diagnose_foreign_key (fk, "on delete action");

              if (fk.referenced_table () != ofk->referenced_table ())
                diagnose_foreign_key (fk, "pointed-to class");

              if (fk.referenced_columns () != ofk->referenced_columns ())
                diagnose_foreign_key (fk, "id member set");

              if (fk.contains_size () != ofk->contains_size ())
                diagnose_foreign_key (fk, "id member set");

              for (foreign_key::contains_size_type i (0);
                   i != fk.contains_size (); ++i)
              {
                sema_rel::contains& c (fk.contains_at (i));
                sema_rel::contains& oc (ofk->contains_at (i));

                if (c.column ().name () != oc.column ().name ())
                  diagnose_foreign_key (fk, "id member set");
              }
            }
            else
            {
              add_foreign_key& afk (g.new_node<add_foreign_key> (fk, at, g));
              g.new_edge<unames> (at, afk, fk.name ());
            }
          }
          else
          {
            if (other.find<foreign_key> (fk.name ()) == 0)
            {
              drop_foreign_key& dfk (g.new_node<drop_foreign_key> (fk.id ()));
              g.new_edge<unames> (at, dfk, fk.name ());
            }
          }
        }

        virtual void
        traverse (sema_rel::index& i)
        {
          using sema_rel::index;

          if (mode == mode_add)
          {
            if (index* oi = other.find<index> (i.name ()))
            {
              if (i.type () != oi->type ())
                diagnose_index (i, "type", oi->type (), i.type ());

              if (i.method () != oi->method ())
                diagnose_index (i, "method", oi->method (), i.method ());

              if (i.options () != oi->options ())
                diagnose_index (i, "options", oi->options (), i.options ());

              if (i.contains_size () != oi->contains_size ())
                diagnose_index (i, "member set", "", "");

              for (index::contains_size_type j (0);
                   j != i.contains_size (); ++j)
              {
                sema_rel::contains& c (i.contains_at (j));
                sema_rel::contains& oc (oi->contains_at (j));

                if (c.column ().name () != oc.column ().name ())
                  diagnose_index (i, "member set", "", "");

                if (c.options () != oc.options ())
                  diagnose_index (
                    i, "member options", oc.options (), c.options ());
              }
            }
            else
            {
              add_index& ai (g.new_node<add_index> (i, at, g));
              g.new_edge<unames> (at, ai, i.name ());
            }
          }
          else
          {
            if (other.find<index> (i.name ()) == 0)
            {
              drop_index& di (g.new_node<drop_index> (i.id ()));
              g.new_edge<unames> (at, di, i.name ());
            }
          }
        }

        void
        diagnose_column (sema_rel::column& c,
                         char const* name,
                         string const& ov,
                         string const& nv)
        {
          table& t (c.table ());
          location const& tl (t.get<location> ("cxx-location"));
          location const& cl (c.get<location> ("cxx-location"));

          error (cl) << "change to data member results in the change of " <<
            "the corresponding column " << name;

          if (!ov.empty () || !nv.empty ())
            cerr << " (old: '" << ov << "', new: '" << nv << "')";

          cerr << endl;

          error (cl) << "this change is not yet handled automatically" << endl;
          info (cl) << "corresponding column '" << c.name () << "' " <<
            "originates here" << endl;
          info (tl) << "corresponding table '" << t.name () << "' " <<
            "originates here" << endl;
          info (cl) << "consider re-implementing this change by adding " <<
            "a new data member with the desired " << name << ", migrating " <<
            "the data, and deleting the old data member" << endl;

          throw operation_failed ();
        }

        void
        diagnose_primary_key (sema_rel::primary_key& pk, char const* name)
        {
          location const& l (pk.get<location> ("cxx-location"));

          error (l) << "changing object id " << name << " in an existing " <<
            "class is not supported" << endl;
          info (l) << "consider re-implementing this change by adding " <<
            "a new class with the desired object id " << name << ", " <<
            "migrating the data, and deleteing the old class" << endl;

          throw operation_failed ();
        }

        void
        diagnose_foreign_key (sema_rel::foreign_key& fk, char const* name)
        {
          location const& l (fk.get<location> ("cxx-location"));

          error (l) << "changing object pointer " << name << " is not " <<
            "supported" << endl;
          info (l) << "consider re-implementing this change by adding " <<
            "a new object pointer with the desired " << name << ", " <<
            "migrating the data, and deleteing the old pointer" << endl;

          throw operation_failed ();
        }

        void
        diagnose_index (sema_rel::index& i,
                        char const* name,
                        string const& ov,
                        string const& nv)
        {
          table& t (i.table ());
          location const& tl (t.get<location> ("cxx-location"));
          location const& il (i.get<location> ("cxx-location"));

          error (il) << "change to index " << name;

          if (!ov.empty () || !nv.empty ())
            cerr << " (old: '" << ov << "', new: '" << nv << "')";

          cerr << " is not yet handled automatically" << endl;

          info (il) << "corresponding index '" << i.name () << "' " <<
            "originates here" << endl;
          info (tl) << "corresponding table '" << t.name () << "' " <<
            "originates here" << endl;
          info (il) << "consider re-implementing this change by adding " <<
            "a new index with the desired " << name << " and deleting the " <<
            "old one" << endl;

          throw operation_failed ();
        }

      protected:
        table& other;
        mode_type mode;
        alter_table& at;
        graph& g;
      };

      struct diff_model: trav_rel::table
      {
        enum mode_type {mode_add, mode_drop};

        diff_model (model& o, mode_type m, changeset& s, graph& gr)
            : other (o), mode (m), cs (s), g (gr) {}

        virtual void
        traverse (sema_rel::table& t)
        {
          using sema_rel::table;

          if (mode == mode_add)
          {
            if (table* ot = other.find<table> (t.name ()))
            {
              // See if there are any changes to the table.
              //
              alter_table& at (g.new_node<alter_table> (t.id ()));

              // Set the alters edge for lookup.
              //
              table* bt (cs.lookup<table, drop_table> (t.name ()));
              assert (bt != 0);
              alters& ae (g.new_edge<alters> (at, *bt));

              {
                trav_rel::table table;
                trav_rel::unames names;
                diff_table dtable (*ot, diff_table::mode_add, at, g);
                table >> names >> dtable;
                table.traverse (t);
              }

              {
                trav_rel::table table;
                trav_rel::unames names;
                diff_table dtable (t, diff_table::mode_drop, at, g);
                table >> names >> dtable;
                table.traverse (*ot);
              }

              if (!at.names_empty ())
                g.new_edge<qnames> (cs, at, t.name ());
              else
              {
                g.delete_edge (at, *bt, ae);
                g.delete_node (at);
              }
            }
            else
            {
              // New table.
              //
              add_table& at (g.new_node<add_table> (t, cs, g));
              g.new_edge<qnames> (cs, at, t.name ());
            }
          }
          else
          {
            if (other.find<table> (t.name ()) == 0)
            {
              drop_table& dt (g.new_node<drop_table> (t.id ()));
              g.new_edge<qnames> (cs, dt, t.name ());
            }
          }
        }

      protected:
        model& other;
        mode_type mode;
        changeset& cs;
        graph& g;
      };

      // Assumes the new model has cxx-location set.
      //
      changeset&
      diff (model& o, model& n, changelog& l)
      {
        changeset& r (l.new_node<changeset> (n.version ()));

        // Set the alters edge for lookup.
        //
        l.new_edge<alters> (r,
                            l.contains_changeset_empty ()
                            ? static_cast<qscope&> (l.model ())
                            : l.contains_changeset_back ().changeset ());

        {
          trav_rel::model model;
          trav_rel::qnames names;
          diff_model dmodel (o, diff_model::mode_add, r, l);
          model >> names >> dmodel;
          model.traverse (n);
        }

        {
          trav_rel::model model;
          trav_rel::qnames names;
          diff_model dmodel (n, diff_model::mode_drop, r, l);
          model >> names >> dmodel;
          model.traverse (o);
        }

        return r;
      }

      //
      // patch
      //

      struct patch_table: trav_rel::add_column,
                          trav_rel::drop_column,
                          trav_rel::alter_column,
                          trav_rel::add_index,
                          trav_rel::drop_index,
                          trav_rel::add_foreign_key,
                          trav_rel::drop_foreign_key
      {
        patch_table (table& tl, graph& gr): t (tl), g (gr) {}

        virtual void
        traverse (sema_rel::add_column& ac)
        {
          try
          {
            column& c (g.new_node<column> (ac, t, g));
            g.new_edge<unames> (t, c, ac.name ());
          }
          catch (duplicate_name const&)
          {
            cerr << "error: invalid changelog: column '" << ac.name () <<
              "' already exists in table '" << t.name () << "'" << endl;
            throw operation_failed ();
          }
        }

        virtual void
        traverse (sema_rel::drop_column& dc)
        {
          table::names_iterator i (t.find (dc.name ()));

          if (i == t.names_end () || !i->nameable ().is_a<column> ())
          {
            cerr << "error: invalid changelog: column '" << dc.name () <<
              "' does not exist in table '" << t.name () << "'" << endl;
            throw operation_failed ();
          }

          g.delete_edge (t, i->nameable (), *i);
        }

        virtual void
        traverse (sema_rel::alter_column& ac)
        {
          if (column* c = t.find<column> (ac.name ()))
          {
            if (ac.null_altered ())
              c->null (ac.null ());
          }
          else
          {
            cerr << "error: invalid changelog: column '" << ac.name () <<
              "' does not exist in table '" << t.name () << "'" << endl;
            throw operation_failed ();
          }
        }

        virtual void
        traverse (sema_rel::add_index& ai)
        {
          using sema_rel::index;

          try
          {
            index& i (g.new_node<index> (ai, t, g));
            g.new_edge<unames> (t, i, ai.name ());
          }
          catch (duplicate_name const&)
          {
            cerr << "error: invalid changelog: index '" << ai.name () <<
              "' already exists in table '" << t.name () << "'" << endl;
            throw operation_failed ();
          }
        }

        virtual void
        traverse (sema_rel::drop_index& di)
        {
          using sema_rel::index;
          table::names_iterator i (t.find (di.name ()));

          if (i == t.names_end () || !i->nameable ().is_a<index> ())
          {
            cerr << "error: invalid changelog: index '" << di.name () <<
              "' does not exist in table '" << t.name () << "'" << endl;
            throw operation_failed ();
          }

          g.delete_edge (t, i->nameable (), *i);
        }

        virtual void
        traverse (sema_rel::add_foreign_key& afk)
        {
          using sema_rel::foreign_key;

          try
          {
            foreign_key& fk (g.new_node<foreign_key> (afk, t, g));
            g.new_edge<unames> (t, fk, afk.name ());
          }
          catch (duplicate_name const&)
          {
            cerr << "error: invalid changelog: foreign key '" << afk.name () <<
              "' already exists in table '" << t.name () << "'" << endl;
            throw operation_failed ();
          }
        }

        virtual void
        traverse (sema_rel::drop_foreign_key& dfk)
        {
          using sema_rel::foreign_key;
          table::names_iterator i (t.find (dfk.name ()));

          if (i == t.names_end () || !i->nameable ().is_a<foreign_key> ())
          {
            cerr << "error: invalid changelog: foreign key '" << dfk.name () <<
              "' does not exist in table '" << t.name () << "'" << endl;
            throw operation_failed ();
          }

          g.delete_edge (t, i->nameable (), *i);
        }

      protected:
        table& t;
        graph& g;
      };

      struct patch_model: trav_rel::add_table,
                          trav_rel::drop_table,
                          trav_rel::alter_table
      {
        patch_model (model& ml, graph& gr): m (ml), g (gr) {}

        virtual void
        traverse (sema_rel::add_table& at)
        {
          try
          {
            table& t (g.new_node<table> (at, m, g));
            g.new_edge<qnames> (m, t, at.name ());
          }
          catch (duplicate_name const&)
          {
            cerr << "error: invalid changelog: table '" << at.name () <<
              "' already exists in model version " << m.version () << endl;
            throw operation_failed ();
          }
        }

        virtual void
        traverse (sema_rel::drop_table& dt)
        {
          model::names_iterator i (m.find (dt.name ()));

          if (i == m.names_end () || !i->nameable ().is_a<table> ())
          {
            cerr << "error: invalid changelog: table '" << dt.name () <<
              "' does not exist in model version " << m.version () << endl;
            throw operation_failed ();
          }

          g.delete_edge (m, i->nameable (), *i);
        }

        virtual void
        traverse (sema_rel::alter_table& at)
        {
          if (table* t = m.find<table> (at.name ()))
          {
            trav_rel::alter_table atable;
            trav_rel::unames names;
            patch_table ptable (*t, g);
            atable >> names >> ptable;
            atable.traverse (at);
          }
          else
          {
            cerr << "error: invalid changelog: table '" << at.name () <<
              "' does not exist in model version " << m.version () << endl;
            throw operation_failed ();
          }
        }

      protected:
        model& m;
        graph& g;
      };

      model&
      patch (model& m, changeset& c, graph& g)
      {
        model& r (g.new_node<model> (m, g));

        trav_rel::changeset changeset;
        trav_rel::qnames names;
        patch_model pmodel (r, g);
        changeset >> names >> pmodel;
        changeset.traverse (c);

        r.version (c.version ());
        return r;
      }
    }

    cutl::shared_ptr<changelog>
    generate (model& m,
              model_version const& mv,
              changelog* old,
              std::string const& in_name,
              std::string const& out_name,
              bool force_init)
    {
      cutl::shared_ptr<changelog> cl (new (shared) changelog);
      graph& g (*cl);

      if (old == 0)
      {
        // Don't allow changelog initialization if the version is closed.
        // This will prevent adding new files to an existing object model
        // with a closed version.
        //
        if (!mv.open)
        {
          cerr << out_name << ": error: unable to initialize changelog " <<
            "because current version is closed" << endl;
          throw operation_failed ();
        }

        if (!force_init)
          cerr << out_name << ": info: initializing changelog with base " <<
            "version " << mv.base << endl;

        if (mv.base == mv.current)
          g.new_edge<contains_model> (*cl, g.new_node<model> (m, g));
        else
        {
          // In this case we have to create an empty model at the base
          // version and a changeset.
          //
          model& nm (g.new_node<model> (mv.base));
          g.new_edge<contains_model> (*cl, nm);
          changeset& c (diff (nm, m, *cl));

          if (!c.names_empty ())
          {
            g.new_edge<alters_model> (c, nm);
            g.new_edge<contains_changeset> (*cl, c);
          }
        }

        return cl;
      }

      // Get the changelog base and current versions and do some sanity
      // checks.
      //
      version bver (old->model ().version ());
      version cver (
        old->contains_changeset_empty ()
        ? bver
        : old->contains_changeset_back ().changeset ().version ());

      if (mv.base < bver)
      {
        cerr << in_name << ": error: latest changelog base version is " <<
          "greater than model base version" << endl;
        throw operation_failed ();
      }

      if (mv.current < cver)
      {
        cerr << in_name << ": error: latest changelog current version is " <<
          "greater than model current version" << endl;
        throw operation_failed ();
      }

      // Build the new changelog.
      //
      model& oldm (old->model ());

      // Now we have a case with a "real" old model (i.e., non-empty
      // and with version older than current) as well as zero or more
      // changeset.
      //
      //
      model* last (&g.new_node<model> (oldm, g));
      model* base (bver == mv.base ? last : 0);
      if (base != 0)
        g.new_edge<contains_model> (*cl, *base);

      for (changelog::contains_changeset_iterator i (
             old->contains_changeset_begin ());
           i != old->contains_changeset_end (); ++i)
      {
        changeset& cs (i->changeset ());

        // Don't copy the changeset for the current version. Instead, we
        // will re-create it from scratch.
        //
        if (cs.version () == mv.current)
          break;

        model& prev (*last);
        last = &patch (prev, cs, g);

        if (base == 0)
        {
          if (last->version () == mv.base)
          {
            base = last;
            g.new_edge<contains_model> (*cl, *base);
          }
          else if (last->version () > mv.base)
          {
            // We have a gap. Plug it with an empty base model. We will
            // also need to create a new changeset for this step.
            //
            base = &g.new_node<model> (mv.base);
            g.new_edge<contains_model> (*cl, *base);

            changeset& c (diff (*base, *last, *cl));
            if (!c.names_empty ())
            {
              g.new_edge<alters_model> (c, *base);
              g.new_edge<contains_changeset> (*cl, c);
            }

            continue;
          }
        }

        // Copy the changeset unless it is below or at our base version.
        //
        if (last->version () <= mv.base)
          continue;

        changeset& c (
          g.new_node<changeset> (
            cs,
            cl->contains_changeset_empty ()
            ? static_cast<qscope&> (*base) // Cannot be NULL.
            : cl->contains_changeset_back ().changeset (),
            g));

        g.new_edge<alters_model> (c, prev);
        g.new_edge<contains_changeset> (*cl, c);
      }

      // If we still haven't found the new base model, then it means it
      // has version greater than any changeset we have seen.
      //
      if (base == 0)
      {
        if (mv.base == mv.current)
          base = &g.new_node<model> (m, g);
        else
        {
          // Fast-forward the latest model to the new base.
          //
          base = last;
          base->version (mv.base);
        }

        g.new_edge<contains_model> (*cl, *base);
      }

      // If the current version is closed, make sure the model hasn't
      // changed.
      //
      if (!mv.open)
      {
        // If the last changeset has the current version, then apply it.
        //
        model* om (last);
        if (!old->contains_changeset_empty ())
        {
          changeset& c (old->contains_changeset_back ().changeset ());
          if (c.version () == mv.current)
            om = &patch (*last, c, g);
        }

        changeset& c (diff (*om, m, *cl));

        if (!c.names_empty ())
        {
          qnames& n (*c.names_begin ());

          cerr << out_name << ": error: current version is closed" << endl;
          cerr << out_name << ": info: first new change is " <<
            n.nameable ().kind () << " '" << n.name () << "'" << endl;

          throw operation_failed ();
        }
      }

      // Add a changeset for the current version unless it is the same
      // as the base version.
      //
      if (mv.base != mv.current)
      {
        changeset& c (diff (*last, m, *cl));

        if (!c.names_empty ())
        {
          g.new_edge<alters_model> (c, *last);
          g.new_edge<contains_changeset> (*cl, c);
        }
      }

      return cl;
    }
  }
}
