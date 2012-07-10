// file      : odb/validator.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <iostream>

#include <odb/traversal.hxx>
#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/validator.hxx>

#include <odb/relational/validator.hxx>

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

  struct data_member1: traversal::data_member, context
  {
    data_member1 (bool& valid)
        : valid_ (valid)
    {
    }

    virtual void
    traverse (type& m)
    {
      if (transient (m))
        return;

      count_++;
      semantics::names* hint;
      semantics::type& t (utype (m, hint));

      if (t.fq_anonymous (hint))
      {
        os << m.file () << ":" << m.line () << ":" << m.column () << ":"
           << " error: unnamed type in data member declaration" << endl;

        os << m.file () << ":" << m.line () << ":" << m.column () << ":"
           << " info: use 'typedef' to name this type" << endl;

        valid_ = false;
      }

      // Make sure id or inverse member is not marked readonly since we
      // depend on these three sets not having overlaps.
      //
      if (m.count ("readonly")) // context::readonly() also checks the class.
      {
        if (id (m))
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " error: object id should not be declared readonly" << endl;

          valid_ = false;
        }

        if (inverse (m))
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " error: inverse object pointer should not be declared "
             << "readonly" << endl;

          valid_ = false;
        }
      }

      // Resolve null overrides.
      //
      override_null (m);
      override_null (m, "value");
    }

    bool& valid_;
    size_t count_;
  };

  // Find special members (id, version).
  //
  struct special_members: traversal::class_, context
  {
    special_members (class_kind_type kind,
                     bool& valid,
                     semantics::data_member*& id,
                     semantics::data_member*& optimistic)
        : kind_ (kind), member_ (valid, id, optimistic)
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
          if (!object (c))
            return;
          break;
        }
      case class_view:
        {
          break;
        }
      case class_composite:
        {
          if (!composite (c))
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
    struct member: traversal::data_member, context
    {
      member (bool& valid,
              semantics::data_member*& id,
              semantics::data_member*& optimistic)
          : valid_ (valid), id_ (id), optimistic_ (optimistic)
      {
      }

      virtual void
      traverse (semantics::data_member& m)
      {
        if (id (m))
        {
          if (id_ == 0)
            id_ = &m;
          else
          {
            os << m.file () << ":" << m.line () << ":" << m.column () << ":"
               << " error: multiple object id members" << endl;

            os << id_->file () << ":" << id_->line () << ":" << id_->column ()
               << ": info: previous id member is declared here" << endl;

            valid_ = false;
          }
        }

        if (version (m))
        {
          if (optimistic_ == 0)
            optimistic_ = &m;
          else
          {
            os << m.file () << ":" << m.line () << ":" << m.column () << ":"
               << " error: multiple version members" << endl;

            semantics::data_member& o (*optimistic_);

            os << o.file () << ":" << o.line () << ":" << o.column ()
               << ": info: previous version member is declared here" << endl;

            valid_ = false;
          }
        }
      }

      bool& valid_;
      semantics::data_member*& id_;
      semantics::data_member*& optimistic_;
    };

    class_kind_type kind_;
    member member_;
    traversal::names names_;
    traversal::inherits inherits_;
  };

  //
  //
  struct value_type: traversal::type, context
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
  struct class1: traversal::class_, context
  {
    class1 (bool& valid, value_type& vt)
        : valid_ (valid), vt_ (vt), member_ (valid)
    {
      *this >> names_ >> member_;
    }

    virtual void
    traverse (type& c)
    {
      if (object (c))
        traverse_object (c);
      else if (view (c))
        traverse_view (c);
      else
      {
        if (composite (c))
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
          os << c.file () << ":" << c.line () << ":" << c.column () << ": "
             << "error: unable to resolve member function '" << name << "' "
             << "specified with '#pragma db callback' for class '"
             << class_name (c) << "'" << endl;

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
      type* poly_root (0);

      for (type::inherits_iterator i (c.inherits_begin ());
           i != c.inherits_end ();
           ++i)
      {
        type& b (i->base ());

        if (object (b))
        {
          base = true;

          if (type* r = polymorphic (b))
          {
            if (poly_root == 0)
            {
              poly_root = r;
              c.set ("polymorphic-base", &static_cast<semantics::class_&> (b));
            }
            // If poly_root and r are the same, then we have virtual
            // inheritance. Though we don't support it at the moment.
            //
            else //if (poly_root != r)
            {
              os << c.file () << ":" << c.line () << ":" << c.column () << ":"
                 << " error: persistent class '" << class_name (c) << "' "
                 << "derives from multiple polymorphic bases" << endl;

              type& a (*poly_root);
              os << a.file () << ":" << a.line () << ":" << a.column () << ":"
                 << " info: first polymorphic base defined here" << endl;

              type& b (*r);
              os << b.file () << ":" << b.line () << ":" << b.column () << ":"
                 << " info: second polymorphic base defined here" << endl;

              valid_ = false;
            }
          }
        }
        else if (view (b) || composite (b))
        {
          // @@ Should we use hint here?
          //
          string name (class_fq_name (b));

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: base class '" << name << "' is a view or value type"
             << endl;

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: object types cannot derive from view or value "
             << "types"
             << endl;

          os << b.file () << ":" << b.line () << ":" << b.column () << ":"
             << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check special members.
      //
      semantics::data_member* id (0);
      semantics::data_member* optimistic (0);
      {
        special_members t (class_object, valid_, id, optimistic);
        t.traverse (c);
      }

      if (id == 0)
      {
        // An object without an id should either be reuse-abstract
        // or explicitly marked as such. We check that it is not
        // polymorphic below.
        //
        if (!(c.count ("id") || abstract (c)))
        {
          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: no data member designated as an object id" << endl;

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: use '#pragma db id' to specify an object id member"
             << endl;

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: or explicitly declare that this persistent class "
             << "has no object id" << endl;

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
          os << id->file () << ":" << id->line () << ":" << id->column ()
             << ": error: object id member cannot have default value"
             << endl;

          valid_ = false;
        }

        // Automatically mark the id member as not null. If we already have
        // an explicit null pragma for this member, issue an error.
        //
        if (id->count ("null"))
        {
          os << id->file () << ":" << id->line () << ":" << id->column ()
             << ": error: object id member cannot be null" << endl;

          valid_ = false;
        }
        else
          id->set ("not-null", true);
      }

      if (optimistic != 0)
      {
        semantics::data_member& m (*optimistic);

        // Make sure we have the class declared optimistic.
        //
        if (&optimistic->scope () == &c && !c.count ("optimistic"))
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " error: version data member in a class not declared "
             << "optimistic" << endl;

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: use '#pragma db optimistic' to declare this "
             << "class optimistic" << endl;

          valid_ = false;
        }

        // Make sure we have object id.
        //
        if (id == 0)
        {
          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: optimistic class without an object id" << endl;

          valid_ = false;
        }

        // Make sure id and version members are in the same class. The
        // current architecture relies on that.
        //
        if (id != 0 && &id->scope () != &optimistic->scope ())
        {
          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: object id and version members are in different "
             << "classes" << endl;

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: object id and version members must be in the same "
             << "class" << endl;

          os << id->file () << ":" << id->line () << ":" << id->column ()
             << ": info: object id member is declared here" << endl;

          os << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " error: version member is declared here" << endl;

          valid_ = false;
        }

        // Make sure this class is not readonly.
        //
        if (readonly (c))
        {
          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: optimistic class cannot be readonly" << endl;

          valid_ = false;
        }

        // This takes care of also marking derived classes as optimistic.
        //
        c.set ("optimistic-member", optimistic);
      }
      else
      {
        // Make sure there is a version member if the class declared
        // optimistic.
        //
        if (c.count ("optimistic"))
        {
          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: optimistic class without a version member" << endl;

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: use '#pragma db version' to declare on of the "
             << "data members as a version" << endl;

          valid_ = false;
        }
      }

      // Polymorphic inheritance.
      //
      if (c.count ("polymorphic") && poly_root == 0)
      {
        // Root of the hierarchy.
        //

        if (id == 0)
        {
          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: polymorphic class without an object id" << endl;

          valid_ = false;
        }

        if (!TYPE_POLYMORPHIC_P (c.tree_node ()))
        {
          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: non-polymorphic class (class without virtual "
             << "functions) cannot be declared polymorphic" << endl;

          valid_ = false;
        }

        poly_root = &c;
      }

      if (poly_root != 0)
        c.set ("polymorphic-root", poly_root);

      // Check members.
      //
      member_.count_ = 0;
      names (c);

      if (member_.count_ == 0 && !base)
      {
        os << c.file () << ":" << c.line () << ":" << c.column () << ":"
           << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }

      // Update features set based on this object.
      //
      if (class_file (c) == unit.file ())
      {
        if (poly_root != 0)
          features.polymorphic_object = true;
        else if (id == 0 && !abstract (c))
          features.no_id_object = true;
        else
          features.simple_object = true;
      }
    }

    virtual void
    traverse_view (type& c)
    {
      // Views require query support.
      //
      if (!options.generate_query ())
      {
        os << c.file () << ":" << c.line () << ":" << c.column () << ":"
           << " error: query support is required when using views"
           << endl;

        os << c.file () << ":" << c.line () << ":" << c.column () << ":"
           << " info: use the --generate-query option to enable query "
           << "support"
           << endl;

        valid_ = false;
      }

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
          os << c.file () << ":" << c.line () << ":" << c.column () << ": "
             << "error: unable to resolve member function '" << name << "' "
             << "specified with '#pragma db callback' for class '"
             << class_name (c) << "'" << endl;

          valid_ = false;
        }

        // No const version for views.

        //@@ Would be nice to check the signature of the function(s)
        //   instead of postponing it until the C++ compilation. Though
        //   we may still get C++ compilation errors because of const
        //   mismatch.
        //
      }

      // Check bases.
      //
      for (type::inherits_iterator i (c.inherits_begin ());
           i != c.inherits_end ();
           ++i)
      {
        type& b (i->base ());

        if (object (b) || view (b) || composite (b))
        {
          // @@ Should we use hint here?
          //
          string name (class_fq_name (b));

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: base class '" << name << "' is an object, "
             << "view, or value type"
             << endl;

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: view types cannot derive from view, object or "
             << "value types"
             << endl;

          os << b.file () << ":" << b.line () << ":" << b.column () << ":"
             << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      semantics::data_member* optimistic (0);
      {
        special_members t (class_view, valid_, id, optimistic);
        t.traverse (c);
      }

      if (id != 0)
      {
        os << id->file () << ":" << id->line () << ":" << id->column ()
           << ": error: view type data member cannot be designated as an "
           << "object id" << endl;

        valid_ = false;
      }

      if (optimistic != 0)
      {
        semantics::data_member& o (*optimistic);

        os << o.file () << ":" << o.line () << ":" << o.column ()
           << ": error: view type data member cannot be designated as a "
           << "version" << endl;

        valid_ = false;
      }

      // Check members.
      //
      member_.count_ = 0;
      names (c);

      if (member_.count_ == 0)
      {
        os << c.file () << ":" << c.line () << ":" << c.column () << ":"
           << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }

      // Update features set based on this view.
      //
      if (class_file (c) == unit.file ())
      {
        features.view = true;
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

        if (composite (b))
          base = true;
        else if (object (b) || view (b))
        {
          // @@ Should we use hint here?
          //
          string name (class_fq_name (b));

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: base class '" << name << "' is a view or object "
             << "type"
             << endl;

          os << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " info: composite value types cannot derive from object "
             << "or view types" << endl;

          os << b.file () << ":" << b.line () << ":" << b.column () << ":"
             << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      semantics::data_member* optimistic (0);
      {
        special_members t (class_composite, valid_, id, optimistic);
        t.traverse (c);
      }

      if (id != 0)
      {
        os << id->file () << ":" << id->line () << ":" << id->column ()
           << ": error: value type data member cannot be designated as an "
           << "object id" << endl;

        valid_ = false;
      }

      if (optimistic != 0)
      {
        semantics::data_member& o (*optimistic);

        os << o.file () << ":" << o.line () << ":" << o.column ()
           << ": error: value type data member cannot be designated as a "
           << "version" << endl;

        valid_ = false;
      }

      // Check members.
      //
      member_.count_ = 0;
      names (c);

      if (member_.count_ == 0 && !base)
      {
        os << c.file () << ":" << c.line () << ":" << c.column () << ":"
           << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }
    }

    bool& valid_;
    value_type& vt_;

    data_member1 member_;
    traversal::names names_;
  };

  struct typedefs1: typedefs
  {
    typedefs1 (traversal::declares& d)
        : typedefs (true), declares_ (d)
    {
    }

    void
    traverse (semantics::typedefs& t)
    {
      if (check (t))
        traversal::typedefs::traverse (t);
      else
        declares_.traverse (t);
    }

  private:
    traversal::declares& declares_;
  };

  //
  // Pass 2.
  //

  struct data_member2: traversal::data_member, context
  {
    data_member2 (bool& valid)
        : valid_ (valid)
    {
    }

    virtual void
    traverse (type& m)
    {
      if (transient (m))
        return;

      if (null (m))
      {
        if (semantics::class_* c = composite_wrapper (utype (m)))
        {
          if (has_a (*c, test_container))
          {
            os << m.file () << ":" << m.line () << ":" << m.column () << ":"
               << " error: composite member containing containers cannot "
               << "be null" << endl;

            os << c->file () << ":" << c->line () << ":" << c->column () << ":"
               << " info: composite value type is defined here" << endl;

            valid_ = false;
          }
        }
      }
    }

    bool& valid_;
  };

  struct object_no_id_members: object_members_base
  {
    object_no_id_members (bool& valid)
        : object_members_base (false, false, true), valid_ (valid), dm_ (0)
    {
    }

    virtual void
    traverse_pointer (semantics::data_member& m, semantics::class_&)
    {
      if (inverse (m))
      {
        semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

        os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
           << " error: inverse object pointer member '" << member_prefix_
           << m.name () << "' in an object without an object id" << endl;

        valid_ = false;
      }
    }

    virtual void
    traverse_container (semantics::data_member& m, semantics::type&)
    {
      semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

      os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
         << " error: container member '" << member_prefix_ << m.name ()
         << "' in an object without an object id" << endl;

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
    semantics::data_member* dm_; // Direct object data member.
  };

  struct composite_id_members: object_members_base
  {
    composite_id_members (bool& valid)
        : object_members_base (false, false, true), valid_ (valid), dm_ (0)
    {
    }

    virtual void
    traverse_pointer (semantics::data_member& m, semantics::class_&)
    {
      semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

      os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
         << " error: object pointer member '" << member_prefix_ << m.name ()
         << "' in a composite value type that is used as an object id" << endl;

      valid_ = false;
    }

    virtual void
    traverse_simple (semantics::data_member& m)
    {
      if (readonly (member_path_, member_scope_))
      {
        semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

        os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
           << " error: readonly member '" << member_prefix_ << m.name ()
           << "' in a composite value type that is used as an object id"
           << endl;

        valid_ = false;
      }
    }

    virtual void
    traverse_container (semantics::data_member& m, semantics::type&)
    {
      semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

      os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
         << " error: container member '" << member_prefix_ << m.name ()
         << "' in a composite value type that is used as an object id" << endl;

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
    semantics::data_member* dm_; // Direct composite member.
  };

  struct view_members: object_members_base
  {
    view_members (bool& valid)
        : object_members_base (false, false, true), valid_ (valid), dm_ (0)
    {
    }

    virtual void
    traverse_simple (semantics::data_member& m)
    {
      if (object_pointer (utype (m)))
      {
        semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

        os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
           << " error: view data member '" << member_prefix_ << m.name ()
           << "' is an object pointer" << endl;

        os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
           << ": info: views cannot contain object pointers" << endl;

        valid_ = false;
      }
    }

    virtual void
    traverse_container (semantics::data_member& m, semantics::type&)
    {
      semantics::data_member& dm (dm_ != 0 ? *dm_ : m);

      os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
         << " error: view data member '" << member_prefix_ << m.name ()
         << "' is a container" << endl;

      os << dm.file () << ":" << dm.line () << ":" << dm.column () << ":"
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
  struct class2: traversal::class_, context
  {
    class2 (bool& valid)
        : valid_ (valid),
          data_member_ (valid),
          object_no_id_members_ (valid),
          composite_id_members_ (valid),
          view_members_ (valid)
    {
      *this >> data_member_names_ >> data_member_;

      // Find the has_lt_operator function template..
      //
      has_lt_operator_ = 0;

      tree odb (
        lookup_qualified_name (
          global_namespace, get_identifier ("odb"), false, false));

      if (odb != error_mark_node)
      {
        tree compiler (
          lookup_qualified_name (
            odb, get_identifier ("compiler"), false, false));

        if (compiler != error_mark_node)
        {
          has_lt_operator_ = lookup_qualified_name (
            compiler, get_identifier ("has_lt_operator"), false, false);

          if (has_lt_operator_ != error_mark_node)
            has_lt_operator_ = OVL_CURRENT (has_lt_operator_);
          else
          {
            os << unit.file () << ": error: unable to resolve has_lt_operator "
               << "function template inside odb::compiler" << endl;
            has_lt_operator_ = 0;
          }
        }
        else
          os << unit.file () << ": error: unable to resolve compiler "
             << "namespace inside odb" << endl;
      }
      else
        os << unit.file () << ": error: unable to resolve odb namespace"
           << endl;

      if (has_lt_operator_ == 0)
        valid_ = false;
    }

    virtual void
    traverse (type& c)
    {
      if (object (c))
        traverse_object (c);
      else if (view (c))
        traverse_view (c);
      else if (composite (c))
        traverse_composite (c);
    }

    virtual void
    traverse_object (type& c)
    {
      semantics::data_member* id (id_member (c));

      if (id != 0)
      {
        if (semantics::class_* cm = composite_wrapper (utype (*id)))
        {
          // Composite id cannot be auto.
          //
          if (auto_ (*id))
          {
            os << id->file () << ":" << id->line () << ":" << id->column ()
               << ": error: composite id cannot be automatically assigned"
               << endl;

            valid_ = false;
          }

          // Make sure we don't have any containers or pointers in this
          // composite value type.
          //
          if (valid_)
          {
            composite_id_members_.traverse (*cm);

            if (!valid_)
              os << id->file () << ":" << id->line () << ":" << id->column ()
                 << ": info: composite id is defined here" << endl;
          }

          // Check that the composite value type is default-constructible.
          //
          if (!cm->default_ctor ())
          {
            os << cm->file () << ":" << cm->line () << ":" << cm->column ()
               << ": error: composite value type that is used as object id "
               << "is not default-constructible" << endl;

            os << cm->file () << ":" << cm->line () << ":" << cm->column ()
               << ": info: provide default constructor for this value type"
               << endl;

            os << id->file () << ":" << id->line () << ":" << id->column ()
               << ": info: composite id is defined here" << endl;

            valid_ = false;
          }

          // If this is a session object, make sure that the composite
          // value can be compared.
          //
          if (session (c) && has_lt_operator_ != 0)
          {
            tree args (make_tree_vec (1));
            TREE_VEC_ELT (args, 0) = cm->tree_node ();

            tree inst (
              instantiate_template (
                has_lt_operator_, args, tf_none));

            bool v (inst != error_mark_node);

            if (v &&
                DECL_TEMPLATE_INSTANTIATION (inst) &&
                !DECL_TEMPLATE_INSTANTIATED (inst))
            {
              // Instantiate this function template to see if the value type
              // provides operator<. Unfortunately, GCC instantiate_decl()
              // does not provide any control over the diagnostics it issues
              // in case of an error. To work around this, we are going to
              // temporarily redirect diagnostics to /dev/null, which is
              // where asm_out_file points to (see plugin.cxx).
              //
              int ec (errorcount);
              FILE* s (global_dc->printer->buffer->stream);
              global_dc->printer->buffer->stream = asm_out_file;

              instantiate_decl (inst, false, false);

              global_dc->printer->buffer->stream = s;
              v = (ec == errorcount);
            }

            if (!v)
            {
              os << cm->file () << ":" << cm->line () << ":" << cm->column ()
                 << ": error: composite value type that is used as object id "
                 << "in persistent class with session support does not "
                 << "define the less than (<) comparison"
                 << endl;

              os << cm->file () << ":" << cm->line () << ":" << cm->column ()
                 << ": info: provide operator< for this value type" << endl;

              os << id->file () << ":" << id->line () << ":" << id->column ()
                 << ": info: composite id is defined here" << endl;

              os << c.file () << ":" << c.line () << ":" << c.column ()
                 << ": info: persistent class is defined here" << endl;

              valid_ = false;
            }
          }
        }
      }
      else
      {
        if (!abstract (c))
        {
          // Make sure we don't have any containers or inverse pointers.
          //
          object_no_id_members_.traverse (c);
        }
      }

      names (c);
    }

    virtual void
    traverse_view (type& c)
    {
      // Make sure we don't have any containers or object pointers.
      //
      view_members_.traverse (c);

      names (c);
    }

    virtual void
    traverse_composite (type& c)
    {
      names (c);
    }

    bool& valid_;
    tree has_lt_operator_;

    data_member2 data_member_;
    traversal::names data_member_names_;

    object_no_id_members object_no_id_members_;
    composite_id_members composite_id_members_;
    view_members view_members_;
  };
}

void validator::
validate (options const& ops,
          features& f,
          semantics::unit& u,
          semantics::path const& p,
          unsigned short pass)
{
  bool valid (true);

  {
    auto_ptr<context> ctx (create_context (cerr, u, ops, f, 0));

    if (pass == 1)
    {
      traversal::unit unit;
      traversal::defines unit_defines;
      traversal::declares unit_declares;
      typedefs1 unit_typedefs (unit_declares);
      traversal::namespace_ ns;
      value_type vt (valid);
      class1 c (valid, vt);

      unit >> unit_defines >> ns;
      unit_defines >> c;
      unit >> unit_declares >> vt;
      unit >> unit_typedefs >> c;

      traversal::defines ns_defines;
      traversal::declares ns_declares;
      typedefs1 ns_typedefs (ns_declares);

      ns >> ns_defines >> ns;
      ns_defines >> c;
      ns >> ns_declares >> vt;
      ns >> ns_typedefs >> c;

      unit.dispatch (u);
    }
    else
    {
      traversal::unit unit;
      traversal::defines unit_defines;
      typedefs unit_typedefs (true);
      traversal::namespace_ ns;
      class2 c (valid);

      unit >> unit_defines >> ns;
      unit_defines >> c;
      unit >> unit_typedefs >> c;

      traversal::defines ns_defines;
      typedefs ns_typedefs (true);

      ns >> ns_defines >> ns;
      ns_defines >> c;
      ns >> ns_typedefs >> c;

      unit.dispatch (u);
    }
  }

  if (!valid)
    throw failed ();

  try
  {
    relational::validator v;
    v.validate (ops, f, u, p, pass);
  }
  catch (relational::validator::failed const&)
  {
    throw failed ();
  }
}
