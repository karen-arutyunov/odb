// file      : odb/validator.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <iostream>

#include <odb/traversal.hxx>
#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/validator.hxx>

using namespace std;

namespace
{
  // Resolve null overrides.
  //
  static void
  override_null (semantics::node& n, string const& prefix = "")
  {
    string p (prefix.empty () ? prefix : prefix + '-');

    if (n.count (p + "null") && n.count (p + "not-null"))
    {
      if (n.get<location_t> (p + "null-location") <
          n.get<location_t> (p + "not-null-location"))
      {
        n.remove (p + "null");
        n.remove (p + "null-location");
      }
      else
      {
        n.remove (p + "not-null");
        n.remove (p + "not-null-location");
      }
    }
  }

  //
  // Pass 1.
  //

  struct data_member: traversal::data_member
  {
    data_member (bool& valid)
        : valid_ (valid)
    {
    }

    virtual void
    traverse (type& m)
    {
      if (context::transient (m))
        return;

      count_++;
      semantics::type& type (m.type ());
      semantics::belongs& b (m.belongs ());

      if (type.fq_anonymous (b.hint ()))
      {
        cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " error: unnamed type in data member declaration" << endl;

        cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " info: use 'typedef' to name this type" << endl;

        valid_ = false;
      }

      // Resolve null overrides.
      //
      override_null (m);
      override_null (m, "value");
    }

    bool& valid_;
    size_t count_;
  };

  // Find id member.
  //
  struct id_member: traversal::class_
  {
    id_member (class_kind kind, bool& valid, semantics::data_member*& m)
        : kind_ (kind), member_ (valid, m)
    {
      if (kind != class_view)
        *this >> inherits_ >> *this;

      *this >> names_ >> member_;
    }

    virtual void
    traverse (semantics::class_& c)
    {
      // Skip transient bases.
      //
      switch (kind_)
      {
      case class_object:
        {
          if (!context::object (c))
            return;
          break;
        }
      case class_view:
        {
          break;
        }
      case class_composite:
        {
          if (!context::composite (c))
            return;
          break;
        }
      case class_other:
        {
          assert (false);
          break;
        }
      }

      // Views don't have bases.
      //
      if (kind_ != class_view)
        inherits (c);

      names (c);
    }

  private:
    struct member: traversal::data_member
    {
      member (bool& valid, semantics::data_member*& m)
          : valid_ (valid), m_ (m)
      {
      }

      virtual void
      traverse (semantics::data_member& m)
      {
        if (m.count ("id"))
        {
          if (m_ != 0)
          {
            cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
                 << " error: multiple object id members" << endl;

            cerr << m_->file () << ":" << m_->line () << ":" << m_->column ()
                 << ": info: previous id member declared here" << endl;

            valid_ = false;
          }
          else
            m_ = &m;
        }
      }

      bool& valid_;
      semantics::data_member*& m_;
    };

    class_kind kind_;
    member member_;
    traversal::names names_;
    traversal::inherits inherits_;
  };

  //
  //
  struct value_type: traversal::type
  {
    value_type (bool& valid): valid_ (valid) {}

    virtual void
    traverse (semantics::type& t)
    {
      // Resolve null overrides.
      //
      override_null (t);
      override_null (t, "value");
    }

    bool& valid_;
  };

  //
  //
  struct class1: traversal::class_
  {
    class1 (bool& valid,
            options const& ops,
            semantics::unit& unit,
            value_type& vt)
        : valid_ (valid),
          options_ (ops),
          unit_ (unit),
          vt_ (vt),
          member_ (valid)
    {
      *this >> names_ >> member_;
    }

    virtual void
    traverse (type& c)
    {
      if (context::object (c))
        traverse_object (c);
      else if (context::view (c))
        traverse_view (c);
      else
      {
        if (context::composite (c))
          traverse_composite (c);

        vt_.dispatch (c);
      }
    }

