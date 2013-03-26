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

      struct diff_table: trav_rel::column
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
                diagnose_unsupported (c, "type");

              if (c.null () != oc->null ())
              {
                alter_column& ac (g.new_node<alter_column> (c.id ()));
                ac.null (c.null ());
                g.new_edge<unames> (at, ac, c.name ());
              }

              if (c.default_ () != oc->default_ ())
                diagnose_unsupported (c, "default value");

              if (c.options () != oc->options ())
                diagnose_unsupported (c, "options");
            }
            else
            {
              add_column& ac (g.new_node<add_column> (c, at, g));
              g.new_edge<unames> (at, ac, c.name ());
            }
          }
          else
          {
            if (other.find<sema_rel::column> (c.name ()) == 0)
            {
              drop_column& dc (g.new_node<drop_column> (c.id ()));
              g.new_edge<unames> (at, dc, c.name ());
            }
          }
        }

        void
        diagnose_unsupported (sema_rel::column& c, char const* name)
        {
          table& t (c.table ());
          location const& tl (t.get<location> ("cxx-location"));
          location const& cl (c.get<location> ("cxx-location"));

          error (cl) << "change to data member results in the change of " <<
            "the corresponding column " << name << endl;
          error (cl) << "this change is not yet handled automatically" << endl;
          info (cl) << "corresponding column '" << c.name () << "' " <<
            "originates here" << endl;
          info (tl) << "corresponding table '" << t.name () << "' " <<
            "originates here" << endl;
          info (cl) << "consider re-implementing this change by creating " <<
            "a new data member with the desired " << name << ", migrating " <<
            "the data, and deleting the old data member" << endl;

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
                g.delete_node (at);
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
      diff (model& o, model& n, graph& g)
      {
        changeset& r (g.new_node<changeset> (n.version ()));

        {
          trav_rel::model model;
          trav_rel::qnames names;
          diff_model dmodel (o, diff_model::mode_add, r, g);
          model >> names >> dmodel;
          model.traverse (n);
        }

        {
          trav_rel::model model;
          trav_rel::qnames names;
          diff_model dmodel (n, diff_model::mode_drop, r, g);
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
                          trav_rel::alter_column
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
              std::string const& out_name)
    {
      cutl::shared_ptr<changelog> cl (new (shared) changelog);
      graph& g (*cl);

      if (old == 0)
      {
        if (!mv.open)
        {
          cerr << out_name << ": error: unable to initialize changelog " <<
            "because current version is closed" << endl;
          throw operation_failed ();
        }

        cerr << out_name << ": info: initializing changelog with base " <<
          "version " << m.version () << endl;

        g.new_edge<contains_model> (*cl, g.new_node<model> (m, g));
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

      if (mv.current < cver)
      {
        cerr << in_name << ": error: latest changelog version is greater " <<
          "than current version" << endl;
        throw operation_failed ();
      }

      // Build the new changelog.
      //
      model& oldm (old->model ());

      // Handle the cases where we just override the log with the current
      // model.
      //
      if (mv.base == mv.current || bver == mv.current || oldm.names_empty ())
      {
        // If the current version is closed, make sure the model hasn't
        // changed.
        //
        if (!mv.open)
        {
          changeset& cs (diff (oldm, m, g));

          if (!cs.names_empty ())
          {
            qnames& n (*cs.names_begin ());

            cerr << out_name << ": error: current version is closed" << endl;
            cerr << out_name << ": info: first new change is " <<
              n.nameable ().kind () << " '" << n.name () << "'" << endl;

            throw operation_failed ();
          }
        }

        g.new_edge<contains_model> (*cl, g.new_node<model> (m, g));
        return cl;
      }

      // Now we have a case with a "real" old model (i.e., non-empty
      // and with version older than current) as well as zero or more
      // changeset.
      //
      //
      model* base (bver >= mv.base ? &g.new_node<model> (oldm, g) : 0);
      model* last (&oldm);

      for (changelog::contains_changeset_iterator i (
             old->contains_changeset_begin ());
           i != old->contains_changeset_end (); ++i)
      {
        changeset& cs (i->changeset ());

        // Don't copy the changeset for the current version. Instead, we
        // will re-create it from scratch.
        //
        if (cs.version () == mv.current)
        {
          // If the current version is closed, make sure the model hasn't
          // changed.
          //
          if (!mv.open)
          {
            model& old (patch (*last, cs, g));
            changeset& cs (diff (old, m, g));

            if (!cs.names_empty ())
            {
              qnames& n (*cs.names_begin ());

              cerr << out_name << ": error: current version is closed" << endl;
              cerr << out_name << ": info: first new change is " <<
                n.nameable ().kind () << " '" << n.name () << "'" << endl;

              throw operation_failed ();
            }
          }
          break;
        }

        last = &patch (*last, cs, g);

        if (base == 0 && last->version () >= mv.base)
          base = last;

        // Copy the changeset unless it is below or at our base version.
        //
        if (last->version () <= mv.base)
          continue;

        g.new_edge<contains_changeset> (*cl, g.new_node<changeset> (cs, g));
      }

      // If we still haven't found the new base model, then take the
      // latest and update its version.
      //
      if (base == 0)
      {
        base = last != &oldm ? last : &g.new_node<model> (oldm, g);
        base->version (mv.base);
      }
      g.new_edge<contains_model> (*cl, *base);

      // Add a changeset for the current version.
      //
      changeset& cs (diff (*last, m, g));

      if (!cs.names_empty ())
        g.new_edge<contains_changeset> (*cl, cs);

      return cl;
    }
  }
}
