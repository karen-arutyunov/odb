// file      : odb/validator.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <iostream>

#include <odb/traversal.hxx>
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
      if (n.get<location_t> (p + "null-loc") <
          n.get<location_t> (p + "not-null-loc"))
      {
        n.remove (p + "null");
        n.remove (p + "null-loc");
      }
      else
      {
        n.remove (p + "not-null");
        n.remove (p + "not-null-loc");
      }
    }
  }

  struct data_member: traversal::data_member
  {
    data_member (bool& valid)
        : valid_ (valid)
    {
    }

    virtual void
    traverse (type& m)
    {
      if (m.count ("transient"))
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
    id_member (bool object, bool& valid, semantics::data_member*& m)
        : object_ (object), member_ (valid, m)
    {
      *this >> inherits_ >> *this;
      *this >> names_ >> member_;
    }

    virtual void
    traverse (semantics::class_& c)
    {
      // Skip transient bases.
      //
      if (object_)
      {
        if (!c.count ("object"))
          return;
      }
      else
      {
        if (!context::comp_value (c))
          return;
      }

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

    bool object_;
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
  struct class_: traversal::class_
  {
    class_ (bool& valid, semantics::unit& unit, value_type& vt)
        : valid_ (valid), unit_ (unit), vt_ (vt), member_ (valid)
    {
      *this >> names_ >> member_;
    }

    virtual void
    traverse (type& c)
    {
      if (c.count ("object"))
        traverse_object (c);
      else
      {
        if (context::comp_value (c))
          traverse_value (c);

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

        if (b.count ("object"))
          base = true;
        if (context::comp_value (b))
        {
          // @@ Should we use hint here?
          //
          string name (b.fq_name ());

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: base class '" << name << "' is a value type"
               << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: object types cannot derive from value types" << endl;

          cerr << b.file () << ":" << b.line () << ":" << b.column () << ":"
               << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      {
        id_member t (true, valid_, id);
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
          id->set ("not-null", string ());
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
    traverse_value (type& c)
    {
      bool base (false);

      for (type::inherits_iterator i (c.inherits_begin ());
           i != c.inherits_end ();
           ++i)
      {
        type& b (i->base ());

        if (context::comp_value (b))
          base = true;
        else if (b.count ("object"))
        {
          // @@ Should we use hint here?
          //
          string name (b.fq_name ());

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: base class '" << name << "' is an object type"
               << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: composite value types cannot derive from object "
               << "types" << endl;

          cerr << b.file () << ":" << b.line () << ":" << b.column () << ":"
               << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      {
        id_member t (false, valid_, id);
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
    semantics::unit& unit_;
    value_type& vt_;

    data_member member_;
    traversal::names names_;
  };
}

bool validator::
validate (options const&,
          semantics::unit& u,
          semantics::path const&)
{
  bool valid (true);

  traversal::unit unit;
  traversal::defines unit_defines;
  traversal::declares unit_declares;
  traversal::namespace_ ns;
  value_type vt (valid);
  class_ c (valid, u, vt);

  unit >> unit_defines >> ns;
  unit_defines >> c;
  unit >> unit_declares >> vt;

  traversal::defines ns_defines;
  traversal::declares ns_declares;

  ns >> ns_defines >> ns;
  ns_defines >> c;
  ns >> ns_declares >> vt;

  unit.dispatch (u);

  return valid;
}

validator::
validator ()
{
}