    virtual void
    traverse_object (type& c)
    {
      // Check that the callback function exist.
      //
      if (c.count ("callback"))
      {
        string name (c.get<string> ("callback"));
        tree decl (
          lookup_qualified_name (
            c.tree_node (), get_identifier (name.c_str ()), false, false));

        if (decl == error_mark_node || TREE_CODE (decl) != BASELINK)
        {
          cerr << c.file () << ":" << c.line () << ":" << c.column () << ": "
               << "error: unable to resolve member function '" << name << "' "
               << "specified with '#pragma db callback' for class '"
               << c.name () << "'" << endl;

          valid_ = false;
        }

        // Figure out if we have a const version of the callback. OVL_*
        // macros work for both FUNCTION_DECL and OVERLOAD.
        //
        for (tree o (BASELINK_FUNCTIONS (decl)); o != 0; o = OVL_NEXT (o))
        {
          tree f (OVL_CURRENT (o));
          if (DECL_CONST_MEMFUNC_P (f))
          {
            c.set ("callback-const", true);
            break;
          }
        }

        //@@ Would be nice to check the signature of the function(s)
        //   instead of postponing it until the C++ compilation. Though
        //   we may still get C++ compilation errors because of const
        //   mismatch.
        //
      }

      // Check bases.
      //
      bool base (false);

      for (type::inherits_iterator i (c.inherits_begin ());
           i != c.inherits_end ();
           ++i)
      {
        type& b (i->base ());

        if (context::object (b))
          base = true;
        else if (context::view (b) || context::composite (b))
        {
          // @@ Should we use hint here?
          //
          string name (b.fq_name ());

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: base class '" << name << "' is a view or value type"
               << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: object types cannot derive from view or value "
               << "types"
               << endl;

          cerr << b.file () << ":" << b.line () << ":" << b.column () << ":"
               << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      {
        id_member t (class_object, valid_, id);
        t.traverse (c);
      }

      if (id == 0)
      {
        if (!context::abstract (c))
        {
          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: no data member designated as object id" << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: use '#pragma db id' to specify object id member"
               << endl;

          valid_ = false;
        }
      }
      else
      {
        c.set ("id-member", id);

        // Complain if an id member has a default value (default value
        // for the id's type is ok -- we will ignore it).
        //
        if (id->count ("default"))
        {
          cerr << id->file () << ":" << id->line () << ":" << id->column ()
               << ": error: object id member cannot have default value"
               << endl;

          valid_ = false;
        }

        // Automatically mark the id member as not null. If we already have
        // an explicit null pragma for this member, issue an error.
        //
        if (id->count ("null"))
        {
          cerr << id->file () << ":" << id->line () << ":" << id->column ()
               << ": error: object id member cannot be null" << endl;

          valid_ = false;
        }
        else
          id->set ("not-null", true);
      }

      // Check members.
      //
      member_.count_ = 0;
      names (c);

      if (member_.count_ == 0 && !base)
      {
        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }
    }

    virtual void
    traverse_view (type& c)
    {
      // Views require query support.
      //
      if (!options_.generate_query ())
      {
        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: query support is required when using views"
             << endl;

        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: use the --generate-query option to enable query "
             << "support"
             << endl;

        valid_ = false;
      }

      // Check bases.
      //
      for (type::inherits_iterator i (c.inherits_begin ());
           i != c.inherits_end ();
           ++i)
      {
        type& b (i->base ());

        if (context::object (b) ||
            context::view (b) ||
            context::composite (b))
        {
          // @@ Should we use hint here?
          //
          string name (b.fq_name ());

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: base class '" << name << "' is an object, "
               << "view, or value type"
               << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: view types cannot derive from view, object or "
               << "value types"
               << endl;

          cerr << b.file () << ":" << b.line () << ":" << b.column () << ":"
               << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      {
        id_member t (class_view, valid_, id);
        t.traverse (c);
      }

      if (id != 0)
      {
        cerr << id->file () << ":" << id->line () << ":" << id->column ()
             << ": error: view type data member cannot be designated as "
             << "object id" << endl;

        valid_ = false;
      }

      // Check members.
      //
      member_.count_ = 0;
      names (c);

      if (member_.count_ == 0)
      {
        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }
    }

    virtual void
    traverse_composite (type& c)
    {
      bool base (false);

      for (type::inherits_iterator i (c.inherits_begin ());
           i != c.inherits_end ();
           ++i)
      {
        type& b (i->base ());

        if (context::composite (b))
          base = true;
        else if (context::object (b) || context::view (b))
        {
          // @@ Should we use hint here?
          //
          string name (b.fq_name ());

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: base class '" << name << "' is a view or object "
               << "type"
               << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: composite value types cannot derive from object "
               << "or view types" << endl;

          cerr << b.file () << ":" << b.line () << ":" << b.column () << ":"
               << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      {
        id_member t (class_composite, valid_, id);
        t.traverse (c);
      }

      if (id != 0)
      {
        cerr << id->file () << ":" << id->line () << ":" << id->column ()
             << ": error: value type data member cannot be designated as "
             << "object id" << endl;

        valid_ = false;
      }

      // Check members.
      //
      member_.count_ = 0;
      names (c);

      if (member_.count_ == 0 && !base)
      {
        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }
    }

    bool& valid_;
    options const& options_;
    semantics::unit& unit_;
    value_type& vt_;

    data_member member_;
    traversal::names names_;
  };

  //
  // Pass 2.
  //

  struct view_members: object_members_base
  {
    view_members (bool& valid)
        : object_members_base (false, false, true), valid_ (valid), dm_ (0)
    {
    }

    virtual void
    traverse_simple (semantics::data_member& m)
    {
      if (context::object_pointer (m.type ()))
      {
        semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

        cerr << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
             << " error: view data member '" << member_prefix_ << m.name ()
             << "' is an object pointer" << endl;

        cerr << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
             << ": info: views cannot contain object pointers" << endl;

        valid_ = false;
      }
    }

    virtual void
    traverse_container (semantics::data_member& m, semantics::type&)
    {
      semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

      cerr << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
           << " error: view data member '" << member_prefix_ << m.name ()
           << "' is a container" << endl;

      cerr << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
           << ": info: views cannot contain containers" << endl;

      valid_ = false;
    }

    virtual void
    traverse_composite (semantics::data_member* m, semantics::class_& c)
    {
      semantics::data_member* old_dm (dm_);

      if (dm_ == 0)
        dm_ = m;

      object_members_base::traverse_composite (m, c);

      dm_ = old_dm;
    }

  private:
    bool& valid_;
    semantics::data_member* dm_; // Direct view data member.
  };

  //
  //
  struct class2: traversal::class_
  {
    class2 (bool& valid, options const& ops, semantics::unit& unit)
        : valid_ (valid), options_ (ops), unit_ (unit), view_members_ (valid)
    {
    }

    virtual void
    traverse (type& c)
    {
      if (context::object (c))
        traverse_object (c);
      else if (context::view (c))
        traverse_view (c);
      else if (context::composite (c))
        traverse_composite (c);
    }

    virtual void
    traverse_object (type&)
    {
    }

    virtual void
    traverse_view (type& c)
    {
      // Make sure we don't have any containers or object pointers.
      //
      view_members_.traverse (c);
    }

    virtual void
    traverse_composite (type&)
    {
    }

    bool& valid_;
    options const& options_;
    semantics::unit& unit_;

    view_members view_members_;
  };
}

bool validator::
validate (options const& ops,
          semantics::unit& u,
          semantics::path const&,
          unsigned short pass)
{
  auto_ptr<context> ctx (create_context (cerr, u, ops));

  bool valid (true);

  if (pass == 1)
  {
    traversal::unit unit;
    traversal::defines unit_defines;
    traversal::declares unit_declares;
    traversal::namespace_ ns;
    value_type vt (valid);
    class1 c (valid, ops, u, vt);

    unit >> unit_defines >> ns;
    unit_defines >> c;
    unit >> unit_declares >> vt;

    traversal::defines ns_defines;
    traversal::declares ns_declares;

    ns >> ns_defines >> ns;
    ns_defines >> c;
    ns >> ns_declares >> vt;

    unit.dispatch (u);
  }
  else
  {
    traversal::unit unit;
    traversal::defines unit_defines;
    traversal::namespace_ ns;
    value_type vt (valid);
    class2 c (valid, ops, u);

    unit >> unit_defines >> ns;
    unit_defines >> c;

    traversal::defines ns_defines;

    ns >> ns_defines >> ns;
    ns_defines >> c;

    unit.dispatch (u);
  }

  return valid;
}
